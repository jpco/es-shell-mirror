/* var.c -- es variables ($Revision: 1.1.1.1 $) */

#include "es.h"
#include "gc.h"
#include "var.h"
#include "term.h"


Dict *vars;
static Boolean rebound = TRUE;

DefineTag(Var, static);

static Boolean specialvar(const char *name) {
    return (*name == '*' || *name == '0') && name[1] == '\0';
}

static Boolean hasbindings(List *list) {
    for (; list != NULL; list = list->next)
        if (isclosure(list->term)) {
            Closure *closure = getclosure(list->term);
            assert(closure != NULL);
            if (closure->binding != NULL)
                return TRUE;
        }
    return FALSE;
}

static Var *mkvar(List *defn) {
    Ref(Var *, var, NULL);
    Ref(List *, lp, defn);
    var = gcnew(Var);
    var->defn = lp;
    var->flags = hasbindings(lp) ? var_hasbindings : 0;
    RefEnd(lp);
    RefReturn(var);
}

static void *VarCopy(void *op) {
    void *np = gcnew(Var);
    memcpy(np, op, sizeof (Var));
    return np;
}

static size_t VarScan(void *p) {
    Var *var = p;
    var->defn = forward(var->defn);
    return sizeof (Var);
}

/* iscounting -- is it a counter number, i.e., an integer > 0 */
static Boolean iscounting(const char *name) {
    int c;
    const char *s = name;
    while ((c = *s++) != '\0')
        if (!isdigit(c))
            return FALSE;
    if (streq(name, "0"))
        return FALSE;
    return name[0] != '\0';
}


/*
 * public entry points
 */

/* validatevar -- ensure that a variable name is valid */
extern void validatevar(const char *var) {
    if (*var == '\0')
        fail("es:var", "zero-length variable name");
    if (iscounting(var))
        fail("es:var", "illegal variable name: %S", var);
}

/* varlookup -- lookup a variable in the current context */
extern List *varlookup(const char *name, Binding *bp) {
    Var *var;

    validatevar(name);
    for (; bp != NULL; bp = bp->next)
        if (streq(name, bp->name))
            return bp->defn;

    var = dictget(vars, name);
    if (var == NULL)
        return NULL;
    return var->defn;
}

extern List *varlookup2(char *name1, char *name2, Binding *bp) {
    Var *var;
    
    for (; bp != NULL; bp = bp->next)
        if (streq2(bp->name, name1, name2))
            return bp->defn;

    var = dictget2(vars, name1, name2);
    if (var == NULL)
        return NULL;
    return var->defn;
}

static List *callsettor(char *name, List *defn) {
    Push p;
    List *settor;

    if (specialvar(name) || (settor = varlookup2("set-", name, NULL)) == NULL)
        return defn;

    Ref(List *, lp, defn);
    Ref(List *, fn, settor);
    varpush(&p, "0", mklist(mkstr(name), NULL));

    lp = listcopy(eval(append(fn, lp), NULL));

    varpop(&p);
    RefEnd(fn);
    RefReturn(lp);
}

extern void vardef(char *name, Binding *binding, List *defn) {
    Var *var;

    validatevar(name);
    for (; binding != NULL; binding = binding->next)
        if (streq(name, binding->name)) {
            binding->defn = defn;
            rebound = TRUE;
            return;
        }

    RefAdd(name);
    defn = callsettor(name, defn);

    var = dictget(vars, name);
    if (var != NULL)
        if (defn != NULL) {
            var->defn = defn;
            var->env = NULL;
            var->flags = hasbindings(defn) ? var_hasbindings : 0;
        } else
            vars = dictput(vars, name, NULL);
    else if (defn != NULL) {
        var = mkvar(defn);
        vars = dictput(vars, name, var);
    }
    RefRemove(name);
}

extern void varpush(Push *push, char *name, List *defn) {
    Var *var;

    validatevar(name);
    push->name = name;
    push->nameroot.next = rootlist;
    push->nameroot.p = (void **) &push->name;
    rootlist = &push->nameroot;

    defn = callsettor(name, defn);

    var = dictget(vars, push->name);
    if (var == NULL) {
        push->defn  = NULL;
        push->flags = 0;
        var     = mkvar(defn);
        vars        = dictput(vars, push->name, var);
    } else {
        push->defn  = var->defn;
        push->flags = var->flags;
        var->defn   = defn;
        var->env    = NULL;
        var->flags  = hasbindings(defn) ? var_hasbindings : 0;
    }

    push->next = pushlist;
    pushlist = push;

    push->defnroot.next = rootlist;
    push->defnroot.p = (void **) &push->defn;
    rootlist = &push->defnroot;
}

extern void varpop(Push *push) {
    Var *var;
    
    assert(pushlist == push);
    assert(rootlist == &push->defnroot);
    assert(rootlist->next == &push->nameroot);

    push->defn = callsettor(push->name, push->defn);
    var = dictget(vars, push->name);

    if (var != NULL)
        if (push->defn != NULL) {
            var->defn = push->defn;
            var->flags = push->flags;
            var->env = NULL;
        } else
            vars = dictput(vars, push->name, NULL);
    else if (push->defn != NULL) {
        var = mkvar(NULL);
        var->defn = push->defn;
        var->flags = push->flags;
        vars = dictput(vars, push->name, var);
    }

    pushlist = pushlist->next;
    rootlist = rootlist->next->next;
}

/* addtolist -- dictforall procedure to create a list */
extern void addtolist(void *arg, char *key, void *value) {
    List **listp = arg;
    Term *term = mkstr(key);
    *listp = mklist(term, *listp);
}

static void listexternal(void *arg, char *key, void *value) {
    if ((((Var *) value)->flags & var_isinternal) == 0 && !specialvar(key))
        addtolist(arg, key, value);
}

static void listinternal(void *arg, char *key, void *value) {
    if (((Var *) value)->flags & var_isinternal)
        addtolist(arg, key, value);
}

/* listvars -- return a list of all the (dynamic) variables */
extern List *listvars(Boolean internal) {
    Ref(List *, varlist, NULL);
    dictforall(vars, internal ? listinternal : listexternal, &varlist);
    varlist = sortlist(varlist);
    RefReturn(varlist);
}

/* hide -- worker function for dictforall to hide initial state */
static void hide(void *dummy, char *key, void *value) {
    ((Var *) value)->flags |= var_isinternal;
}

/* hidevariables -- mark all variables as internal */
extern void hidevariables(void) {
    dictforall(vars, hide, NULL);
}

/* initvars -- initialize the variable machinery */
extern void initvars(void) {
    globalroot(&vars);
    vars = mkdict();
}
