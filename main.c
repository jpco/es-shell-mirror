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

/* initrunflags -- set $runflags for this shell */
static void initrunflags(int flags, Boolean noexec, Boolean printcmds, Boolean loginshell) {
    Ref(List *, runflags, runflags_from_int(flags));
    if (noexec)
        runflags = mklist(mkstr("noexec"), runflags);
    if (printcmds)
        runflags = mklist(mkstr("printcmds"), runflags);
    if (loginshell)
        runflags = mklist(mkstr("login"), runflags);
    vardef("runflags", NULL, runflags);
    RefEnd(runflags);
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
    volatile int ac;
    char **volatile av;

    volatile int runflags = 0;              /* -[eivL] */
    volatile Boolean protected = FALSE;     /* -p */
    volatile Boolean allowquit = FALSE;     /* -d */
    volatile Boolean cmd_stdin = FALSE;     /* -s */
    volatile Boolean loginshell = FALSE;    /* -l or $0[0] == '-' */
    Boolean printcmds = FALSE;              /* -x */
    Boolean noexec = FALSE;                 /* -n */
    Boolean keepclosed = FALSE;             /* -o */
    char *volatile cmd = NULL;              /* -c */

    initgc();
    initconv();

    if (argc == 0) {
        argc = 1;
        argv = ealloc(2 * sizeof (char *));
        argv[0] = "es";
        argv[1] = NULL;
    }
    if (*argv[0] == '-')
        loginshell = TRUE;

    while ((c = getopt(argc, argv, "eilxvnpodsc:?GIL")) != EOF)
        switch (c) {
        case 'c':   cmd = optarg;                   break;
        case 'e':   runflags |= eval_throwonfalse;  break;
        case 'i':   runflags |= run_interactive;    break;
        case 'v':   runflags |= run_echoinput;      break;
#if LISPTREES
        case 'L':   runflags |= run_lisptrees;      break;
#endif
        case 'x':   printcmds = TRUE;               break;
        case 'n':   noexec = TRUE;                  break;
        case 'l':   loginshell = TRUE;              break;
        case 'p':   protected = TRUE;               break;
        case 'o':   keepclosed = TRUE;              break;
        case 'd':   allowquit = TRUE;               break;
        case 's':   cmd_stdin = TRUE;               goto getopt_done;
#if GCVERBOSE
        case 'G':   gcverbose = TRUE;               break;
#endif
#if GCINFO
        case 'I':   gcinfo = TRUE;                  break;
#endif
        default:
            usage();
        }

getopt_done:
    if (cmd_stdin && cmd != NULL) {
        eprint("es: -s and -c are incompatible\n");
        exit(1);
    }

    if (!keepclosed) {
        checkfd(0, oOpen);
        checkfd(1, oCreate);
        checkfd(2, oCreate);
    }

    ac = argc;
    av = argv;

    ExceptionHandler
        roothandler = &_localhandler;   /* unhygienic */

        initprims();
        initvars();
        initrunflags(runflags, noexec, printcmds, loginshell);
        initinput();

        runinitial();

        initpath();
        initpid();
        initsignals(runflags & run_interactive, allowquit);
        hidevariables();
        initenv(environ, protected);

        vardef("0", NULL, mklist(mkstr(av[0]), NULL));
        Ref(List *, args, listify(ac - optind, av + optind));

        if (cmd != NULL)
            args = mklist(mkstr(cmd), args);

        args = mklist(mkstr(cmd_stdin ? "true" : "false"), args);
        args = mklist(mkstr(cmd != NULL ? "true" : "false"), args);
        args = append(varlookup("es:main", NULL), args);

        return exitstatus(eval(args, NULL, runflags));

        RefEnd(args);

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
