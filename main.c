/* main.c -- initialization for es ($Revision: 1.3 $) */

#include "es.h"

#if GCVERBOSE
Boolean gcverbose   = FALSE;    /* -G */
#endif
#if GCINFO
Boolean gcinfo      = FALSE;    /* -I */
#endif

/* #if 0 && !HPUX && !defined(linux) && !defined(sgi) */
/* extern int getopt (int argc, char **argv, const char *optstring); */
/* #endif */

extern int optind;
extern char *optarg;

/* extern int isatty(int fd); */
extern char **environ;


/* checkfd -- open /dev/null on an fd if it is closed */
static void checkfd(int fd, OpenKind r) {
    int new;
    new = dup(fd);
    if (new != -1)
        close(new);
    else if (errno == EBADF && (new = eopen("/dev/null", r)) != -1)
        mvfd(new, fd);
}

/* initpath -- set $path based on the configuration default */
static void initpath(void) {
    int i;
    static const char * const path[] = { INITIAL_PATH };

    Ref(List *, list, NULL);
    for (i = arraysize(path); i-- > 0;) {
        Term *t = mkstr((char *) path[i]);
        list = mklist(t, list);
    }
    vardef("path", NULL, list);
    RefEnd(list);
}

/* initpid -- set $pid for this shell */
static void initpid(void) {
    vardef("pid", NULL, mklist(mkstr(str("%d", getpid())), NULL));
}

/* usage -- print usage message and die */
static noreturn usage(void) {
    eprint(
        "usage: es [-c command] [-silevxnpo] [file [args ...]]\n"
        "   -c cmd  execute argument\n"
        "   -s  read commands from standard input; stop option parsing\n"
        "   -i  interactive shell\n"
        "   -l  login shell\n"
        "   -e  exit if any command exits with false status\n"
        "   -v  print input to standard error\n"
        "   -x  print commands to standard error before executing\n"
        "   -n  just parse; don't execute\n"
        "   -p  don't load functions from the environment\n"
        "   -o  don't open stdin, stdout, and stderr if they were closed\n"
        "   -d  don't ignore SIGQUIT or SIGTERM\n"
#if GCINFO
        "   -I  print garbage collector information\n"
#endif
#if GCVERBOSE
        "   -G  print verbose garbage collector information\n"
#endif
#if LISPTREES
        "   -L  print parser results in LISP format\n"
#endif
    );
    exit(1);
}

/* main -- initialize, parse command arguments, and start running */
int main(int argc, char **argv) {
    int c;

    Boolean interactive = FALSE;            /* -i */
    volatile Boolean protected = FALSE;     /* -p */
    volatile Boolean allowquit = FALSE;     /* -d */
    Boolean keepclosed = FALSE;             /* -o */

    initgc();
    initconv();

    while ((c = getopt(argc, argv, "+eilxvnpodsc:?GIL")) != EOF)
        switch (c) {
        case 'i':   interactive = TRUE;             break;
        case 'p':   protected = TRUE;               break;
        case 'o':   keepclosed = TRUE;              break;
        case 'd':   allowquit = TRUE;               break;
#if GCVERBOSE
        case 'G':   gcverbose = TRUE;               break;
#endif
#if GCINFO
        case 'I':   gcinfo = TRUE;                  break;
#endif
        case 's': goto getopt_done;
        case 'c':  // The following cases are vestigial, while we
        case 'e':  // migrate argument parsing into es:main.
        case 'v':
#if LISPTREES
        case 'L':
#endif
        case 'x':
        case 'n':
        case 'l':
            break;
        default:
            usage();
        }

getopt_done:
    if (!keepclosed) {
        checkfd(0, oOpen);
        checkfd(1, oCreate);
        checkfd(2, oCreate);
    }

    ExceptionHandler
        roothandler = &_localhandler;   /* unhygienic */

        initprims();
        initvars();
        initinput();

        runinitial();

        initpath();
        initpid();
        initsignals(interactive, allowquit);
        hidevariables();
        initenv(environ, protected);

        Ref(List *, args, listify(argc, argv));

        Ref(List *, esmain, varlookup("es:main", NULL));
        if (esmain == NULL) {
            eprint("es:main not set\n");
            return 1;
        }

        esmain = append(esmain, args);
        return exitstatus(eval(esmain, NULL, 0));

        RefEnd2(esmain, args);

    CatchException (e)

        if (termeq(e->term, "exit"))
            return exitstatus(e->next);
        else if (termeq(e->term, "error"))
            eprint("%L\n",
                   e->next == NULL ? NULL : e->next->next,
                   " ");
        else if (!issilentsignal(e))
            eprint("uncaught exception: %L\n", e, " ");
        return 1;

    EndExceptionHandler
}
