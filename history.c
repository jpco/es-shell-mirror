/* history.c -- control the history file ($Revision: 1.2 $) */
/* stdgetenv is based on the FreeBSD getenv */

#include "es.h"
#include "input.h"


/*
 * constants
 */

#define BUFSIZE     ((size_t) 4096)     /* buffer size to fill reads into */


/*
 * globals
 */

#if READLINE
extern void add_history(char *);
extern int read_history(const char *);
extern int append_history(int, const char *);

static char *history;
#endif

static char *histbuf, *histbufbegin = NULL;
static long histbuflen;

extern Boolean pendinghistory() {
    return histbufbegin != NULL;
}

extern void addhistory(char *line, long len) {
    if (line == NULL || len == 0)
        return;

    if (histbufbegin == NULL) {
        histbufbegin = ealloc(BUFSIZE);
        histbuf = histbufbegin;
        histbuflen = BUFSIZE;
    }

    ptrdiff_t bufpos = histbuf - histbufbegin;
    if (histbuflen < (1 + len + bufpos)) {
        while (histbuflen < (1 + len + bufpos))
            histbuflen *= 2;
        histbufbegin = erealloc(histbufbegin, histbuflen);
        histbuf = histbufbegin + bufpos;
    }
    memcpy(histbuf, line, len);
    histbuf += len;
}

extern char *gethistory() {
    if (histbufbegin == NULL)
        return NULL;

    *histbuf = '\0';
    if (histbuf > histbufbegin && histbuf[-1] == '\n')
        histbuf[-1] = '\0';

    char *res = histbufbegin;
    histbufbegin = NULL;

    char *s, *end;
    long len = strlen(res);

    /* if this command is only whitespace and comments, return NULL */
    for (s = res, end = s + len; s < end; s++)
        switch (*s) {
        case '#': case '\n':    goto retnull;
        case ' ': case '\t':    break;
        default:                goto retreal;
        }
retnull:
    efree(res);
    return NULL;

retreal:
    return res;
}

#if READLINE
/* sethistory -- change the file for the history log */
extern void sethistory(char *file) {
    if (file != NULL) {
        int err;
        if ((err = read_history(file))) {
            eprint("sethistory(%s): %s (%d)\n", file, esstrerror(err), err);
            vardef("history", NULL, NULL);
            return;
        }
    }
    history = file;
}

extern void writehistory(char *cmd) {
    if (cmd == NULL)
        return;

    int err;
    add_history(cmd);
    if (history != NULL && (err = append_history(1, history))) {
        eprint("history(%s): %s\n", history, esstrerror(err));
        vardef("history", NULL, NULL);
    }
}

extern void inithistory() {
    globalroot(&history);   /* history file */
}
#endif
