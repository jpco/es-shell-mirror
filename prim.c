/* prim.c -- primitives and primitive dispatching ($Revision: 1.1.1.1 $) */

#include "es.h"
#include "prim.h"

static Dict *prims;

extern List *prim(char *s, List *list, Binding *binding) {
    List *(*p)(List *, Binding *);
    p = (List *(*)(List *, Binding *)) dictget(prims, s);
    if (p == NULL)
        fail("es:prim", "unknown primitive: %s", s);
    return (*p)(list, binding);
}

PRIM(primitives) {
    static List *primlist = NULL;
    if (primlist == NULL) {
        globalroot(&primlist);
        dictforall(prims, addtolist, &primlist);
        primlist = sortlist(primlist);
    }
    return primlist;
}

extern void initprims(void) {
    prims = mkdict();
    globalroot(&prims);

    prims = initprims_controlflow(prims);
    prims = initprims_io(prims);
    prims = initprims_etc(prims);

#define primdict prims
    X(primitives);
}
