/* conv.c -- convert between internal and external forms ($Revision: 1.1.1.1 $) */

#include "es.h"
#include "print.h"
#include "term.h"

/* %L -- print a list
 *
 * NB: This function consumes two arguments -- the list (List *), and the
 * separator (char *) */
static Boolean Lconv(Format *f) {
    List *lp, *next;
    char *sep;
    const char *fmt = (f->flags & FMT_altform) ? "%S%s" : "%s%s";

    lp = va_arg(f->args, List *);
    sep = va_arg(f->args, char *);
    for (; lp != NULL; lp = next) {
        next = lp->next;
        fmtprint(f, fmt, getstr(lp->term), next == NULL ? "" : sep);
    }
    return FALSE;
}

/* treecount -- count the number of nodes in a flattened tree list */
static int treecount(Tree *tree) {
    return tree == NULL
         ? 0
         : tree->kind == nList
            ? treecount(tree->u[0].p) + treecount(tree->u[1].p)
            : 1;
}

/* binding -- print a binding statement */
static void binding(Format *f, Tree *tree) {
    Tree *np;
    char *sep = "";
    fmtprint(f, "(");
    for (np = tree; np != NULL; np = np->u[1].p) {
        Tree *binding;
        assert(np->kind == nList);
        binding = np->u[0].p;
        assert(binding != NULL);
        assert(binding->kind == nAssign);
        fmtprint(f, "%s%#T = %T", sep, binding->u[0].p, binding->u[1].p);
        sep = "; ";
    }
    fmtprint(f, ") ");
}

// Print the arguments of a lambda
static void printargs(Format *f, Tree *args) {
    Tree *arg;
    Tree *fbind = NULL;
    Tree *last = NULL;

    if (args == NULL)
        return;

    for (arg = args->u[0].p; args != NULL; last = args, args = args->u[1].p) {
        assert(args->kind == nList);
        arg = args->u[0].p;
        assert(arg != NULL);

        if (arg->kind != nAssign && fbind != NULL) {
            last->u[1].p = NULL;
            binding(f, fbind);
            fbind = NULL;
            last->u[1].p = args;
        }
        if (arg->kind == nAssign && fbind == NULL) {
            fbind = args;
        }
        if (arg->kind != nAssign) {
            fmtprint(f, "%#T ", arg);
        }
    }
    if (fbind != NULL) {
        binding(f, fbind);
    }
}

/* %T -- print a tree */
static Boolean Tconv(Format *f) {
    Tree *n = va_arg(f->args, Tree *);
    Boolean group = (f->flags & FMT_altform) != 0;


#define tailcall(tree, altform) \
    STMT(n = (tree); group = (altform); goto top)

top:
    if (n == NULL) {
        if (group)
            fmtcat(f, "()");
        return FALSE;
    }

    switch (n->kind) {

    case nWord:
        fmtprint(f, "%s", n->u[0].s);
        return FALSE;

    case nQword:
        fmtprint(f, "%#S", n->u[0].s);
        return FALSE;

    case nPrim:
        fmtprint(f, "$&%s", n->u[0].s);
        return FALSE;

    case nAssign:
        fmtprint(f, "%#T = ", n->u[0].p);
        tailcall(n->u[1].p, FALSE);

    case nConcat:
        fmtprint(f, "%#T^", n->u[0].p);
        tailcall(n->u[1].p, TRUE);

    case nMatch:
        fmtprint(f, "~ %#T", n->u[0].p);
        if (treecount(n->u[1].p) > 0) {
            fmtprint(f, " ");
            tailcall(n->u[1].p, FALSE);
        } else
            return FALSE;

    case nExtract:
        fmtprint(f, "~~ %#T", n->u[0].p);
        if (treecount(n->u[1].p) > 0) {
            fmtprint(f, " ");
            tailcall(n->u[1].p, FALSE);
        } else
            return FALSE;

    case nVarsub:
        fmtprint(f, "$%#T(%T)", n->u[0].p, n->u[1].p);
        return FALSE;

    case nCall: {
        Tree *t = n->u[0].p;
        fmtprint(f, "<=");
        if (t != NULL && (t->kind == nLambda || t->kind == nPrim))
            tailcall(t, FALSE);
        fmtprint(f, "{%T}", t);
        return FALSE;
    }

    case nVar:
        fmtputc(f, '$');
        n = n->u[0].p;
        if (n == NULL || n->kind == nWord || n->kind == nQword)
            goto top;
        fmtprint(f, "(%#T)", n);
        return FALSE;

    case nLambda:
        if (n->u[0].p != NULL) {
            fmtprint(f, "@ ");
            printargs(f, n->u[0].p);
        }
        fmtprint(f, "{%T}", n->u[1].p);
        return FALSE;

    case nList:
        if (!group) {
            for (; n->u[1].p != NULL; n = n->u[1].p)
                fmtprint(f, "%T ", n->u[0].p);
            n = n->u[0].p;
            goto top;
        }
        switch (treecount(n)) {
        case 0:
            fmtcat(f, "()");
            break;
        case 1:
            fmtprint(f, "%T%T", n->u[0].p, n->u[1].p);
            break;
        default:
            fmtprint(f, "(%T", n->u[0].p);
            while ((n = n->u[1].p) != NULL) {
                assert(n->kind == nList);
                fmtprint(f, " %T", n->u[0].p);
            }
            fmtputc(f, ')');
        }
        return FALSE;

    default:
        panic("conv: bad node kind: %d", n->kind);

    }

    NOTREACHED;
    return FALSE;
}

/* enclose -- build up a closure */
static void enclose(Format *f, Binding *binding, const char *sep) {
    if (binding != NULL) {
        Binding *next = binding->next;
        enclose(f, next, "; ");
        fmtprint(f, "%S = %L%s", binding->name, binding->defn, " ", sep);
    }
}

/* %C -- print a closure */
static Boolean Cconv(Format *f) {
    Closure *closure = va_arg(f->args, Closure *);
    Tree *tree = closure->tree;
    Binding *binding = closure->binding;
    Boolean altform = (f->flags & FMT_altform) != 0;

    if (altform)
        fmtprint(f, "%S", str("%C", closure));
    else {
	switch (tree->kind) {
	case nLambda: {
	    if (binding != NULL
                    || tree->u[0].p != NULL) {
		fmtprint(f, "@ ");
	    }
	    if (binding != NULL) {
		fmtprint(f, "(");
		enclose(f, binding, "");
		fmtprint(f, ") ");
	    }
	    printargs(f, tree->u[0].p);
	    fmtprint(f, "{%T}", tree->u[1].p);
	    break;
        }
	default:
	    fmtprint(f, "%T", tree);
	}
    }

    return FALSE;
}

/* %S -- print a string with conservative quoting rules */
static Boolean Sconv(Format *f) {
    int c;
    enum { Begin, Quoted, Unquoted } state = Begin;
    const unsigned char *s, *t;
    extern const char nw[];

    s = va_arg(f->args, const unsigned char *);
    if (f->flags & FMT_altform || *s == '\0')
        goto quoteit;
    for (t = s; (c = *t) != '\0'; t++)
        if (nw[c] || c == '@')
            goto quoteit;
    fmtprint(f, "%s", s);
    return FALSE;

quoteit:

    for (t = s; (c = *t); t++)
        if (!isprint(c)) {
            if (state == Quoted)
                fmtputc(f, '\'');
            if (state != Begin)
                fmtputc(f, '^');
            switch (c) {
                case '\a':  fmtprint(f, "\\a"); break;
                case '\b':  fmtprint(f, "\\b"); break;
                case '\f':  fmtprint(f, "\\f"); break;
                case '\n':  fmtprint(f, "\\n"); break;
                case '\r':  fmtprint(f, "\\r"); break;
                case '\t':  fmtprint(f, "\\t"); break;
                    case '\33': fmtprint(f, "\\e"); break;
                default:    fmtprint(f, "\\%o", c); break;
            }
            state = Unquoted;
        } else {
            if (state == Unquoted)
                fmtputc(f, '^');
            if (state != Quoted)
                fmtputc(f, '\'');
            if (c == '\'')
                fmtputc(f, '\'');
            fmtputc(f, c);
            state = Quoted;
        }

    switch (state) {
        case Begin:
        fmtprint(f, "''");
        break;
        case Quoted:
        fmtputc(f, '\'');
        break;
        case Unquoted:
        break;
    }

    return FALSE;
}

/* %F -- protect an exported name from brain-dead shells */
static Boolean Fconv(Format *f) {
    int c;
    unsigned char *name, *s;

    name = va_arg(f->args, unsigned char *);

    for (s = name; (c = *s) != '\0'; s++)
        if ((s == name ? isalpha(c) : isalnum(c))
            || (c == '_' && s[1] != '_'))
            fmtputc(f, c);
        else
            fmtprint(f, "__%02x", c);
    return FALSE;
}

/* install the conversion routines */
void initconv(void) {
    fmtinstall('C', Cconv);
    fmtinstall('F', Fconv);
    fmtinstall('L', Lconv);
    fmtinstall('S', Sconv);
    fmtinstall('T', Tconv);
}
