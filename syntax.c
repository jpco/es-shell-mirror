/* syntax.c -- abstract syntax tree re-writing rules ($Revision: 1.1.1.1 $) */

#include "es.h"
#include "input.h"
#include "syntax.h"
#include "token.h"

Tree errornode;
Tree *parsetree;

#define IS_THUNK(t)    ((t)->kind == nLambda && (t)->u[0].p == NULL)

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
        assert(p->kind == nList || p->kind == nRedir);
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
        assert(head == NULL || head->kind == nList || head->kind == nRedir);
        return head;
    }
    return treeappend(head, treecons(tail, NULL));
}

/* thunkify -- wrap a tree in thunk braces if it isn't already a thunk */
extern Tree *thunkify(Tree *tree) {
    if (tree != NULL) {
        if (IS_THUNK(tree))
            return tree;
        if (tree->CAR != NULL && tree->CDR == NULL) {
            if (IS_THUNK(tree->CAR))
                return tree->CAR;
            if (tree->CAR->kind == nList)
                if (tree->CAR->CAR != NULL && tree->CAR->CDR == NULL)
                    if (IS_THUNK(tree->CAR->CAR))
                        return tree->CAR->CAR;
        }
    }
    return mk(nLambda, NULL, tree);
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

static Boolean isredir(Tree *t) {
    char *s;

    if (t == NULL || t->kind != nList)
        return FALSE;
    t = t->CAR;
    if (t == NULL || t->kind != nWord)
        return FALSE;

    s = t->u[0].s;
    assert(s != NULL);

    if (*s++ != '%') return FALSE;

    if (streq(s, "create")) return TRUE;
    if (streq(s, "append")) return TRUE;
    if (streq(s, "here")) return TRUE;

    if (!strneq(s, "open", 4)) return FALSE;
    s += 4;

    if (*s == '\0' || streq(s, "file")) return TRUE;

    if (*s++ != '-') return FALSE;

    if (streq(s, "write")) return TRUE;
    if (streq(s, "append")) return TRUE;
    if (streq(s, "create")) return TRUE;

    return FALSE;
}

/* prefix -- prefix a tree with a given word */
extern Tree *prefix(char *s, Tree *t) {
    return treecons(mk(nWord, s), t);
}

/* flatten -- flatten the output of the glommer so we can pass the result as a single element */
extern Tree *flatten(Tree *t, char *sep) {
    return mk(nCall, prefix("%flatten", treecons(mk(nQword, sep), treecons(t, NULL))));
}

/* backquote -- create a backquote command */
extern Tree *backquote(Tree *ifs, Tree *body) {
    return mk(nCall,
          prefix("%backquote",
             treecons(flatten(ifs, ""),
                  treecons(body, NULL))));
}

/* fnassign -- translate a function definition into an assignment */
extern Tree *fnassign(Tree *name, Tree *defn) {
    return mk(nAssign, mk(nConcat, mk(nWord, "fn-"), name), defn);
}

/* mklambda -- create a lambda */
extern Tree *mklambda(Tree *params, Tree *body) {
    if (params == NULL)
        params = treecons(mk(nWord, "*"), NULL);
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

/* mkpipe -- assemble a pipe from the commands that make it up (destructive) */
extern Tree *mkpipe(Tree *t1, int outfd, int infd, Tree *t2) {
    Tree *tail;
    Boolean pipetail;

    pipetail = firstis(t2, "%pipe");
    tail = prefix(str("%d", outfd),
              prefix(str("%d", infd),
                 pipetail ? t2->CDR : treecons(thunkify(t2), NULL)));
    if (firstis(t1, "%pipe"))
        return treeappend(t1, tail);
    t1 = thunkify(t1);
    if (pipetail) {
          t2->CDR = treecons(t1, tail);
          return t2;
    }
    return prefix("%pipe", treecons(t1, tail));
}

static Tree *injectpassvar(Tree *tree) {
    Tree *nv = mk(nVar, mk(nWord, "-"));
    switch (tree->kind) {
    case nLet: case nLocal: case nClosure: case nAssign:
        tree->CDR = treeconsend2(tree->CDR, nv);
        break;
    default: {
        Tree *body = tree;
        while (isredir(body)) {
            int i;
            for (i = 0; i < 5; i++) {
                assert(body != NULL && (body->kind == nList || IS_THUNK(body)));
                // Artisinally crafted for the peculiar syntax of redirections
                if (body->kind == nList)
                    body = body->u[i < 3].p;
                else
                    body = body->u[1].p;
            }
        }
        if (IS_THUNK(body))
            body = treecons(body, treecons(nv, NULL));
        else
            body = treeconsend2(body, nv);
    }}
    return tree;
}

/* mkpass -- assemble a pass from the commands that make it up (destructive) */
extern Tree *mkpass(Tree *t1, Tree *t2) {
    Tree *tail = NULL;
    Boolean passtail = firstis(t2, "%pass");

    if (passtail) {
        tail = t2->CDR;
    } else if (t2 != NULL) {
        t2 = injectpassvar(t2);
        if (IS_THUNK(t2->CAR) && t2->CDR == NULL) {
            tail = t2;
        } else {
            tail = treecons(thunkify(t2), NULL);
        }
    }
    if (firstis(t1, "%pass"))
        return treeappend(t1, tail);

    t1 = thunkify(t1);
    if (passtail) {
        t2->CDR = treecons(t1, tail);
        return t2;
    }
    return prefix("%pass", treecons(t1, tail));
}

extern Tree *mkcmp(char *op, Tree *t1, Tree *t2) {
    return mk(nCmp, op, treecons(t1, treecons(t2, NULL)));
}

extern Tree *mkop(char *op, Tree *t1, Tree *t2) {
    if (t1->kind == nList) {
        return mk(nOp, op, treeconsend2(t1, t2));
    } else {
        if (t1->kind == nOp && *(t1->u[0].s) == *op) {
            t1->u[1].p = treeconsend(t1->u[1].p, t2);
            return t1;
        } else {
            Tree *tail = (t2->kind == nList ? t2 : treecons(t2, NULL));
            return mk(nOp, op, treecons(t1, tail));
        }
    }
}

extern Tree *mkneg(Tree *t) {
    if (t->kind == nInt) {
        es_int_t i = t->u[0].i;
        if (i < -ES_INT_MAX) {
            yyerror("integer overflow");
            return NULL;
        }
        t->u[0].i = -i;
        return t;
    }
    if (t->kind == nFloat) {
        t->u[0].f = -(t->u[0].f);
        return t;
    }
    return mkop("-", mk(nInt, 0), t);
}

/*
 * redirections -- these involve queueing up redirection in the prefix of a
 *  tree and then rewriting the tree to include the appropriate commands
 */

static Tree placeholder = { nRedir };

extern Tree *redirect(Tree *t) {
    Tree *r, *p;
    if (t == NULL)
        return NULL;
    if (t->kind != nRedir)
        return t;
    r = t->CAR;
    t = t->CDR;
    for (; r->kind == nRedir; r = r->CDR)
        t = treeappend(t, r->CAR);
    for (p = r; p->CAR != &placeholder; p = p->CDR) {
        assert(p != NULL);
        assert(p->kind == nList);
    }
    if (firstis(r, "%heredoc"))
        if (!queueheredoc(r))
            return &errornode;
    p->CAR = thunkify(redirect(t));
    return r;
}

extern Tree *mkredircmd(char *cmd, int fd) {
    return prefix(cmd, prefix(str("%d", fd), NULL));
}

extern Tree *mkredir(Tree *cmd, Tree *file) {
    Tree *word = NULL;
    if (file != NULL && IS_THUNK(file)) { /* /dev/fd operations */
        char *op;
        Tree *var;
        static int id = 0;
        if (firstis(cmd, "%open"))
            op = "%readfrom";
        else if (firstis(cmd, "%create"))
            op = "%writeto";
        else {
            yyerror("bad /dev/fd redirection");
            op = "";
        }
        var = mk(nWord, str("_devfd%d", id++));
        cmd = treecons(
            mk(nWord, op),
            treecons(var, NULL)
        );
        word = treecons(mk(nVar, var), NULL);
    } else if (!firstis(cmd, "%heredoc") && !firstis(cmd, "%here"))
        file = mk(nCall, prefix("%one", treecons(file, NULL)));
    cmd = treeappend(
        cmd,
        treecons(
            file,
            treecons(&placeholder, NULL)
        )
    );
    if (word != NULL)
        cmd = mk(nRedir, word, cmd);
    return cmd;
}

/* mkclose -- make a %close node with a placeholder */
extern Tree *mkclose(int fd) {
    return prefix("%close", prefix(str("%d", fd), treecons(&placeholder, NULL)));
}

/* mkdup -- make a %dup node with a placeholder */
extern Tree *mkdup(int fd0, int fd1) {
    return prefix("%dup",
              prefix(str("%d", fd0),
                 prefix(str("%d", fd1),
                    treecons(&placeholder, NULL))));
}

/* redirappend -- destructively add to the list of redirections, before any other nodes */
extern Tree *redirappend(Tree *tree, Tree *r) {
    Tree *t, **tp;
    for (; r->kind == nRedir; r = r->CDR)
        tree = treeappend(tree, r->CAR);
    assert(r->kind == nList);
    for (t = tree, tp = &tree; t != NULL && t->kind == nRedir; t = *(tp = &t->CDR))
        ;
    assert(t == NULL || t->kind == nList);
    *tp = mk(nRedir, r, t);
    return tree;
}
