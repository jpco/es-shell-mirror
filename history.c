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

Boolean disablehistory = FALSE;
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

    if (histbuflen < (1 + len + histbuf - histbufbegin)) {
        while (histbuflen < (1 + len + histbuf - histbufbegin))
            histbuflen *= 2;
        histbufbegin = erealloc(histbufbegin, histbuflen);
    }
    memcpy(histbuf, line, len);
    histbuf += len;
}

extern char *gethistory() {
    if (histbufbegin != NULL)
        *histbuf = '\0';
    char *res = histbufbegin;
    histbufbegin = NULL;
    return res;
}

extern void writehistory(char *cmd) {
    long len = strlen(cmd);
    if (cmd == NULL)
        return;
#if READLINE
    int err;
    if (len > 0 && cmd[len - 1] == '\n')
        cmd[len- 1] = '\0';
    // FIXME: should disablehistory prevent add_history() from being called?
    //        Do we even need disablehistory now?
    add_history(cmd);
    if (history == NULL || disablehistory)
        return;
    if ((err = append_history(1, history))) {
        eprint("history(%s): %s\n", history, esstrerror(err));
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
