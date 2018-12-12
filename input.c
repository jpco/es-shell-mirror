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

Boolean disablehistory = FALSE;
Boolean resetterminal = FALSE;
static char *history;

#if READLINE
int rl_meta_chars;  /* for editline; ignored for gnu readline */
extern char *readline(char *);
extern void add_history(char *);
extern int read_history(const char *);
extern int append_history(int, const char *);
extern void rl_reset_terminal(char *);
extern char *rl_basic_word_break_characters;
extern char *rl_completer_quote_characters;

#if ABUSED_GETENV
static char *stdgetenv(const char *);
static char *esgetenv(const char *);
static char *(*realgetenv)(const char *) = stdgetenv;
#endif
#else /* !READLINE */
static int historyfd = -1;
#endif


/*
 * errors and warnings
 */

/* locate -- identify where an error came from */
static char *locate(Input *in, char *s) {
    return (in->runflags & run_interactive)
        ? s
        : str("%s:%d: %s", in->name, in->lineno, s);
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
 * history
 */

/* loghistory -- write the last command out to a file */
static void loghistory(const char *cmd, size_t len) {
#if READLINE
    int err;
    if (history == NULL || disablehistory)
        return;
    if ((err = append_history(1, history))) {
        eprint("history(%s): %s\n", history, esstrerror(errno));
        vardef("history", NULL, NULL);
        return;
    }
#else
    const char *s, *end;
    if (history == NULL || disablehistory)
        return;
    if (historyfd == -1) {
        historyfd = eopen(history, oAppend);
        if (historyfd == -1) {
            eprint("history(%s): %s\n", history, esstrerror(errno));
            vardef("history", NULL, NULL);
            return;
        }
    }
    /* skip empty lines and comments in history */
    for (s = cmd, end = s + len; s < end; s++)
        switch (*s) {
        case '#': case '\n':    return;
        case ' ': case '\t':    break;
        default:        goto writeit;
        }
    writeit:
        ;
    /*
     * Small unix hack: since read() reads only up to a newline
     * from a terminal, then presumably this write() will write at
     * most only one input line at a time.
     */
    ewrite(historyfd, cmd, len);
#endif
}

/* sethistory -- change the file for the history log */
extern void sethistory(char *file) {
#if READLINE
    if (file != NULL) {
        int err;
        if ((err = read_history(file))) {
            eprint("sethistory(%s): %s\n", file, esstrerror(errno));
            vardef("history", NULL, NULL);
            return;
        }
    }
#else
    if (historyfd != -1) {
        close(historyfd);
        historyfd = -1;
    }
#endif
    history = file;
}

#define NRUNFLAGS 6
static struct{
    int   mask;
    char *name;
} flagarr[NRUNFLAGS] = {
    {eval_throwonfalse, "throwonfalse"},
    {run_interactive, "interactive"},
    {run_noexec, "noexec"},         // flag value only used for arg parsing now
    {run_echoinput, "echoinput"},
    {run_printcmds, "printcmds"},   // flag value only used for arg parsing now
    {run_lisptrees, "lisptrees"}
};

extern void setrunflags(int flags) {
    if (input != NULL)
        input->runflags = flags;
}

/* export_runflags -- make runflags into an es list */
extern List *export_runflags(int flags) {
    int len = 0;
    char *flagstrs[NRUNFLAGS];

    int i;
    for (i = 0; i < NRUNFLAGS; i++) {
        if (flags & flagarr[i].mask) {
            flagstrs[len++] = flagarr[i].name;
        }
    }

    return listify(len, flagstrs);
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
    } else if (in->bufbegin < in->buf && in->buf[-1] == c && (in->runflags & run_echoinput) == 0)
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

/* getverbose -- get a character, print it to standard error */
static int getverbose(Input *in) {
    if (in->fill == ungetfill)
        return get(in);
    else {
        int c = get(in);
        if (c != EOF) {
            char buf = c;
            ewrite(2, &buf, 1);
        }
        return c;
    }
}

/* eoffill -- report eof when called to fill input buffer */
static int eoffill(Input *in) {
    assert(in->fd == -1);
    return EOF;
}

#if READLINE
/* callreadline -- readline wrapper */
static char *callreadline(char *prompt) {
    char *r;
    if (prompt == NULL)
        prompt = ""; /* bug fix for readline 2.0 */
    if (resetterminal) {
        rl_reset_terminal(NULL);
        resetterminal = FALSE;
    }
    interrupted = FALSE;
    if (!setjmp(slowlabel)) {
        slow = TRUE;
        r = interrupted ? NULL : readline(prompt);
    } else
        r = NULL;
    slow = FALSE;
    if (r == NULL)
        errno = EINTR;
    SIGCHK();
    return r;
}

#if ABUSED_GETENV

/* getenv -- fake version of getenv for readline (or other libraries) */
static char *esgetenv(const char *name) {
    List *value = varlookup(name, NULL);
    if (value == NULL)
        return NULL;
    else {
        char *export;
        static Dict *envdict;
        static Boolean initialized = FALSE;
        Ref(char *, string, NULL);

        gcdisable();
        if (!initialized) {
            initialized = TRUE;
            envdict = mkdict();
            globalroot(&envdict);
        }

        string = dictget(envdict, name);
        if (string != NULL)
            efree(string);

        export = str("%W", value);
        string = ealloc(strlen(export) + 1);
        strcpy(string, export);
        envdict = dictput(envdict, (char *) name, string);

        gcenable();
        RefReturn(string);
    }
}

static char *
stdgetenv(name)
    register const char *name;
{
    extern char **environ;
    register int len;
    register const char *np;
    register char **p, *c;

    if (name == NULL || environ == NULL)
        return (NULL);
    for (np = name; *np && *np != '='; ++np)
        continue;
    len = np - name;
    for (p = environ; (c = *p) != NULL; ++p)
        if (strncmp(c, name, len) == 0 && c[len] == '=') {
            return (c + len + 1);
        }
    return (NULL);
}

char *
getenv(char *name)
{
    return realgetenv(name);
}

extern void
initgetenv(void)
{
    realgetenv = esgetenv;
}

#endif /* ABUSED_GETENV */

#endif  /* READLINE */

/* fdfill -- fill input buffer by reading from a file descriptor */
static int fdfill(Input *in) {
    long nread;
    assert(in->buf == in->bufend);
    assert(in->fd >= 0);

#if READLINE
    if (in->runflags & run_interactive && in->fd == 0) {
        char *rlinebuf = callreadline(prompt);
        if (rlinebuf == NULL)
            nread = 0;
        else {
            if (*rlinebuf != '\0')
                add_history(rlinebuf);
            nread = strlen(rlinebuf) + 1;
            if (in->buflen < (unsigned int)nread) {
                while (in->buflen < (unsigned int)nread)
                    in->buflen *= 2;
                in->bufbegin = erealloc(in->bufbegin, in->buflen);
            }
            memcpy(in->bufbegin, rlinebuf, nread - 1);
            in->bufbegin[nread - 1] = '\n';
            efree(rlinebuf);
        }
    } else
#endif
    do {
        nread = eread(in->fd, (char *) in->bufbegin, in->buflen);
        SIGCHK();
    } while (nread == -1 && errno == EINTR);

    if (nread <= 0) {
        close(in->fd);
        in->fd = -1;
        in->fill = eoffill;
        in->runflags &= ~run_interactive;
        if (nread == -1)
            fail("$&parse", "%s: %s", in->name == NULL ? "es" : in->name, esstrerror(errno));
        return EOF;
    }

    if (in->runflags & run_interactive)
        loghistory((char *) in->bufbegin, nread);

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
    emptyherequeue();

    if (ISEOF(input))
        throw(mklist(mkstr("eof"), NULL));

#if READLINE
    prompt = (pr1 == NULL) ? "" : pr1;
#else
    if (pr1 != NULL)
        eprint("%s", pr1);
#endif
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
#if LISPTREES
    if (input->runflags & run_lisptrees)
        eprint("%B\n", parsetree);
#endif
    return parsetree;
}

/* resetparser -- clear parser errors in the signal handler */
extern void resetparser(void) {
    error = NULL;
}

/* fdcleanup -- cleanup after running from a file descriptor */
static void fdcleanup(Input *in) {
    unregisterfd(&in->fd);
    if (in->fd != -1)
        close(in->fd);
    efree(in->bufbegin);
}

/* runinput -- run a command with a file as input */
extern List *runinput(char *name, List *cmd) {
    int fd = 0;

    if (name != NULL && (fd = eopen(name, oOpen)) == -1)
        fail("$&runinput", "%s: %s\n", name, esstrerror(errno));

    return runfd(fd, name, cmd);
}

/* runfd -- run a command with the given fd as input */
extern List *runfd(int fd, const char *name, List *cmd) {
    Input in;
    List *result;

    memzero(&in, sizeof (Input));
    in.lineno = 1;
    in.fill = fdfill;
    in.cleanup = fdcleanup;
    in.fd = fd;
    if (input != NULL)
        in.runflags = input->runflags;
    registerfd(&in.fd, TRUE);
    in.buflen = BUFSIZE;
    in.bufbegin = in.buf = ealloc(in.buflen);
    in.bufend = in.bufbegin;
    in.name = (name == NULL) ? "stdin" : name;

    int flags = 0;
    if (input != NULL)
        flags = input->runflags;
    result = NULL;

    flags &= ~eval_inchild;
    in.get = (flags & run_echoinput) ? getverbose : get;
    in.prev = input;
    input = &in;

    ExceptionHandler

        result = eval(cmd, NULL, flags);

    CatchException (e)

        (*input->cleanup)(input);
        input = input->prev;
        throw(e);

    EndExceptionHandler

    input = in.prev;
    (*in.cleanup)(&in);
    return result;
}


/*
 * pushing new input sources
 */

/* stringcleanup -- cleanup after running from a string */
static void stringcleanup(Input *in) {
    efree(in->bufbegin);
}

/* stringfill -- placeholder than turns into EOF right away */
static int stringfill(Input *in) {
    in->fill = eoffill;
    return EOF;
}

/* parseinput -- turn an input source into a tree */
extern Tree *parseinput(Input *in) {
    Tree * volatile result = NULL;

    in->prev = input;
    in->runflags = 0;
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

/* parsestring -- turn a string into a tree; must be exactly one tree */
extern Tree *parsestring(const char *str) {
    Input in;
    Tree *result;
    unsigned char *buf;

    assert(str != NULL);

    /* TODO: abstract out common code with runstring */

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
    globalroot(&history);       /* history file */
    globalroot(&error);     /* parse errors */
    globalroot(&prompt);        /* main prompt */
    globalroot(&prompt2);       /* secondary prompt */

#if !READLINE
    /* mark the historyfd as a file descriptor to hold back from forked children */
    registerfd(&historyfd, TRUE);
#endif

    /* call the parser's initialization */
    initparse();

#if READLINE
    rl_meta_chars = 0;
    rl_basic_word_break_characters=" \t\n\\'`$><=;|&{()}";
    rl_completer_quote_characters="'";
#endif
}
