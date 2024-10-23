#ifndef _parse_h_
#define _parse_h_

#include <limits.h>

typedef struct {
    char* cmd;
    char** argv;   /* NULL terminated array of strings */
} Task;

typedef struct {
    Task* tasks;         /* ordered list of tasks to pipe */
    int   ntasks;        /* # of tasks in the parse */

    char* infile;        /* filename of 'infile'  */
    char* outfile;       /* filename of 'outfile' */

    int background;      /* run process in background, 1 if is a background process */
    int invalid_syntax;  /* parse failed */
} Parse;


Parse* parse_cmdline (char* cmdline);
void parse_destroy (Parse** P);
void parse_debug (Parse* P);

#endif /* _parse_h_ */
