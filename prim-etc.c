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

PRIM(setnoexport) {
    Ref(List *, lp, list);
    setnoexport(lp);
    RefReturn(lp);
}

PRIM(version) {
    return mklist(mkstr((char *) version), NULL);
}

PRIM(exec) {
    return eval(list, NULL, evalflags | eval_inchild);
}

PRIM(dot) {
    int c, fd;
    Push zero, star;
    volatile int runflags = (evalflags & eval_inchild);
    const char * const usage = ". [-einvx] file [arg ...]";

    esoptbegin(list, "$&dot", usage);
    while ((c = esopt("einvx")) != EOF)
        switch (c) {
        case 'e':   runflags |= eval_exitonfalse;   break;
        case 'i':   runflags |= run_interactive;    break;
        case 'n':   runflags |= run_noexec;     break;
        case 'v':   runflags |= run_echoinput;  break;
        case 'x':   runflags |= run_printcmds;  break;
        }

    Ref(List *, result, NULL);
    Ref(List *, lp, esoptend());
    if (lp == NULL)
        fail("$&dot", "usage: %s", usage);

    Ref(char *, file, getstr(lp->term));
    lp = lp->next;
    fd = eopen(file, oOpen);
    if (fd == -1)
        fail("$&dot", "%s: %s", file, esstrerror(errno));

    varpush(&star, "*", lp);
    varpush(&zero, "0", mklist(mkstr(file), NULL));

    result = runfd(fd, file, runflags);

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

PRIM(exitonfalse) {
    return eval(list, NULL, evalflags | eval_exitonfalse);
}

PRIM(batchloop) {
    Ref(List *, result, true);
    Ref(List *, dispatch, NULL);

    ExceptionHandler

        for (;;) {
            List *parser, *cmd;
            parser = varlookup("fn-%parse", NULL);
            cmd = (parser == NULL)
                    ? prim("parse", NULL, NULL, 0)
                    : eval(parser, NULL, 0);
            dispatch = varlookup("fn-%dispatch", NULL);
            if (cmd != NULL) {
                if (dispatch != NULL)
                    cmd = append(dispatch, cmd);
                result = eval(cmd, NULL, evalflags);
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

PRIM(noreturn) {
    if (list == NULL)
        fail("$&noreturn", "usage: $&noreturn lambda args ...");
    Ref(List *, lp, list);
    Ref(Closure *, closure, getclosure(lp->term));
    if (closure == NULL || closure->tree->kind != nLambda)
        fail("$&noreturn", "$&noreturn: %E is not a lambda", lp->term);
    Ref(Tree *, tree, closure->tree);
    Ref(Binding *, context, bindargs(tree->u[0].p, lp->next, closure->binding));
    lp = walk(tree->u[1].p, context, evalflags);
    RefEnd3(context, tree, closure);
    RefReturn(lp);
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
    X(version);
    X(exec);
    X(dot);
    X(split);
    X(fsplit);
    X(parse);
    X(batchloop);
    X(collect);
    X(setnoexport);
    X(result);
    X(exitonfalse);
    X(noreturn);
    X(setmaxevaldepth);
    return primdict;
}
