/* syntax.c -- abstract syntax tree re-writing rules ($Revision: 1.1.1.1 $) */

#include "es.h"
#include "input.h"
#include "syntax.h"

/* FIXME: these declarations && initparse() don't belong here */
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

/* prefix -- prefix a tree with a given word */
extern Tree *prefix(char *s, Tree *t) {
    return treecons(mk(nWord, s), t);
}

/* mklambda -- create a lambda/closure */
extern Tree *mklambda(Tree *params, Tree *body) {
    /* if (body != NULL && body->kind == nList) {
	Tree *tb = body->u[0].p;
	while (tb != NULL && tb->kind == nLambda && tb->u[0].p == NULL)
	    tb = tb->u[1].p;
	body->u[0].p = tb;
    } */
    return mk(nLambda, params, body);
}
