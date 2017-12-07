/* parse.y -- grammar for es ($Revision: 1.2 $) */

%{
/* Some yaccs insist on including stdlib.h */
#include "es.h"
#include "input.h"
#include "syntax.h"
%}

%token  WORD QWORD
%token  LOCAL LET FOR CLOSURE FN
%token  ANDAND BACKBACK EXTRACT CALL COUNT DUP FLAT OROR PRIM REDIR SUB
%token  NL ENDFILE ERROR
%token  INT FLOAT ARITH_BEGIN ARITH_VAR
%token  LEQ GEQ NEQ LESS GREATER EQ

%left   LOCAL LET FOR CLOSURE ')'
%left   ANDAND OROR NL
%left   '!'
%left   PIPE
%left   PASS
%right  VWORD
%left   SUB
%right  '$'

%nonassoc   LESS GREATER LEQ GEQ EQ
%left       '+' '-'
%left       '*' '/' '%'
%nonassoc   NEG

%union {
        Tree *tree;
        char *str;
        NodeKind kind;
}

%type <str>     WORD QWORD keyword
                ARITH_VAR INT FLOAT
%type <tree>    REDIR PIPE DUP
                body cmd cmdsa cmdsan comword first fn line word param assign
                binding bindings params nlwords words simple redir sword vword vsword
                arith cmparith cmp
%type <kind>    binder

%start es

%%

es      : line end              { parsetree = $1; YYACCEPT; }
        | error end             { yyerrok; parsetree = NULL; YYABORT; }

end     : NL                    { unsetskip(); if (!readheredocs(FALSE)) YYABORT; }
        | ENDFILE               { unsetskip(); if (!readheredocs(TRUE)) YYABORT; }

line    : cmd                   { $$ = $1; }
        | cmdsa line            { $$ = mkseq("%seq", $1, $2); }

body    : cmd                   { $$ = $1; }
        | cmdsan body           { $$ = mkseq("%seq", $1, $2); }

cmdsa   : cmd ';'               { $$ = $1; }
        | cmd '&'               { $$ = prefix("%background", mk(nList, thunkify($1), NULL)); }

cmdsan  : cmdsa                 { $$ = $1; }
        | cmd NL                { $$ = $1; if (!readheredocs(FALSE)) YYABORT; }

/* re: parsing, there are basically four kinds of 'cmd': 'simple', assignment, extraction, and
 * 'self-referential'.
 *
 *  - 'simple':           first (word|redir)*
 *  - 'assign':           first '=' (word)*
 *  - 'extraction':       '~(~?)' (word)+
 *  - 'self-referential': rules where 'cmd' shows up on the RHS
 */
cmd     :               %prec LET               { $$ = NULL; }
        | simple                                { $$ = redirect($1); if ($$ == &errornode) YYABORT; }
        | redir cmd     %prec '!'               { $$ = redirect(mk(nRedir, $1, $2)); if ($$ == &errornode) YYABORT; }
        | first assign                          { $$ = mk(nAssign, $1, $2); }
        | fn                                    { $$ = $1; }
        | binder nl '(' bindings ')' nl cmd     { $$ = mk($1, $4, $7); }
        | cmd ANDAND nl cmd                     { $$ = mkseq("%and", $1, $4); }
        | cmd OROR nl cmd                       { $$ = mkseq("%or", $1, $4); }
        | cmd PIPE nl cmd                       { $$ = mkpipe($1, $2->u[0].i, $2->u[1].i, $4); }
        | cmd PASS nl cmd                       { $$ = mkpass($1, $4); }
        | '!' caret cmd                         { $$ = prefix("%not", mk(nList, thunkify($3), NULL)); }
        | '~' word words                        { $$ = mk(nMatch, $2, $3); }
        | EXTRACT word words                    { $$ = mk(nExtract, $2, $3); }

simple  : first                         { $$ = treecons2($1, NULL); }
        | simple word                   { $$ = treeconsend2($1, $2); }
        | simple redir                  { $$ = redirappend($1, $2); }

redir   : DUP                           { $$ = $1; }
        | REDIR word                    { $$ = mkredir($1, $2); }

bindings: binding                       { $$ = treecons2($1, NULL); }
        | bindings ';' binding          { $$ = treeconsend2($1, $3); }
        | bindings NL binding           { $$ = treeconsend2($1, $3); }

binding :                               { $$ = NULL; }
        | fn                            { $$ = $1; }
        | word assign                   { $$ = mk(nAssign, $1, $2); }
        | word                          { $$ = mk(nAssign, $1, NULL); }

assign  : caret '=' caret words         { $$ = $4; }

fn      : FN word params '{' body '}'   { $$ = fnassign($2, mklambda($3, $5)); }
        | FN word                       { $$ = fnassign($2, NULL); }

/*
 * 'first' is 'comword(^sword)*'.  Specifically, like 'word'
 *  but cannot start with a keyword.
 */
first   : comword                       { $$ = $1; }
        | first '^' sword               { $$ = mk(nConcat, $1, $3); }

/* 'sword' is a single word; i.e., a unit which can be combined with '^'. */
sword   : comword                       { $$ = $1; }
        | keyword                       { $$ = mk(nWord, $1); }

/* 'word' is 'sword(^sword)*'.  A (one-or-more) list of carat-separated 'sword's. */
word    : sword                         { $$ = $1; }
        | word '^' sword                { $$ = mk(nConcat, $1, $3); }

/* a 'vsword' is an sword inside of a variable. */
vsword  : vword                         { $$ = $1; }
        | keyword                       { $$ = mk(nWord, $1); }

/* a 'vword' is a word which can appear in a variable */
vword   : param                         { $$ = $1; }
        | '(' nlwords ')'               { $$ = $2; }
        | '{' body '}'                  { $$ = thunkify($2); }
        | '@' params '{' body '}'       { $$ = mklambda($2, $4); }
        | '$' vsword      %prec VWORD   { $$ = mk(nVar, $2); }
        | ARITH_BEGIN cmparith ')'      { $$ = mk(nArith, $2); }
        | CALL sword                    { $$ = mk(nCall, $2); }
        | COUNT sword                   { $$ = mk(nCall, prefix("%count", treecons(mk(nVar, $2), NULL))); }
        | FLAT sword                    { $$ = flatten(mk(nVar, $2), " "); }
        | PRIM WORD                     { $$ = mk(nPrim, $2); }
        | '`' vsword                    { $$ = backquote(mk(nVar, mk(nWord, "ifs")), $2); }
        | BACKBACK word sword           { $$ = backquote($2, $3); }

/* a 'comword' is a usual command word */
comword : vword                         { $$ = $1; }
        | '$' vsword SUB words ')'      { $$ = mk(nVarsub, $2, $4); }

/* a 'param' is essentially a 'unit non-special word'. */
param   : WORD                          { $$ = mk(nWord, $1); }
        | QWORD                         { $$ = mk(nQword, $1); }

/* a zero-or-more space-separated list of 'param's. */
params  :                               { $$ = NULL; }
        | params param                  { $$ = treeconsend($1, $2); }

/* 'words' is a zero-or-more space-separated list of 'word's. */
words   :                               { $$ = NULL; }
        | words word                    { $$ = treeconsend($1, $2); }

/* a zero-or-more space-or-\n-separated list of 'word's. */
nlwords :                               { $$ = NULL; }
        | nlwords word                  { $$ = treeconsend($1, $2); }
        | nlwords NL                    { $$ = $1; }

/* zero-or-more newlines. */
nl      :                               { unsetskip(); }
        | nl NL                         { unsetskip(); }

/* zero-or-one '^' characters. */
caret   :
        | '^'

binder  : LOCAL         { $$ = nLocal; }
        | LET           { $$ = nLet; }
        | FOR           { $$ = nFor; }
        | CLOSURE       { $$ = nClosure; }

keyword : '!'           { $$ = "!"; }
        | '~'           { $$ = "~"; }
        | EXTRACT       { $$ = "~~"; }
        | LOCAL         { $$ = "local"; }
        | LET           { $$ = "let"; }
        | FOR           { $$ = "for"; }
        | FN            { $$ = "fn"; }
        | CLOSURE       { $$ = "%closure"; }

arith   : arith '-' arith      { $$ = mkop("-", $1, $3); }
        | arith '+' arith      { $$ = mkop("+", $1, $3); }
        | arith '*' arith      { $$ = mkop("*", $1, $3); }
        | arith '/' arith      { $$ = mkop("/", $1, $3); }
        | arith '%' arith      { $$ = mkop("%", $1, $3); }
        | '-' arith  %prec NEG { $$ = mkop("-", mk(nInt, "0"), $2); }
        | '(' arith ')'        { $$ = $2; }
        | ARITH_VAR            { $$ = mk(nVar, mk(nWord, $1)); }
        | INT                  { $$ = mk(nInt, $1); }
        | FLOAT                { $$ = mk(nFloat, $1); }

cmp : arith LESS arith      { $$ = mkcmp("<", $1, $3); }
    | arith GREATER arith   { $$ = mkcmp(">", $1, $3); }
    | arith EQ arith        { $$ = mkcmp("=", $1, $3); }
    | arith LEQ arith       { $$ = mkcmp("<=", $1, $3); }
    | arith GEQ arith       { $$ = mkcmp(">=", $1, $3); }
    | arith NEQ arith       { $$ = mkcmp("!=", $1, $3); }

cmparith    : arith                 { $$ = $1; }
            | cmp                   { $$ = $1; }

/*
arithlogic  : cmp ANDAND cmp    { $$ = ???; }
            | cmp OROR cmp      { $$ = ???; }
*/
