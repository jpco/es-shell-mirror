/* eval.c -- evaluation of lists and trees ($Revision: 1.2 $) */

#include "es.h"

unsigned long evaldepth = 0, maxevaldepth = MAXmaxevaldepth;

/* assign -- bind a list of values to a list of variables */
static List *assign(Tree *varform, Tree *valueform0, Binding *binding0) {
    Ref(List *, result, NULL);

    Ref(Tree *, valueform, valueform0);
    Ref(Binding *, binding, binding0);
    Ref(List *, vars, glom(varform, binding));

    if (vars == NULL)
        fail("es:assign", "null variable name");

    Ref(List *, values, glom(valueform, binding));
    result = values;

    for (; vars != NULL; vars = vars->next) {
        List *value;
        Ref(char *, name, getstr(vars->term));
        if (values == NULL)
            value = NULL;
        else if (vars->next == NULL || values->next == NULL) {
            value = values;
            values = NULL;
        } else {
            value = mklist(values->term, NULL);
            values = values->next;
        }
        vardef(name, binding, value);
        RefEnd(name);
    }

    RefEnd4(values, vars, binding, valueform);
    RefReturn(result);
}

/* matchpattern -- does the text match a pattern? */
static List *matchpattern(Tree *subjectform0, Tree *patternform0,
              Binding *binding) {
    Boolean result;
    List *pattern;
    StrList *quote = NULL;
    Ref(Binding *, bp, binding);
    Ref(Tree *, patternform, patternform0);
    Ref(List *, subject, glom(subjectform0, bp));
    pattern = glom2(patternform, bp, &quote);
    result = listmatch(subject, pattern, quote);
    RefEnd3(subject, patternform, bp);
    return result ? true : false;
}

/* extractpattern -- Like matchpattern, but returns matches */
static List *extractpattern(Tree *subjectform0, Tree *patternform0,
                Binding *binding) {
    List *pattern;
    StrList *quote = NULL;
    Ref(List *, result, NULL);
    Ref(Binding *, bp, binding);
    Ref(Tree *, patternform, patternform0);
    Ref(List *, subject, glom(subjectform0, bp));
    pattern = glom2(patternform, bp, &quote);
    result = (List *) extractmatches(subject, pattern, quote);
    RefEnd3(subject, patternform, bp);
    RefReturn(result);
}

/* bindargs -- bind an argument list to the parameters of a lambda */
/* TODO: This function REALLY deserves a polish/rewrite */
extern Binding *bindargs(Tree *params0, List *args, Binding *binding0, Binding *context) {
    if (params0 == NULL)
        return binding0;

    Ref(Binding *, binding, binding0);
    Ref(Tree *, params, params0);
    Ref(Tree *, lastvar, NULL);

    for (; params != NULL; params = params->u[1].p) {
        assert(params->kind == nList);
        Ref(Tree *, param, params->u[0].p);
        Ref(List *, value, NULL);

        param = params->u[0].p;
        if (param->kind == nAssign) {
            Ref(List *, vars, glom(param->u[0].p, context));
            Ref(List *, values, glom(param->u[1].p, context));

            if (vars == NULL)
                fail("es:@", "null variable name");

            for (; vars != NULL; vars = vars->next) {
                Ref(char *, name, getstr(vars->term));
                if (values == NULL)
                    value = NULL;
                else if (vars->next == NULL || values->next == NULL) {
                    value = values;
                    values = NULL;
                } else {
                    value = mklist(values->term, NULL);
                    values = values->next;
                }
                binding = mkbinding(name, value, binding);
                RefEnd(name);
            }

            RefEnd2(values, vars);
        } else {
            assert(param->kind == nWord || param->kind == nQword);
            if (lastvar != NULL) {
                if (args == NULL)
                    value = NULL;
                else if (args->next == NULL) {
                    value = args;
                    args = NULL;
                } else {
                    value = mklist(args->term, NULL);
                    args = args->next;
                }
                binding = mkbinding(lastvar->u[0].s, value, binding);
            }
            lastvar = param;
        }
        RefEnd2(param, value);
    }
    if (lastvar != NULL) {
        binding = mkbinding(lastvar->u[0].s, args, binding);
    }

    RefEnd2(lastvar, params);
    RefReturn(binding);
}

/* walk -- walk through a tree, evaluating nodes */
extern List *walk(Tree *tree0, Binding *binding0) {
    Tree *volatile tree = tree0;
    Binding *volatile binding = binding0;

    if (tree == NULL)
        return true;

    switch (tree->kind) {

    case nLambda: case nConcat: case nList: case nQword:
    case nVar: case nWord: case nVarsub: case nCall: case nPrim: {
        List *list;
        Ref(Binding *, bp, binding);
        list = glom(tree, binding);
        binding = bp;
        RefEnd(bp);
        return eval(list, binding);
    }

    case nAssign:
        return assign(tree->u[0].p, tree->u[1].p, binding);

    case nMatch:
        return matchpattern(tree->u[0].p, tree->u[1].p, binding);

    case nExtract:
        return extractpattern(tree->u[0].p, tree->u[1].p, binding);

    default:
        panic("walk: bad node kind %d", tree->kind);

    }

    NOTREACHED;
    return NULL;
}

/* eval -- evaluate a list, producing a list */
extern List *eval(List *list0, Binding *binding0) {

    Closure *volatile cp;
    List *fn = NULL;

    if (++evaldepth >= maxevaldepth)
        fail("es:eval", "max-eval-depth exceeded");

    Ref(List *, list, list0);
    Ref(Binding *, binding, binding0);
    Ref(char *, funcname, NULL);

restart:
    if (list == NULL) {
        RefPop3(funcname, binding, list);
        --evaldepth;
        return true;
    }
    assert(list->term != NULL);

    if ((cp = getclosure(list->term)) != NULL) {
        switch (cp->tree->kind) {
        case nPrim:
            assert(cp->binding == NULL);
            list = prim(cp->tree->u[0].s, list->next, binding);
            break;
        case nLambda: {
            Ref(Tree *, tree, cp->tree);
            Ref(Binding *, context,
                        bindargs(tree->u[0].p, list->next,
                            cp->binding, cp->binding));
            list = walk(tree->u[1].p, context);
            RefEnd2(context, tree);
            break;
        }
        case nList: {
            list = glom(cp->tree, cp->binding);
            list = append(list, list->next);
            goto restart;
        }
        default:
            panic("eval: bad closure node kind %d",
                  cp->tree->kind);
        }
        goto done;
    }

    List *whatis = varlookup("shell-%whatis", NULL);
    fn = (whatis == NULL)
        ? prim("whatis", mklist(list->term, NULL), NULL)
        : eval(append(whatis, mklist(list->term, NULL)), NULL);

    if (fn == NULL)
        fail("es:whatis", "unknown command");

    list = append(fn, list->next);
    goto restart;

done:
    --evaldepth;
    RefEnd2(funcname, binding);
    RefReturn(list);
}

/* eval1 -- evaluate a term, producing a list */
extern List *eval1(Term *term) {
    return eval(mklist(term, NULL), NULL);
}
