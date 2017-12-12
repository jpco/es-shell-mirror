/* main.c -- initialization for es ($Revision: 1.3 $) */

#include "es.h"

extern int optind;
extern char *optarg;

extern char **environ;

/* main -- initialize, parse command arguments, and start running */
int main(int argc, char **argv) {
    volatile int ac;
    char **volatile av;

    initgc();
    initconv();

    if (argc == 0) {
        argc = 1;
        argv = ealloc(2 * sizeof (char *));
        argv[0] = "es";
        argv[1] = NULL;
    }

    ac = argc;
    av = argv;

    ExceptionHandler
        roothandler = &_localhandler;   /* unhygienic */

        initinput();
        initprims();
        initvars();
    
        runinitial();
    
        hidevariables();

        vardef("*", NULL, listify(ac - optind, av + optind));
        return exitstatus(runfd(0));

    CatchException (e)

        if (termeq(e->term, "exit"))
            return exitstatus(e->next);
        else if (termeq(e->term, "error"))
            eprint("%L\n",
                   e->next == NULL ? NULL : e->next->next,
                   " ");
        else
            eprint("uncaught exception: %L\n", e, " ");
        return 1;

    EndExceptionHandler
}
