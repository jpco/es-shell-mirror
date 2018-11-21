/* glom.c -- walk parse tree to produce list ($Revision: 1.1.1.1 $) */

#include "es.h"
#include "gc.h"

/* concat -- cartesion cross product concatenation */
extern List *concat(List *list1, List *list2) {
    List **p, *result = NULL;

    gcdisable();
    for (p = &result; list1 != NULL; list1 = list1->next) {
        List *lp;
        for (lp = list2; lp != NULL; lp = lp->next) {
            List *np = mklist(termcat(list1->term, lp->term), NULL);
            *p = np;
            p = &np->next;
        }
    }

    Ref(List *, list, result);
    gcenable();
    RefReturn(list);
}

/* qcat -- concatenate two quote flag terms */
static char *qcat(const char *q1, const char *q2, Term *t1, Term *t2) {
    size_t len1, len2;
    char *result, *s;

    assert(gcisblocked());

    if (q1 == QUOTED && q2 == QUOTED)
        return QUOTED;
    if (q1 == UNQUOTED && q2 == UNQUOTED)
        return UNQUOTED;

    len1 = (q1 == QUOTED || q1 == UNQUOTED) ? strlen(getstr(t1)) : strlen(q1);
    len2 = (q2 == QUOTED || q2 == UNQUOTED) ? strlen(getstr(t2)) : strlen(q2);
    result = s = gcalloc(len1 + len2 + 1, &StringTag);

    if (q1 == QUOTED)
        memset(s, 'q', len1);
    else if (q1 == UNQUOTED)
        memset(s, 'r', len1);
    else
        memcpy(s, q1, len1);
    s += len1;
    if (q2 == QUOTED)
        memset(s, 'q', len2);
    else if (q2 == UNQUOTED)
        memset(s, 'r', len2);
    else
        memcpy(s, q2, len2);
    s += len2;
    *s = '\0';

    return result;
}

/* qconcat -- cartesion cross product concatenation; also produces a quote list */
static List *qconcat(List *list1, List *list2, StrList *ql1, StrList *ql2, StrList **quotep) {
    List **p, *result = NULL;
    StrList **qp;

    gcdisable();
    for (p = &result, qp = quotep; list1 != NULL; list1 = list1->next, ql1 = ql1->next) {
        List *lp;
        StrList *qlp;
        for (lp = list2, qlp = ql2; lp != NULL; lp = lp->next, qlp = qlp->next) {
            List *np;
            StrList *nq;
            np = mklist(termcat(list1->term, lp->term), NULL);
            *p = np;
            p = &np->next;
            nq = mkstrlist(qcat(ql1->str, qlp->str, list1->term, lp->term), NULL);
            *qp = nq;
            qp = &nq->next;
        }
    }

    Ref(List *, list, result);
    gcenable();
    RefReturn(list);
}

/* subscript -- variable subscripting */
static List *subscript(List *list, List *subs) {
    int lo, hi, len, counter;
    List *result, **prevp, *current;
    int rev;

    gcdisable();

    result = NULL;
    prevp = &result;
    len = length(list);
    current = list;
    counter = 1;
    rev = 0;

    if (subs != NULL && streq(getstr(subs->term), "...")) {
        lo = 1;
        goto mid_range;
    }

    while (subs != NULL) {
        lo = atoi(getstr(subs->term));
        if (lo < 1) {
            Ref(char *, bad, getstr(subs->term));
            gcenable();
            fail("es:subscript", "bad subscript: %s", bad);
            RefEnd(bad);
        }
        subs = subs->next;
        if (subs != NULL && streq(getstr(subs->term), "...")) {
        mid_range:
            subs = subs->next;
            if (subs == NULL)
                hi = len;
            else {
                hi = atoi(getstr(subs->term));
                if (hi < 1) {
                    Ref(char *, bad, getstr(subs->term));
                    gcenable();
                    fail("es:subscript", "bad subscript: %s", bad);
                    RefEnd(bad);
                }
                if (hi > len)
                    hi = len;
                subs = subs->next;
            }
        } else
            hi = lo;
        if (lo > len)
            continue;
        if (counter > lo) {
            current = list;
            counter = 1;
        }
        if (lo > hi) {rev = 1; int t = lo; lo = hi; hi = t; }
        List **spp = prevp;
        for (; counter < lo; counter++, current = current->next)
            ;
        for (; counter <= hi; counter++, current = current->next) {
            *prevp = mklist(current->term, NULL);
            prevp = &(*prevp)->next;
        }
        if (rev) {
            *spp = reverse(*spp);
            List *f = *spp;
            while (f) {
                prevp = &(f->next);
                f = f->next;
            }
        }
    }

    Ref(List *, r, result);
    gcenable();
    RefReturn(r);
}

typedef union {
    es_int_t i;
    es_float_t f;
} es_num;

static void do_op(char op, int val_t, es_num val, int *accum_t, es_num *accum) {
    int res_t = (val_t == nFloat || *accum_t == nFloat) ? nFloat : nInt;
    feclearexcept(FE_ALL_EXCEPT);

    // TODO: Any risk of overflows in these conversions?
    if (res_t == nFloat && *accum_t == nInt) {
        es_float_t v = (es_float_t) accum->i;
        accum->f = v;
        *accum_t = nFloat;
    }
    if (res_t == nFloat && val_t == nInt) {
        es_float_t v = (es_float_t) val.i;
        val.f = v;
        val_t = nFloat;
    }

    switch (op) {
    case '+':
        if (res_t == nInt) {
            if (accum->i > 0 && val.i > ES_INT_MAX - accum->i)
                fail("es:arith", "integer overflow");
            if (accum->i < 0 && val.i < ES_INT_MIN - accum->i)
                fail("es:arith", "integer overflow");
            accum->i += val.i;
        } else {
            // OVERFLOW?
            accum->f += val.f;
        }
        break;
    case '-':
        if (res_t == nInt) {
            if (accum->i > 0 && val.i < ES_INT_MIN + accum->i)
                fail("es:arith", "integer overflow");
            if (accum->i < 0 && val.i > ES_INT_MAX + accum->i + 1)
                fail("es:arith", "integer overflow");
            accum->i -= val.i;
        } else {
            // OVERFLOW??
            accum->f -= val.f;
        }
        break;
    case '*':
        if (res_t == nInt) {
            if (val.i > 0 &&
                    (accum->i > ES_INT_MAX / val.i ||
                     accum->i < ES_INT_MIN / val.i))
                fail("es:arith", "integer overflow");
            if (val.i < 0 &&
                    (accum->i < ES_INT_MAX / val.i ||
                     accum->i > ES_INT_MIN / val.i))
                fail("es:arith", "integer overflow");
            accum->i *= val.i;
        } else {
            // OVERFLOW???
            accum->f *= val.f;
        }
        break;
    case '/':
        if (res_t == nInt) {
            if (val.i == 0)
                fail("es:arith", "divide by zero");
            if (accum->i == ES_INT_MIN && val.i == -1)
                fail("es:arith", "integer overflow");
            accum->i /= val.i;
        } else {
            if (val.f == 0.0)
                fail("es:arith", "divide by zero");
            // UNDERFLOW????
            accum->f /= val.f;
        }
        break;
    default:
        panic("es:arith: bad operator type %c", op);
    }
    int ex = fetestexcept(FE_DIVBYZERO | FE_INVALID | FE_OVERFLOW | FE_UNDERFLOW);
    if (ex) {
        feclearexcept(FE_DIVBYZERO | FE_INVALID | FE_OVERFLOW | FE_UNDERFLOW);
        fail("es:arith", "floating point exception: %d", ex);
    }
    if (*accum_t == nFloat && accum->f != accum->f)
        fail("es:arith", "NaN");
}

// Turns an nArith tree into an int or float, or dies trying
static int arithmefy(Tree *t, es_num *ret, Binding *b) {
    switch (t->kind) {
    case nInt:
        ret->i = t->u[0].i;
        return nInt;
    case nFloat:
        ret->f = t->u[0].f;
        return nFloat;
    case nOp: {
        char op = *(t->u[0].s);
        int accum_t;
        es_num accum;
        Boolean first = TRUE;
        Ref(Tree *, cl, t->u[1].p);
        for (; cl != NULL; cl = cl->u[1].p) {
            assert(cl->kind == nList);
            Ref(Tree *, elt, cl->u[0].p);
            es_num val;
            int val_t = arithmefy(elt, &val, b);
            if (first) {
                accum = val;
                accum_t = val_t;
                first = FALSE;
            } else {
                do_op(op, val_t, val, &accum_t, &accum);
            }
            RefEnd(elt);
        }
        RefEnd(cl);
        *ret = accum;
        return accum_t;
    }
    default: {
        char *end;
        Ref(List *, lp, glom(t, b, TRUE));
        if (lp == NULL)
            fail("es:arith", "null list in arithmetic");

        if (lp->next != NULL)
            fail("es:arith", "multiple-term list in arithmetic");

        char *nstr = getstr(lp->term);
        errno = 0;

        es_int_t ival = STR_TO_EI(nstr, &end, 0);
        if (*end == '\0') {
            if (errno == ERANGE)
                fail("es:arith", "integer value too large");
            ret->i = ival;
            RefPop(lp);
            return nInt;
        }

        es_float_t fval = STR_TO_EF(nstr, &end);
        if (*end == '\0') {
            if (errno == ERANGE)
                fail("es:arith", "float value too large");
            ret->f = fval;
            RefPop(lp);
            return nFloat;
        }

        RefEnd(lp);
        fail("es:arith", "could not convert element to number");
    }}
    NOTREACHED;
    return 0;
}

/* glom1 -- glom when we don't need to produce a quote list */
static List *glom1(Tree *tree, Binding *binding) {
    Ref(List *, result, NULL);
    Ref(List *, tail, NULL);
    Ref(Tree *, tp, tree);
    Ref(Binding *, bp, binding);

    assert(!gcisblocked());

    while (tp != NULL) {
        Ref(List *, list, NULL);

        switch (tp->kind) {
        case nQword:
            list = mklist(mkterm(tp->u[0].s, NULL), NULL);
            tp = NULL;
            break;
        case nWord:
            list = mklist(mkterm(tp->u[0].s, NULL), NULL);
            tp = NULL;
            break;
        case nInt:
            list = mklist(mkstr(str("%ld", tp->u[0].i)), NULL);
            tp = NULL;
            break;
        case nFloat:
            list = mklist(mkstr(str("%f", tp->u[0].f)), NULL);
            tp = NULL;
            break;
        case nArith: {
            es_num ret;
            switch (arithmefy(tp->u[0].p, &ret, bp)) {
            case nInt:
                list = mklist(mkstr(str("%ld", ret.i)), NULL);
                break;
            case nFloat:
                list = mklist(mkstr(str("%f", ret.f)), NULL);
                break;
            default:
                NOTREACHED;
            }
            tp = NULL;
            break;
        }
        case nThunk:
        case nLambda:
            list = mklist(mkterm(NULL, mkclosure(tp, bp)), NULL);
            tp = NULL;
            break;
        case nPrim:
            list = mklist(mkterm(NULL, mkclosure(tp, NULL)), NULL);
            tp = NULL;
            break;
        case nVar:
            Ref(List *, var, glom1(tp->u[0].p, bp));
            tp = NULL;
            for (; var != NULL; var = var->next) {
                list = listcopy(varlookup(getstr(var->term), bp));
                if (list != NULL) {
                    if (result == NULL)
                        tail = result = list;
                    else
                        tail->next = list;
                    for (; tail->next != NULL; tail = tail->next)
                        ;
                }
                list = NULL;
            }
            RefEnd(var);
            break;
        case nVarsub:
            list = glom1(tp->u[0].p, bp);
            if (list == NULL)
                fail("es:glom", "null variable name in subscript");
            if (list->next != NULL)
                fail("es:glom", "multi-word variable name in subscript");
            Ref(char *, name, getstr(list->term));
            list = varlookup(name, bp);
            Ref(List *, sub, glom1(tp->u[1].p, bp));
            tp = NULL;
            list = subscript(list, sub);
            RefEnd2(sub, name);
            break;
        case nCall:
            list = listcopy(walk(tp->u[0].p, bp, 0));
            tp = NULL;
            break;
        case nList:
            list = glom1(tp->u[0].p, bp);
            tp = tp->u[1].p;
            break;
        case nConcat:
            Ref(List *, l, glom1(tp->u[0].p, bp));
            Ref(List *, r, glom1(tp->u[1].p, bp));
            tp = NULL;
            list = concat(l, r);
            RefEnd2(r, l);
            break;
        default:
            fail("es:glom", "glom1: bad node kind %d", tree->kind);
        }

        if (list != NULL) {
            if (result == NULL)
                tail = result = list;
            else
                tail->next = list;
            for (; tail->next != NULL; tail = tail->next)
                ;
        }
        RefEnd(list);
    }

    RefEnd3(bp, tp, tail);
    RefReturn(result);
}

/* glom2 -- glom and produce a quoting list */
extern List *glom2(Tree *tree, Binding *binding, StrList **quotep) {
    Ref(List *, result, NULL);
    Ref(List *, tail, NULL);
    Ref(StrList *, qtail, NULL);
    Ref(Tree *, tp, tree);
    Ref(Binding *, bp, binding);

    assert(!gcisblocked());
    assert(quotep != NULL);

    /*
     * this loop covers only the cases where we might produce some
     * unquoted (raw) values.  all other cases are handled in glom1
     * and we just add quoted word flags to them.
     */

    while (tp != NULL) {
        Ref(List *, list, NULL);
        Ref(StrList *, qlist, NULL);

        switch (tp->kind) {
        case nWord:
            list = mklist(mkterm(tp->u[0].s, NULL), NULL);
            qlist = mkstrlist(UNQUOTED, NULL);
            tp = NULL;
            break;
        case nList:
            list = glom2(tp->u[0].p, bp, &qlist);
            tp = tp->u[1].p;
            break;
        case nConcat:
            Ref(List *, l, NULL);
            Ref(List *, r, NULL);
            Ref(StrList *, ql, NULL);
            Ref(StrList *, qr, NULL);
            l = glom2(tp->u[0].p, bp, &ql);
            r = glom2(tp->u[1].p, bp, &qr);
            list = qconcat(l, r, ql, qr, &qlist);
            RefEnd4(qr, ql, r, l);
            tp = NULL;
            break;
        default:
            list = glom1(tp, bp);
            Ref(List *, lp, list);
            for (; lp != NULL; lp = lp->next)
                qlist = mkstrlist(QUOTED, qlist);
            RefEnd(lp);
            tp = NULL;
            break;
        }

        if (list != NULL) {
            if (result == NULL) {
                assert(*quotep == NULL);
                result = tail = list;
                *quotep = qtail = qlist;
            } else {
                assert(*quotep != NULL);
                tail->next = list;
                qtail->next = qlist;
            }
            for (; tail->next != NULL; tail = tail->next, qtail = qtail->next)
                ;
            assert(qtail->next == NULL);
        }
        RefEnd2(qlist, list);
    }

    RefEnd4(bp, tp, qtail, tail);
    RefReturn(result);
}

/* glom -- top level glom dispatching */
extern List *glom(Tree *tree, Binding *binding, Boolean globit) {
    if (globit) {
        Ref(List *, list, NULL);
        Ref(StrList *, quote, NULL);
        list = glom2(tree, binding, &quote);
        list = glob(list, quote);
        RefEnd(quote);
        RefReturn(list);
    } else
        return glom1(tree, binding);
}
