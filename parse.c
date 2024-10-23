/* Author: James A. Shackleford
 *
 * A simple shell command parser.  If you are familiar with
 * using bash, zsh, tcsh, etc. then you already understand
 * what this does.
 *
 * Parses the following syntax:
 *
 *  ~$ command_1 [< infile] [| command_n]* [> outfile] [&]
 *
 * and produces a correspondingly populated Parse structure on the heap
 *
 * Note:
 *  - Items in brackets [ ] are optional
 *  - Items in starred brackets [ ]* are optional but can be repeated
 *  - Non-bracketed items are required
 *
 * Examples of valid syntax:
 *
 *     ~$ echo "foo!!!!!!!!" > foo.txt
 *     ~$ wc -l < somefile.txt > numlines.txt
 *     ~$ ls -lh | grep 8.*K | wc -l
 *     ~$ gvim &
 **********************************************************************/
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "parse.h"


typedef struct {
    char* cmd;
    char** argv;
    char* input_fn;
    char* output_fn;
} Unit;

static char ops[] = {'>', '<', '|', '\0'};


static void trim (char* s)
{
    size_t start, end;

    if (!s)
        return;

    end = strlen (s);

    for (start=0; isspace (s[start]); ++start);

    if (s[start]) {
        while (end > 0 && isspace(s[end-1]))
            --end;

        memmove(s, &s[start], end-start);
    }
    s[end-start] = '\0';
}


static int is_background (char* cmdline)
{
    size_t last;
    int ret = 0;

    trim (cmdline);
    last = strlen (cmdline) - 1;

    if ('&' == cmdline[last]) {
        cmdline[last] = '\0';
        ret = 1;
    }

    return ret;
}


static int is_empty (char* cmdline)
{
    trim (cmdline);

    if (!cmdline || !strlen(cmdline))
        return 1;

    return 0;
}


static int is_op (char c)
{
    int i;

    for (i=0; ops[i]; i++)
        if (c == ops[i])
            return 1;

    return 0;
}


static int has_trailing (char needle, char* haystack)
{
    trim (haystack);

    if (haystack[0] == needle ||
        haystack[strlen(haystack)-1] == needle) {
        return 1;
    }

    return 0;
}


static unsigned int count_char (char needle, char* haystack)
{
    unsigned int c = 0;

    while (*haystack)
        if (*haystack++ == needle)
            c++;

    return c;
}


static unsigned int count_args (char* unit)
{
    unsigned int n = 0;

    for (; *unit; unit++) {
        if (isspace((unsigned char)(*unit))) {
            while (isspace((unsigned char)(*++unit)));
            if (*unit == '\"') {
                do {
                    unit++;
                } while (*unit != '\"');
                n++;
            } else if (*unit == '\'') {
                do {
                    unit++;
                } while (*unit != '\'');
                n++;
            } else if (!isspace((unsigned char)(*unit))) {
                n++;
            }
        }
    }

    return n;
}


static int valid_syntax (Parse* P, Unit* U, int i)
{
    if (!U)
        return 0;

    if (U->input_fn && ((i != 0) || is_empty(U->input_fn)))
        return 0;

    if (U->output_fn && ((i != P->ntasks-1) || is_empty(U->output_fn)))
        return 0;

    if (!U->cmd || !*U->cmd)
        return 0;


    return 1;
}


static char* parse_unary (char op, char* unit)
{
    char *start, *end, *arg;

    for (start=NULL; *unit; unit++)
        if (*unit == op) {
           start = ++unit;
           break;
        }

    if (!start)
        return NULL;

    for (end=start; *end; end++)
        if (is_op(*end))
           break;

    arg = strndup (start, end - start);
    arg[end - start] = '\0';
    trim (arg);

    start--;
    memset (start, ' ', end - start);

    return arg;
}


static char* argtok (char* str, char** state)
{
    char* ret;
    char seek_ch;

    if (!str)
        str = *state;

    if (!*str)
        return NULL;

    trim(str);

    seek_ch = *str == '\"' ? '\"' :
              *str == '\'' ? '\'' :
              ' ';

    ret = seek_ch == ' ' ? str : ++str;

    str = strchr (str, seek_ch);

    if (str)
        *str = '\0';

    *state = str ? ++str : (ret + strlen(ret));

    return ret;
}


static void parse_command (Unit* U, char* unit)
{
    unsigned int argc, n;
    char *str, *token, *state;

    trim (unit);
    argc = count_args (unit)+1; /* +1 for command */

    U->argv = malloc ((argc+1) * sizeof(*U->argv));
    U->argv[argc] = NULL;

    for (n=0, str=unit; ; n++, str=NULL) {
        token = argtok (str, &state);
        if (!token)
            break;

        U->argv[n] = strdup (token);
    }

    U->cmd = U->argv[0];
}


static Unit* parse_unit (char* unit)
{
    Unit* U;

    int infiles = count_char ('<', unit);
    int outfiles = count_char ('>', unit);

    if (infiles > 1 || outfiles > 1)
        return NULL;

    if (count_char ('\'', unit) % 2)
        return NULL;

    if (count_char ('\"', unit) % 2)
        return NULL;

    U = malloc (sizeof(*U));
    U->cmd = NULL;
    U->argv = NULL;

    if (infiles)
        U->input_fn = parse_unary ('<', unit);
    else
        U->input_fn = NULL;

    if (outfiles)
        U->output_fn = parse_unary ('>', unit);
    else
        U->output_fn = NULL;

    parse_command (U, unit);

    return U;
}


static void unit_destroy (Unit** U)
{
    int i;

    if (!*U)
        return;

    if ((*U)->input_fn)
        free ((*U)->input_fn);

    if ((*U)->output_fn)
        free ((*U)->output_fn);

    if ((*U)->argv) {
        for (i=0; (*U)->argv[i]; i++)
            free ((*U)->argv[i]);
        free ((*U)->argv);
    }

    free (*U);
    *U = NULL;
}


static void parse_add_unit (Parse* P, Unit* U, int i)
{
    if (!valid_syntax (P, U, i)) {
        P->invalid_syntax = 1;
        goto out;
    }

    P->tasks[i].cmd = U->cmd;

    if (U->argv) {
        P->tasks[i].argv = U->argv;
        U->argv = NULL;
    }

    if (U->input_fn && (i == 0)) {
        P->infile = U->input_fn;
        U->input_fn = NULL;
    }

    if (U->output_fn && (i == P->ntasks-1)) {
        P->outfile = U->output_fn;
        U->output_fn = NULL;
    }

out:
    unit_destroy (&U);
}


static Parse* parse_new ()
{
    Parse* P = malloc (sizeof(*P));

    P->tasks = NULL;
    P->ntasks = 0;
    P->infile = NULL;
    P->outfile = NULL;
    P->background = 0;
    P->invalid_syntax = 0;

    return P;
}


static void parse_init (Parse* P, char* cmdline)
{
    P->background = is_background (cmdline);

    if (count_char ('&', cmdline)) {
        P->invalid_syntax = 1;
        return;
    }

    if (has_trailing ('|', cmdline)) {
        P->invalid_syntax = 1;
        return;
    }

    P->ntasks = count_char ('|', cmdline) + 1;
    P->tasks = malloc (P->ntasks * sizeof (*P->tasks));
    memset (P->tasks, 0, P->ntasks * sizeof (*P->tasks));
}


void parse_destroy (Parse** P)
{
    int i, j;

    if (!*P)
        return;

    if ((*P)->infile)
        free ((*P)->infile);

    if ((*P)->outfile)
        free ((*P)->outfile);

    if ((*P)->tasks) {
        for (i=0; i<(*P)->ntasks; i++) {
            if ((*P)->tasks[i].argv) {
                for (j=0; (*P)->tasks[i].argv[j]; j++)
                    free ((*P)->tasks[i].argv[j]);

                free ((*P)->tasks[i].argv);
            }
        }
        free ((*P)->tasks);
    }

    free (*P);
    *P = NULL;
}


Parse* parse_cmdline (char* cmdline)
{
    char *str, *token, *state;
    int i;
    Unit* U;
    Parse* P;

    if (is_empty (cmdline))
        return NULL;

    P = parse_new ();
    parse_init (P, cmdline);

    for (i=0, str=cmdline; !P->invalid_syntax; i++, str=NULL) {
        token = strtok_r (str, "|", &state);
        if (!token)
            break;

        U = parse_unit (token);

        parse_add_unit (P, U, i);
    }

    return P;
}


void parse_debug (Parse* P)
{
    int i, j;

    fprintf (stderr, "==[ DEBUG: PARSE ]==================================\n");
    fprintf (stderr, "Run in Background? %s\n", P->background ? "Yes" : "No");

    if (P->infile)
        fprintf (stderr, "infile: %s\n", P->infile);

    if (P->outfile)
        fprintf (stderr, "outfile: %s\n", P->outfile);

    fprintf (stderr, "ntasks: %i\n", P->ntasks);

    for (i=0; i<P->ntasks; i++) {
        fprintf (stderr, "Task %i\n", i);
        fprintf (stderr, "  - cmd: [%s]\n", P->tasks[i].cmd);

        if (P->tasks[i].argv)
            for (j=0; P->tasks[i].argv[j]; j++)
                fprintf (stderr, "    + arg[%i]: [%s]\n", j, P->tasks[i].argv[j]);
    }

    fprintf (stderr, "==================================[ DEBUG: PARSE ]==\n");
}
