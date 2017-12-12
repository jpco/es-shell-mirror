/* prim-etc.c -- miscellaneous primitives ($Revision: 1.2 $) */

#define REQUIRE_PWD 1

#include "es.h"
#include "prim.h"

PRIM(result) {
    return list;
}

PRIM(echo) {
    const char *eol = "\n";
    if (list != NULL) {
        if (termeq(list->term, "-n")) {
            eol = "";
            list = list->next;
        } else if (termeq(list->term, "--"))
            list = list->next;
    }
    print("%L%s", list, " ", eol);
    return true;
}

PRIM(count) {
    return mklist(mkstr(str("%d", length(list))), NULL);
}

PRIM(exec) {
    return eval(list, NULL);
}

/* FIXME: I don't like having this here, but I can't figure out es'
 * redirections well enough to go without it. */
PRIM(dot) {
    int fd;
    Push zero, star;

    Ref(List *, result, NULL);
    Ref(List *, lp, list);

    Ref(char *, file, getstr(lp->term));
    lp = lp->next;
    fd = eopen(file, oOpen);
    if (fd == -1)
        fail("$&dot", "%s: %s", file, esstrerror(errno));

    varpush(&star, "*", lp);

    result = runfd(fd);

    varpop(&zero);
    varpop(&star);
    RefEnd2(file, lp);
    RefReturn(result);
}

PRIM(split) {
    char *sep;
    if (list == NULL)
        fail("$&split", "usage: %%split separator [args ...]");
    Ref(List *, lp, list);
    sep = getstr(lp->term);
    lp = fsplit(sep, lp->next, TRUE);
    RefReturn(lp);
}

PRIM(fsplit) {
    char *sep;
    if (list == NULL)
        fail("$&fsplit", "usage: %%fsplit separator [args ...]");
    Ref(List *, lp, list);
    sep = getstr(lp->term);
    lp = fsplit(sep, lp->next, FALSE);
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
           : mklist(mkterm(NULL, mkclosure(mk(nThunk, tree), NULL)),
                NULL);
    RefEnd2(prompt2, prompt1);
    return result;
}

PRIM(inputloop) {
    Ref(List *, result, true);
    Ref(List *, dispatch, NULL);

    ExceptionHandler

        for (;;) {
            List *parser, *cmd;
            parser = varlookup("fn-%parse", NULL);
            cmd = (parser == NULL)
                    ? prim("parse", NULL, NULL)
                    : eval(parser, NULL);
            dispatch = varlookup("fn-%dispatch", NULL);
            if (cmd != NULL) {
                if (dispatch != NULL)
                    cmd = append(dispatch, cmd);
                result = eval(cmd, NULL);
            }
        }

    CatchException (e)

        if (!termeq(e->term, "eof"))
            throw(e);
        RefEnd(dispatch);
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
    X(dot);
    X(split);
    X(fsplit);
    X(parse);
    X(inputloop);
    X(collect);
    X(result);
    X(setmaxevaldepth);
    return primdict;
}
