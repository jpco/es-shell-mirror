/* prim-io.c -- input/output and redirection primitives ($Revision: 1.2 $) */

#include "es.h"
#include "gc.h"
#include "prim.h"

static const char *caller;

static int getnumber(const char *s) {
    char *end;
    int result = strtol(s, &end, 0);

    if (*end != '\0' || result < 0)
        fail(caller, "bad number: %s", s);
    return result;
}

static List *run_redir(int destfd, int srcfd, List *list, int evalflags) {
    assert(list != NULL);
    volatile int inparent = (evalflags & eval_inchild) == 0;
    volatile int ticket = UNREGISTERED;

    Ref(List *, lp, list);
    ticket = (srcfd == -1)
           ? defer_close(inparent, destfd)
           : defer_mvfd(inparent, srcfd, destfd);
    ExceptionHandler
        lp = eval(lp, NULL, evalflags);
        undefer(ticket);
    CatchException (e)
        undefer(ticket);
        throw(e);
    EndExceptionHandler

    RefReturn(lp);
}

static List *redir(List *(*rop)(int *fd, List *list), List *list, int evalflags) {
    int destfd, srcfd;

    assert(list != NULL);
    Ref(List *, lp, list);
    destfd = getnumber(getstr(lp->term));

    lp = (*rop)(&srcfd, lp->next);
    lp = run_redir(destfd, srcfd, lp, evalflags);

    RefReturn(lp);
}

#define REDIR(name) static List *CONCAT(redir_,name)(int *srcfdp, List *list)

static noreturn argcount(const char *s) {
    fail(caller, "argument count: usage: %s", s);
}

REDIR(openfile) {
    int i, fd;
    char *mode, *name;
    OpenKind kind;
    static const struct {
        const char *name;
        OpenKind kind;
    } modes[] = {
        { "r",  oOpen },
        { "w",  oCreate },
        { "a",  oAppend },
        { "r+", oReadWrite },
        { "w+", oReadCreate },
        { "a+", oReadAppend },
        { NULL, 0 }
    };

    assert(length(list) == 3);
    Ref(List *, lp, list);

    mode = getstr(lp->term);
    lp = lp->next;
    for (i = 0;; i++) {
        if (modes[i].name == NULL)
            fail("$&openfile", "bad %%openfile mode: %s", mode);
        if (streq(mode, modes[i].name)) {
            kind = modes[i].kind;
            break;
        }
    }

    name = getstr(lp->term);
    lp = lp->next;
    fd = eopen(name, kind);
    if (fd == -1)
        fail("$&openfile", "%s: %s", name, esstrerror(errno));
    *srcfdp = fd;
    RefReturn(lp);
}

PRIM(openfile) {
    List *lp;
    caller = "$&openfile";
    if (length(list) != 4)
        argcount("%openfile mode fd file cmd");
    /* transpose the first two elements */
    lp = list->next;
    list->next = lp->next;
    lp->next = list;
    return redir(redir_openfile, lp, evalflags);
}

REDIR(dup) {
    int fd;
    assert(length(list) == 2);
    Ref(List *, lp, list);
    fd = dup(fdmap(getnumber(getstr(lp->term))));
    if (fd == -1)
        fail("$&dup", "dup: %s", esstrerror(errno));
    *srcfdp = fd;
    lp = lp->next;
    RefReturn(lp);
}

PRIM(dup) {
    caller = "$&dup";
    if (length(list) != 3)
        argcount("%dup newfd oldfd cmd");
    return redir(redir_dup, list, evalflags);
}

REDIR(close) {
    *srcfdp = -1;
    return list;
}

PRIM(close) {
    caller = "$&close";
    if (length(list) != 2)
        argcount("%close fd cmd");
    return redir(redir_close, list, evalflags);
}

#define BUFSIZE 4096

PRIM(newfd) {
    if (list != NULL)
        fail("$&newfd", "usage: $&newfd");
    return mklist(mkstr(str("%d", newfd())), NULL);
}

/* read1 -- read one byte */
static int read1(int fd) {
    int nread;
    unsigned char buf;
    do {
        nread = eread(fd, (char *) &buf, 1);
    } while (nread == -1 && errno == EINTR);
    if (nread == -1)
        fail("$&read", esstrerror(errno));
    return nread == 0 ? EOF : buf;
}

PRIM(read) {
    int c;
    int fd = fdmap(0);

    static Buffer *buffer = NULL;
    if (buffer != NULL)
        freebuffer(buffer);
    buffer = openbuffer(0);

    while ((c = read1(fd)) != EOF && c != '\n' && c != '\0')
        buffer = bufputc(buffer, c);

    if (c == EOF && buffer->current == 0) {
        freebuffer(buffer);
        buffer = NULL;
        return NULL;
    } else {
        List *result = mklist(mkstr(sealcountedbuffer(buffer)), NULL);
        buffer = NULL;
        return result;
    }
}

extern Dict *initprims_io(Dict *primdict) {
    X(openfile);
    X(close);
    X(dup);
    X(newfd);       
    X(read);
    return primdict;
}
