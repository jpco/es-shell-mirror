#include "es.h"
#include "syntax.h"
#include "input.h"

static int ctoken = 0;
extern char *tokenval;

static char *parseerror = NULL;

extern Tree *parsetree;

// scaffolding functions

static int accept(int c) {
    if (ctoken == 0)
        ctoken = yylex();

    if (ctoken == c) {
        ctoken = 0;
        return 1;
    }

    return 0;
}

#define expect(token) \
    STMT( \
        if (!accept(token)) { \
            parseerror = "expected token " #token; \
            return NULL; \
        } \
    )

#define PARSE(varname, nonterminal) \
    STMT( \
        if (((varname) = CONCAT(parse_,nonterminal)()) == NULL \
                && parseerror != NULL) \
            return NULL; \
    )

#define CHECKED_PARSE(varname, nonterminal) \
    ((((varname) = CONCAT(parse_,nonterminal)()) == NULL \
            && parseerror != NULL) \
        ? (parseerror = NULL, 0) \
        : 1)

// nonterminal functions

static Tree *parse_word();
static Tree *parse_words();
static Tree *parse_sword();
static Tree *parse_lambda();

static Tree *parse_vword() {
    Tree *w = NULL;

    if (accept('(')) {
        Tree *nw;
        while (accept('\n'));
        while (CHECKED_PARSE(nw, word)) {
            w = treeconsend(w, nw);
            while (accept('\n'));
        }
        expect(')');
        return w;
    }

    if (accept('$')) {
        PARSE(w, vword);
        return mk(nVar, w);
    }

    if (accept(CALL)) {
        PARSE(w, sword);
        return mk(nCall, w);
    }

    if (accept(PRIM)) {
        expect(WORD);
        return mk(nPrim, tokenval);
    }

    if (accept(WORD))
        return mk(nWord, tokenval);

    if (accept(QWORD))
        return mk(nQword, tokenval);

    if (CHECKED_PARSE(w, lambda))
        return w;

    parseerror = "unimplemented!";
    return NULL;
}

static Tree *parse_sword() {
    if (accept('~'))
        return mk(nWord, "~");

    if (accept(EXTRACT))
        return mk(nExtract, "~~");

    Tree *v;
    if (accept('$')) {
        PARSE(v, vword);
        if (accept(SUB)) {
            Tree *sub;
            PARSE(sub, words);
            expect(')');
            return mk(nVarsub, v, sub);
        }
        return mk(nVar, v);
    }

    PARSE(v, vword);
    return v;
}

static Tree *parse_word() {
    Tree *w, *nw;
    PARSE(w, sword);
    while (accept('^')) {
        PARSE(nw, sword);
        w = mk(nConcat, w, nw);
    }
    return w;
}

static Tree *parse_words() {
    Tree *w, *accum = NULL;
    while (CHECKED_PARSE(w, word)) {
        if (accum == NULL)
            accum = treecons2(w, NULL);
        else
            accum = treeconsend2(accum, w);
    }
    return accum;
}

static Tree *parse_assign() {
    expect('=');
    Tree *ws;
    PARSE(ws, words);
    return ws;
}

static Tree *parse_cmd() {
    Tree *w, *rw;

    if (accept('~')) {
        PARSE(w, word);
        PARSE(rw, words);
        return mk(nMatch, w, rw);
    }

    if (accept(EXTRACT)) {
        PARSE(w, word);
        PARSE(rw, words);
        return mk(nExtract, w, rw);
    }

    // FIXME: improve error messages from this return
    if (!CHECKED_PARSE(w, word))
        return NULL;

    if (CHECKED_PARSE(rw, assign))
        return mk(nAssign, w, rw);

    PARSE(rw, words);
    return treecons2(w, rw);
}

static Tree *parse_bindings() {
    Tree *w, *a, *accum = NULL;
    while (accept(';') || accept('\n'));
    while (CHECKED_PARSE(w, word)) {
        PARSE(a, assign);
        if (accum == NULL)
            accum = treecons2(mk(nAssign, w, a), NULL);
        else
            accum = treeconsend2(accum, mk(nAssign, w, a));

        while (accept(';') || accept('\n'));
    }
    return accum;
}

static Tree *parse_params() {
    Tree *p = NULL, *accum;
    if (accept('(')) {
        PARSE(p, bindings);
        expect(')');
        PARSE(accum, params);
        return treeappend(p, accum);
    }

    if (accept(WORD))
        p = mk(nWord, tokenval);
    if (accept(QWORD))
        p = mk(nQword, tokenval);

    if (p == NULL)
        return NULL;

    PARSE(accum, params);
    return treecons2(p, accum);
}

static Tree *parse_lambda() {
    Tree *ps = NULL;
    if (accept('@'))
        PARSE(ps, params);

    expect('{'); while (accept('\n'));
    Tree *body;
    PARSE(body, cmd);
    while (accept('\n')); expect('}');

    return mklambda(ps, body);
}

static Tree *parse_es() {
    while (accept('\n'));
    Tree *cmd;
    PARSE(cmd, cmd);
    if (!accept('\n') && !accept(ENDFILE)) {
        // TODO: which token is unexpected?
        parseerror = "unexpected token";
        return NULL;
    }
    return cmd;
}

extern int yyparse() {
    parsetree = parse_es();

    if (parseerror != NULL)
        yyerror(parseerror);
    else
        ctoken = 0;

    return 0;
}
