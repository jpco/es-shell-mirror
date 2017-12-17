/* token.c -- lexical analyzer for es ($Revision: 1.1.1.1 $) */

#include "es.h"
#include "input.h"
#include "syntax.h"
#include "token.h"

#define isodigit(c) ('0' <= (c) && (c) < '8')

#define BUFSIZE ((size_t) 2048)
#define BUFMAX  (8 * BUFSIZE)

typedef enum { NW, RW, KW } State;  /* "nonword", "realword", "keyword" */

static State w = NW;
static Boolean newline = FALSE;
static Boolean goterror = FALSE;
static size_t bufsize = 0;
static char *tokenbuf = NULL;

/* Non-word in default context */
const char nw[] = {
    1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0,     /*   0 -  15 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,     /*  16 -  32 */
    1, 1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,     /* ' ' - '/' */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0,     /* '0' - '?' */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,     /* '@' - 'O' */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0,     /* 'P' - '_' */
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,     /* '`' - 'o' */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0,     /* 'p' - DEL */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,     /* 128 - 143 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,     /* 144 - 159 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,     /* 160 - 175 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,     /* 176 - 191 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,     /* 192 - 207 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,     /* 208 - 223 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,     /* 224 - 239 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,     /* 240 - 255 */
};

/* "dollar non-word", i.e., invalid variable characters */
/* N.B. valid chars are % * - [0-9] [A-Z] _ [a-z]       */
const char dnw[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,     /*   0 -  15 */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,     /*  16 -  32 */
    1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1,     /* ' ' - '/' */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1,     /* '0' - '?' */
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,     /* '@' - 'O' */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0,     /* 'P' - '_' */
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,     /* '`' - 'o' */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1,     /* 'p' - DEL */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,     /* 128 - 143 */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,     /* 144 - 159 */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,     /* 160 - 175 */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,     /* 176 - 191 */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,     /* 192 - 207 */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,     /* 208 - 223 */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,     /* 224 - 239 */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,     /* 240 - 255 */
};

/* print_prompt2 -- called before all continuation lines */
extern void print_prompt2(void) {
    input->lineno++;
    if (prompt2 != NULL)
        eprint("%s", prompt2);
}

/* scanerror -- called for lexical errors */
static void scanerror(char *s) {
    int c;
    /* TODO: check previous character? rc's last hack? */
    while ((c = GETC()) != '\n' && c != EOF)
        ;
    goterror = TRUE;
    yyerror(s);
}

static inline void bufput(char **buf, int pos, char val) {
    while ((unsigned long)pos >= bufsize) {
        *buf = tokenbuf = erealloc(tokenbuf, bufsize *= 2);
    }
    (*buf)[pos] = val;
}

extern int yylex(void) {
    static Boolean dollar = FALSE;
    int c;
    size_t i;           /* The purpose of all these local assignments is to */
    const char *meta;       /* allow optimizing compilers like gcc to load these    */
    char *buf = tokenbuf;       /* values into registers. On a sparc this is a      */
    YYSTYPE *y = &yylval;       /* win, in code size *and* execution time       */

    if (goterror) {
        goterror = FALSE;
        return NL;
    }

    if (newline) {
        --input->lineno; /* slight space optimization; print_prompt2() always increments lineno */
        print_prompt2();
        newline = FALSE;
    }

    /* rc variable-names may contain only alnum, '*' and '_', so use dnw if we are scanning one. */
    meta = (dollar ? dnw : nw);
    dollar = FALSE;

top:
    while ((c = GETC()) == ' ' || c == '\t')
        w = NW;

    if (c == EOF)
        return ENDFILE;

    if (!meta[(unsigned char) c]) {    /* it's a word or keyword. */
        w = RW;
        i = 0;
        do {
            bufput(&buf, i++, c);
        } while ((c = GETC()) != EOF && !meta[(unsigned char) c]);
        UNGETC(c);
        bufput(&buf, i, '\0');
        w = KW;
        if (buf[1] == '\0') {
            int k = *buf;
            if (k == '@' || k == '~')
                return k;
        } else if (streq(buf, "~~"))
            return EXTRACT;
        w = RW;
        y->str = gcdup(buf);
        return WORD;
    }

    if (c == '$' || c == '\'')
        w = KW;

    switch (c) {
    case '$':
        dollar = TRUE;
        if ((c = GETC()) == '&')
            return PRIM;
        UNGETC(c);
        return '$';
    case '\'':
        w = RW;
        i = 0;
        while ((c = GETC()) != '\'' || (c = GETC()) == '\'') {
            bufput(&buf, i++, c);
            if (c == '\n')
                print_prompt2();
            if (c == EOF) {
                w = NW;
                scanerror("eof in quoted string");
                return ERROR;
            }
        }
        UNGETC(c);
        bufput(&buf, i, '\0');
        y->str = gcdup(buf);
        return QWORD;
    case '\\':
        if ((c = GETC()) == '\n') {
            print_prompt2();
            UNGETC(' ');
            goto top; /* Pretend it was just another space. */
        }
        if (c == EOF) {
            UNGETC(EOF);
            goto badescape;
        }
        UNGETC(c);
        c = '\\';
        w = RW;
        c = GETC();
        switch (c) {
        case 'a':   *buf = '\a';    break;
        case 'b':   *buf = '\b';    break;
        case 'e':   *buf = '\033';  break;
        case 'f':   *buf = '\f';    break;
        case 'n':   *buf = '\n';    break;
        case 'r':   *buf = '\r';    break;
        case 't':   *buf = '\t';    break;
        case 'x': case 'X': {
            int n = 0;
            for (;;) {
                c = GETC();
                if (!isxdigit(c))
                    break;
                n = (n << 4)
                  | (c - (isdigit(c) ? '0' : ((islower(c) ? 'a' : 'A') - 0xA)));
            }
            if (n == 0)
                goto badescape;
            UNGETC(c);
            *buf = n;
            break;
        }
        case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': {
            int n = 0;
            do {
                n = (n << 3) | (c - '0');
                c = GETC();
            } while (isodigit(c));
            if (n == 0)
                goto badescape;
            UNGETC(c);
            *buf = n;
            break;
        }
        default:
            if (isalnum(c)) {
            badescape:
                scanerror("bad backslash escape");
                return ERROR;
            }
            *buf = c;
            break;
        }
        buf[1] = 0;
        y->str = gcdup(buf);
        return QWORD;
    case '#':
        while ((c = GETC()) != '\n') /* skip comment until newline */
            if (c == EOF)
                return ENDFILE;
        /* FALLTHROUGH */
    case '\n':
        input->lineno++;
        newline = TRUE;
        w = NW;
        return NL;
    case '(':
        if (w == RW)    /* not keywords, so let & friends work */
            c = SUB;
        w = NW;
        return c;
    case ';':
    case '{':
    case ')':
    case '}':
    case '^':
    case '=':
        w = NW;
        return c;
    case '<':
        if ((c = GETC()) == '=')
            return CALL;
        UNGETC(c);

    default:
        assert(c != '\0');
        w = NW;
        return c; /* don't know what it is, let yacc barf on it */
    }
}

extern void inityy(void) {
    newline = FALSE;
    w = NW;
    if (bufsize > BUFMAX) {     /* return memory to the system if the buffer got too large */
        efree(tokenbuf);
        tokenbuf = NULL;
    }
    if (tokenbuf == NULL) {
        bufsize = BUFSIZE;
        tokenbuf = ealloc(bufsize);
    }
}
