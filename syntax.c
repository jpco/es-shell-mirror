/* syntax.c -- abstract syntax tree re-writing rules ($Revision: 1.1.1.1 $) */

#include "es.h"
#include "input.h"
#include "syntax.h"
#include "token.h"

Tree errornode;
Tree *parsetree;

/* initparse -- called at the dawn of time */
extern void initparse(void) {
    globalroot(&parsetree);
}

/* treecons -- create new tree list cell */
extern Tree *treecons(Tree *car, Tree *cdr) {
    assert(cdr == NULL || cdr->kind == nList);
    return mk(nList, car, cdr);
}

/* treecons2 -- create new tree list cell or do nothing if car is NULL */
extern Tree *treecons2(Tree *car, Tree *cdr) {
    assert(cdr == NULL || cdr->kind == nList);
    return car == NULL ? cdr : mk(nList, car, cdr);
}

/* treeappend -- destructive append for tree lists */
extern Tree *treeappend(Tree *head, Tree *tail) {
    Tree *p, **prevp;
    for (p = head, prevp = &head; p != NULL; p = *(prevp = &p->CDR))
        assert(p->kind == nList);
    *prevp = tail;
    return head;
}

/* treeconsend -- destructive add node at end for tree lists */
extern Tree *treeconsend(Tree *head, Tree *tail) {
    return treeappend(head, treecons(tail, NULL));
}

/* treeconsend2 -- destructive add node at end for tree lists or nothing if tail is NULL */
extern Tree *treeconsend2(Tree *head, Tree *tail) {
    if (tail == NULL) {
        assert(head == NULL || head->kind == nList);
        return head;
    }
    return treeappend(head, treecons(tail, NULL));
}

/* firstis -- check if the first word of a literal command matches a known string */
static Boolean firstis(Tree *t, const char *s) {
    if (t == NULL || t->kind != nList)
        return FALSE;
    t = t->CAR;
    if (t == NULL || t->kind != nWord)
        return FALSE;
    assert(t->u[0].s != NULL);
    return streq(t->u[0].s, s);
}

/* prefix -- prefix a tree with a given word */
extern Tree *prefix(char *s, Tree *t) {
    return treecons(mk(nWord, s), t);
}

static Tree *thunkify(Tree *body) {
    return mk(nLambda, NULL, body);
}

/* mklambda -- create a lambda/closure */
extern Tree *mklambda(Tree *params, Tree *body) {
    return mk(nLambda, params, body);
}

/* mkseq -- destructively add to a sequence of nList/nThink operations */
extern Tree *mkseq(char *op, Tree *t1, Tree *t2) {
    Tree *tail;
    Boolean sametail;

    if (streq(op, "%seq")) {
        if (t1 == NULL)
            return t2;
        if (t2 == NULL)
            return t1;
    }
    
    sametail = firstis(t2, op);
    tail = sametail ? t2->CDR : treecons(thunkify(t2), NULL);
    if (firstis(t1, op))
        return treeappend(t1, tail);
    t1 = thunkify(t1);
    if (sametail) {
          t2->CDR = treecons(t1, tail);
          return t2;
    }
    return prefix(op, treecons(t1, tail));
}
