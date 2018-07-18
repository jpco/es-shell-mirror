/* syntax.h -- abstract syntax tree interface ($Revision: 1.1.1.1 $) */

#define	CAR	u[0].p
#define	CDR	u[1].p


/* parse.c */

#define WORD 258
#define QWORD 259
#define EXTRACT 260
#define CALL 261
#define PRIM 262
#define ENDFILE 265
#define ERROR 266
#define SUB 267

char *tokenval;
extern int yyparse();


/* syntax.c */

extern Tree *treecons(Tree *car, Tree *cdr);
extern Tree *treecons2(Tree *car, Tree *cdr);
extern Tree *treeconsend(Tree *p, Tree *q);
extern Tree *treeconsend2(Tree *p, Tree *q);
extern Tree *treeappend(Tree *head, Tree *tail);

extern Tree *prefix(char *s, Tree *t);
extern Tree *mklambda(Tree *params, Tree *body);
