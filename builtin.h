#ifndef _builtin_h_
#define _builtin_h_

#include "parse.h"
#include "jobs.h"

int is_builtin (char* cmd);
void builtin_execute (Parse* P, Job *jobs, int *njobs,int index);
void set_fg_pgrp(pid_t pgrp);
int builtin_which (Task T);

extern int cur_pipe[2], prev_pipe;

#endif /* _builtin_h_ */
