/* prim-etc.c -- miscellaneous primitives ($Revision: 1.2 $) */

#include "es.h"
#include "prim.h"

PRIM(result) {
    return list;
}

PRIM(echo) {
    print("%L", list, " ");
    return true;
}

PRIM(count) {
    return mklist(mkstr(str("%d", length(list))), NULL);
}

PRIM(exec) {
    return eval(list, NULL);
}

PRIM(split) {
    char *sep;
    if (list == NULL)
        fail("$&split", "usage: %%split separator [args ...]");
    Ref(List *, lp, list);
    sep = getstr(lp->term);
    lp = split(sep, lp->next, FALSE);
    RefReturn(lp);
}

PRIM(parse) {
    List *result;
    Tree *tree;
    Ref(char *, prompt1, NULL);
    Ref(char *, prompt2, NULL);
    Ref(List *, lp, list);
    if (lp != NULL) {
        prompt1 = getstr(lp->term);
        if ((lp = lp->next) != NULL)
            prompt2 = getstr(lp->term);
    }
    RefEnd(lp);
    tree = parse(prompt1, prompt2);
    result = (tree == NULL)
           ? NULL
           : mklist(mkterm(NULL, mkclosure(mk(nLambda, NULL, tree), NULL)),
                NULL);
    RefEnd2(prompt2, prompt1);
    return result;
}

PRIM(main) {
    Ref(List *, result, true);

    ExceptionHandler

        for (;;) {
            List *cmd;
            cmd = prim("parse", NULL, NULL);
            result = eval(cmd, NULL);
        }

    CatchException (e)

        if (!termeq(e->term, "eof"))
            throw(e);
        if (result == true)
            result = true;
        RefReturn(result);

    EndExceptionHandler
}

PRIM(collect) {
    gc();
    return true;
}

PRIM(setmaxevaldepth) {
    char *s;
    long n;
    if (list == NULL) {
        maxevaldepth = MAXmaxevaldepth;
        return NULL;
    }
    if (list->next != NULL)
        fail("$&setmaxevaldepth", "usage: $&setmaxevaldepth [limit]");
    Ref(List *, lp, list);
    n = strtol(getstr(lp->term), &s, 0);
    if (n < 0 || (s != NULL && *s != '\0'))
        fail("$&setmaxevaldepth", "max-eval-depth must be set to a positive integer");
    if (n < MINmaxevaldepth)
        n = (n == 0) ? MAXmaxevaldepth : MINmaxevaldepth;
    maxevaldepth = n;
    RefReturn(lp);
}

/*
 * initialization
 */

extern Dict *initprims_etc(Dict *primdict) {
    X(echo);
    X(count);
    X(exec);
    X(split);
    X(parse);
    X(main);
    X(collect);
    X(result);
    X(setmaxevaldepth);
    return primdict;
}
