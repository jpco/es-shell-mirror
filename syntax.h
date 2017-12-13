/* syntax.h -- abstract syntax tree interface ($Revision: 1.1.1.1 $) */

#define	CAR	u[0].p
#define	CDR	u[1].p


/* syntax.c */

extern Tree errornode;

extern Tree *treecons(Tree *car, Tree *cdr);
extern Tree *treecons2(Tree *car, Tree *cdr);
extern Tree *treeconsend(Tree *p, Tree *q);
extern Tree *treeconsend2(Tree *p, Tree *q);
extern Tree *treeappend(Tree *head, Tree *tail);

extern Tree *prefix(char *s, Tree *t);
extern Tree *mklambda(Tree *params, Tree *body);
extern Tree *mkseq(char *op, Tree *t1, Tree *t2);


/* heredoc.c */

extern Boolean readheredocs(Boolean endfile);
extern Boolean queueheredoc(Tree *t);
