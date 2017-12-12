/* parse.y -- grammar for es ($Revision: 1.2 $) */

%{
/* Some yaccs insist on including stdlib.h */
#include "es.h"
#include "input.h"
#include "syntax.h"
%}

%token  WORD QWORD
%token  LOCAL
%token  EXTRACT CALL PRIM SUB
%token  NL ENDFILE ERROR

%left   LOCAL '%' ')'
%left   NL
%right  VWORD
%left   SUB
%right  '$'

%union {
        Tree *tree;
        char *str;
        NodeKind kind;
}

%type <str>     WORD QWORD keyword
%type <tree>    cmd comword first word param assign
                binding bindings params nlwords words simple sword vword
%type <kind>    binder

%start es

%%

/* We ought to get all these *words simplified... */

es      : cmd end               { parsetree = $1; YYACCEPT; }
        | error end             { yyerrok; parsetree = NULL; YYABORT; }

end     : NL
        | ENDFILE

cmd     :               %prec '%'               { $$ = NULL; }
        | simple                                { $$ = $1; if ($$ == &errornode) YYABORT; }
        | first assign                          { $$ = mk(nAssign, $1, $2); }
        | binder nl '(' bindings ')' nl cmd     { $$ = mk($1, $4, $7); }
        | '~' word words                        { $$ = mk(nMatch, $2, $3); }
        | EXTRACT word words                    { $$ = mk(nExtract, $2, $3); }

simple  : first                         { $$ = treecons2($1, NULL); }
        | simple word                   { $$ = treeconsend2($1, $2); }

bindings: binding                       { $$ = treecons2($1, NULL); }
        | bindings ';' binding          { $$ = treeconsend2($1, $3); }
        | bindings NL binding           { $$ = treeconsend2($1, $3); }

binding :                               { $$ = NULL; }
        | word assign                   { $$ = mk(nAssign, $1, $2); }
        | word                          { $$ = mk(nAssign, $1, NULL); }

assign  : caret '=' caret words         { $$ = $4; }

caret   :
        | '^'

first   : comword                       { $$ = $1; }
        | first '^' sword               { $$ = mk(nConcat, $1, $3); }

sword   : comword                       { $$ = $1; }
        | keyword                       { $$ = mk(nWord, $1); }

word    : sword                         { $$ = $1; }
        | word '^' sword                { $$ = mk(nConcat, $1, $3); }

vword   : param                         { $$ = $1; }
        | '(' nlwords ')'               { $$ = $2; }
        | '{' nl cmd nl '}'             { $$ = thunkify($3); }
        | '@' params '{' nl cmd nl '}'  { $$ = mklambda($2, $5); }
        | '$' vword       %prec VWORD   { $$ = mk(nVar, $2); }
        | CALL sword                    { $$ = mk(nCall, $2); }
        | PRIM WORD                     { $$ = mk(nPrim, $2); }

comword : vword                         { $$ = $1; }
        | '$' vword SUB words ')'       { $$ = mk(nVarsub, $2, $4); }

param   : WORD                          { $$ = mk(nWord, $1); }
        | QWORD                         { $$ = mk(nQword, $1); }

params  :                               { $$ = NULL; }
        | params param                  { $$ = treeconsend($1, $2); }

words   :                               { $$ = NULL; }
        | words word                    { $$ = treeconsend($1, $2); }

nlwords :                               { $$ = NULL; }
        | nlwords word                  { $$ = treeconsend($1, $2); }
        | nlwords NL                    { $$ = $1; }

nl      :
        | nl NL

binder  : LOCAL         { $$ = nLocal; }
        | '%'           { $$ = nClosure; }

keyword : '~'           { $$ = "~"; }
        | EXTRACT       { $$ = "~~"; }
        | LOCAL         { $$ = "local"; }
