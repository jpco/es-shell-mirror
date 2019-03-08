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

static char *history;

#if READLINE
extern void add_history(char *);
extern int read_history(const char *);
extern int append_history(int, const char *);
#else /* !READLINE */
static int historyfd = -1;
#endif


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


static char *histbuf, *histbufbegin = NULL;
static long histbuflen;

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
    if (histbufbegin != NULL)
        *histbuf = '\0';
    char *res = histbufbegin;
    histbufbegin = NULL;

    if (res == NULL)
        return res;

    char *s, *end;
    long len = strlen(res);

    /* if this command is only whitespace and comments, return NULL */
    for (s = res, end = s + len; s < end; s++)
        switch (*s) {
        case '#': case '\n':    goto retnull;
        case ' ': case '\t':    break;
        default:        goto retreal;
        }
retnull:
    efree(res);
    res = NULL;
retreal:
    return res;
}

extern void writehistory(char *cmd) {
    if (cmd == NULL)
        return;

    long len = strlen(cmd);
#if READLINE
    int err;
    Boolean renl = FALSE;
    if (len > 0 && cmd[len - 1] == '\n') {
        cmd[len - 1] = '\0';
        renl = TRUE;
    }
    add_history(cmd);
    if (history != NULL && (err = append_history(1, history))) {
        eprint("history(%s): %s\n", history, esstrerror(err));
        vardef("history", NULL, NULL);
    }
    if (renl)
        cmd[len - 1] = '\n';
#else
    if (history == NULL)
        return;
    if (historyfd == -1) {
        historyfd = eopen(history, oAppend);
        if (historyfd == -1) {
            eprint("history(%s): %s\n", history, esstrerror(errno));
            vardef("history", NULL, NULL);
            return;
        }
    }

    ewrite(historyfd, cmd, len);
#endif
}

extern void inithistory() {
    globalroot(&history);   /* history file */

#if !READLINE
    /* mark the historyfd as a file descriptor to hold back from forked children */
    registerfd(&historyfd, TRUE);
#endif
}
