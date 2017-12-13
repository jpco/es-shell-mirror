/* input.c -- read input from files or strings ($Revision: 1.2 $) */
/* stdgetenv is based on the FreeBSD getenv */

#include "es.h"
#include "input.h"


/*
 * constants
 */

#define BUFSIZE     ((size_t) 4096)     /* buffer size to fill reads into */


/*
 * macros
 */

#define ISEOF(in)   ((in)->fill == eoffill)


/*
 * globals
 */

Input *input;
char *prompt, *prompt2;

/*
 * errors and warnings
 */

/* locate -- identify where an error came from */
static char *locate(Input *in, char *s) {
    return str("%s:%d: %s", in->name, in->lineno, s);
}

static char *error = NULL;

/* yyerror -- yacc error entry point */
extern void yyerror(char *s) {
#if sgi
    /* this is so that trip.es works */
    if (streq(s, "Syntax error"))
        s = "syntax error";
#endif
    if (error == NULL)  /* first error is generally the most informative */
        error = locate(input, s);
}

/* warn -- print a warning */
static void warn(char *s) {
    eprint("warning: %s\n", locate(input, s));
}

/*
 * unget -- character pushback
 */

/* ungetfill -- input->fill routine for ungotten characters */
static int ungetfill(Input *in) {
    int c;
    assert(in->ungot > 0);
    c = in->unget[--in->ungot];
    if (in->ungot == 0) {
        assert(in->rfill != NULL);
        in->fill = in->rfill;
        in->rfill = NULL;
        assert(in->rbuf != NULL);
        in->buf = in->rbuf;
        in->rbuf = NULL;
    }
    return c;
}

/* unget -- push back one character */
extern void unget(Input *in, int c) {
    if (in->ungot > 0) {
        assert(in->ungot < MAXUNGET);
        in->unget[in->ungot++] = c;
    } else if (in->bufbegin < in->buf && in->buf[-1] == c)
        --in->buf;
    else {
        assert(in->rfill == NULL);
        in->rfill = in->fill;
        in->fill = ungetfill;
        assert(in->rbuf == NULL);
        in->rbuf = in->buf;
        in->buf = in->bufend;
        assert(in->ungot == 0);
        in->ungot = 1;
        in->unget[0] = c;
    }
}


/*
 * getting characters
 */

/* get -- get a character, filter out nulls */
static int get(Input *in) {
    int c;
    while ((c = (in->buf < in->bufend ? *in->buf++ : (*in->fill)(in))) == '\0')
        warn("null character ignored");
    return c;
}

/* eoffill -- report eof when called to fill input buffer */
static int eoffill(Input *in) {
    assert(in->fd == -1);
    return EOF;
}

/* fdfill -- fill input buffer by reading from a file descriptor */
static int fdfill(Input *in) {
    long nread;
    assert(in->buf == in->bufend);
    assert(in->fd >= 0);

    do {
        nread = eread(in->fd, (char *) in->bufbegin, in->buflen);
    } while (nread == -1 && errno == EINTR);

    if (nread <= 0) {
        close(in->fd);
        in->fd = -1;
        in->fill = eoffill;
        if (nread == -1)
            fail("$&parse", "%s: %s", in->name == NULL ? "es" : in->name, esstrerror(errno));
        return EOF;
    }

    in->buf = in->bufbegin;
    in->bufend = &in->buf[nread];
    return *in->buf++;
}


/*
 * the input loop
 */

/* parse -- call yyparse(), but disable garbage collection and catch errors */
extern Tree *parse(char *pr1, char *pr2) {
    int result;
    assert(error == NULL);

    inityy();

    if (ISEOF(input))
        throw(mklist(mkstr("eof"), NULL));

    if (pr1 != NULL)
        eprint("%s", pr1);

    prompt2 = pr2;

    gcreserve(300 * sizeof (Tree));
    gcdisable();
    result = yyparse();
    gcenable();

    if (result || error != NULL) {
        char *e;
        assert(error != NULL);
        e = error;
        error = NULL;
        fail("$&parse", "%s", e);
    }
    return parsetree;
}

/* resetparser -- clear parser errors in the signal handler */
extern void resetparser(void) {
    error = NULL;
}

/* runinput -- run from an input source */
extern List *runinput(Input *in) {
    List * volatile result = NULL;
    List *repl;

    in->get = get;
    in->prev = input;
    input = in;

    ExceptionHandler

        repl = varlookup("fn-%input-loop", NULL);
        result = (repl == NULL)
                ? prim("inputloop", NULL, NULL)
                : eval(repl, NULL);

    CatchException (e)

        (*input->cleanup)(input);
        input = input->prev;
        throw(e);

    EndExceptionHandler

    input = in->prev;
    (*in->cleanup)(in);
    return result;
}


/*
 * pushing new input sources
 */

/* fdcleanup -- cleanup after running from a file descriptor */
static void fdcleanup(Input *in) {
    if (in->fd != -1)
        close(in->fd);
    efree(in->bufbegin);
}

/* runfd -- run commands from a file descriptor */
extern List *runfd(int fd) {
    Input in;
    List *result;

    memzero(&in, sizeof (Input));
    in.lineno = 1;
    in.fill = fdfill;
    in.cleanup = fdcleanup;
    in.fd = fd;
    in.buflen = BUFSIZE;
    in.bufbegin = in.buf = ealloc(in.buflen);
    in.bufend = in.bufbegin;
    in.name = str("fd %d", fd);

    RefAdd(in.name);
    result = runinput(&in);
    RefRemove(in.name);

    return result;
}

/* parseinput -- turn an input source into a tree */
extern Tree *parseinput(Input *in) {
    Tree * volatile result = NULL;

    in->prev = input;
    in->get = get;
    input = in;

    ExceptionHandler
        result = parse(NULL, NULL);
        if (get(in) != EOF)
            fail("$&parse", "more than one value in term");
    CatchException (e)
        (*input->cleanup)(input);
        input = input->prev;
        throw(e);
    EndExceptionHandler

    input = in->prev;
    (*in->cleanup)(in);
    return result;
}

/* stringcleanup -- cleanup after running from a string */
static void stringcleanup(Input *in) {
    efree(in->bufbegin);
}

/* stringfill -- placeholder than turns into EOF right away */
static int stringfill(Input *in) {
    in->fill = eoffill;
    return EOF;
}

/* parsestring -- turn a string into a tree; must be exactly one tree */
extern Tree *parsestring(const char *str) {
    Input in;
    Tree *result;
    unsigned char *buf;

    assert(str != NULL);

    memzero(&in, sizeof (Input));
    in.fd = -1;
    in.lineno = 1;
    in.name = str;
    in.fill = stringfill;
    in.buflen = strlen(str);
    buf = ealloc(in.buflen + 1);
    memcpy(buf, str, in.buflen);
    in.bufbegin = in.buf = buf;
    in.bufend = in.buf + in.buflen;
    in.cleanup = stringcleanup;

    RefAdd(in.name);
    result = parseinput(&in);
    RefRemove(in.name);
    return result;
}

/*
 * initialization
 */

/* initinput -- called at dawn of time from main() */
extern void initinput(void) {
    input = NULL;

    /* declare the global roots */
    globalroot(&error);     /* parse errors */
    globalroot(&prompt);        /* main prompt */
    globalroot(&prompt2);       /* secondary prompt */

    /* call the parser's initialization */
    initparse();
}
