/* prim-etc.c -- miscellaneous primitives ($Revision: 1.2 $) */

#include "es.h"
#include "prim.h"

PRIM(result) {
    return list;
}

PRIM(echo) {
    print("%L", list);
    return true;
}

PRIM(count) {
    return mklist(mkstr(str("%d", length(list))), NULL);
}

PRIM(exec) {
    return eval(list, NULL);
}

PRIM(whatis) {
	/* the logic in here is duplicated in eval() */
	if (list == NULL || list->next != NULL)
		fail("$&whatis", "usage: $&whatis program");
	Ref(Term *, term, list->term);
	if (getclosure(term) == NULL) {
		List *fn;
		Ref(char *, prog, getstr(term));
		assert(prog != NULL);
		fn = varlookup2("fn-", prog, binding);
		if (fn != NULL)
			list = fn;
		else 
			fail("$&whatis", "unknown command %s", prog);
        RefEnd(prog);
	}
	RefEnd(term);
	return list;
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
    X(whatis);
    X(split);
    X(parse);
    X(inputloop);
    X(collect);
    X(result);
    X(setmaxevaldepth);
    return primdict;
}
