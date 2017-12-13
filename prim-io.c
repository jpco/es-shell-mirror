/* prim-io.c -- input/output and redirection primitives ($Revision: 1.2 $) */

#include "es.h"
#include "gc.h"
#include "prim.h"

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
    int fd = 0;

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
    X(read);
    return primdict;
}
