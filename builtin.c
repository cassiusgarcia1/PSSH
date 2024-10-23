#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h> /* pid_t */
#include <sys/stat.h> 
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>

#include "builtin.h"
#include "parse.h"
#include "jobs.h"

#define READ 0
#define WRITE 1

int cur_pipe[2], prev_pipe = STDIN_FILENO;

static char* builtin[] = {
    "exit",   /* exits the shell */
    "which",  /* displays full path to command */
    "kill",
    "bg",
    "fg",
    "jobs",
    NULL
};


int is_builtin (char* cmd)
{
    int i;

    for (i=0; builtin[i]; i++) {
        if (!strcmp (cmd, builtin[i]))
            return 1;
    }

    return 0;
}

int extract_job_number(const char *input){
    // Find the position of '%'
    const char *percentSign = strchr(input, '%');

    // If '%' is found
    if (percentSign != NULL) {
        // Convert the substring after '%' to an integer
        int numberAfterPercent = atoi(percentSign + 1);
        return numberAfterPercent;
    } else {
        // Return a default value or handle the case where '%' is not found
        return -1;  // Or any other default value indicating absence of '%'
    }
}

pid_t find_job_pgid(Job *job, int job_num){
    return job[job_num].pgid;
}

void which (Parse* P, int index){
    char* dir;
    char* tmp;
    char* PATH;
    char* state;
    char probe[PATH_MAX];

    Task T = P->tasks[index];
    
    int i;
    for (i = 1; T.argv[i] != NULL; i++){
        if (access (T.argv[i], X_OK) == 0){
            return;
        }

        if(is_builtin(T.argv[i])){
            printf(T.argv[i]);
            printf(": shell built-in command\n");
            continue;
        }

        PATH = strdup (getenv("PATH"));
            
        for (tmp=PATH; ; tmp=NULL) {
            dir = strtok_r (tmp, ":", &state);
            if (!dir)
                break;

            strncpy (probe, dir, PATH_MAX-1);
            strncat (probe, "/", PATH_MAX-1);
            strncat (probe, T.argv[i], PATH_MAX-1);

            if (access (probe, X_OK) == 0) {
                break;
            }
        }

        printf(probe); printf("\n"); 
            
    }
    free (PATH);
}

void jobs_func (Parse* P, Job *jobs, int *njobs){
    int i;
    for(i = 0; i < *njobs; i++){
        print_job(jobs,i);
    }
}

const char *sigabbrev(unsigned int sig)
{
    const char *sigs[31] = { "HUP", "INT", "QUIT", "ILL", "TRAP", "ABRT",
    "BUS", "FPE", "KILL", "USR1", "SEGV", "USR2", "PIPE", "ALRM",
    "TERM", "STKFLT", "CHLD", "CONT", "STOP", "TSTP", "TTIN",
    "TTOU", "URG", "XCPU", "XFSZ", "VTALRM", "PROF", "WINCH", "IO",
    "PWR", "SYS" };
    if (sig == 0 || sig > 31)
        return NULL;
    
    return sigs[sig-1];
}

const char *sigabbrev_desc(unsigned int sig)
{
    const char *sigs[31] = { "Hangup", "Interrupt", "Quit", "Illegal instruction", "Trace/breakpoint trap", "Aborted",
    "Bus error", "Floating point exception", "Killed", "User defined signal 1", " Segmentation fault", "User defined signal 2", "Broken pipe", " Alarm clock",
    "Terminated", "Stack fault", "Child exited", "Continued", "Stopped (signal)", "Stopped", " Stopped (tty input)",
    "Stopped (tty output)", "Urgent I/O condition", "CPU time limit exceeded", "File size limit exceeded", "Virtual timer expired", "Profiling timer expired", "Window changed", "I/O possible",
    "Power failure", "Bad system call" };
    if (sig == 0 || sig > 31)
        return NULL;
    
    return sigs[sig-1];
}

void print_sig_list(){
    int i = 0;
    for (i = 1; i < 32; i++){
        printf("%d) SIG%s",i,sigabbrev(i));
        printf("\t%s\n",sigabbrev_desc(i));
    }
}

int def (pid_t pid,int is_pgid){
    // Use signal 15
    if(is_pgid == 1) pid = -pid;
    if(kill(pid,15)==-1){
        perror("kill()");
        return -1;
    }
    return 0;
}

int specific_signal (int sig,int pid,int is_pgid){
    pid = (pid_t) pid;
    if(sig == 0 && is_pgid == 1){
        if(is_pgid == 1){
            kill(-pid,sig);
            switch(errno){
                case 0:
                    printf("Job %d exists and is able to receive signals\n",pid);
                    break;
                case EPERM:
                    printf("Job %d exists, but we can't send it signals\n",pid);
                    break;
                case ESRCH:
                    printf("FIX LOGIC: pssh: invalid job number: %d\n",pid);
                    break;
            }
        }
        else{
            kill(pid,sig);
            switch(errno){
                case 0:
                    printf("PID %d exists and is able to receive signals\n",pid);
                    break;
                case EPERM:
                    printf("PID %d exists, but we can't send it signals\n",pid);
                    break;
                case ESRCH:
                    printf("pssh: invalid pid: %d\n",pid);
                    break;
            }
        }
    }
    else{
        if(is_pgid == 1) pid = -pid;
        if(kill(pid,sig)==-1){
            perror("kill()");
            return -1;
        }
    }
    return 0;
}

int check_valid_job_num(int job_num, int *njobs){
    if(job_num == -1 || job_num >= *njobs){
        printf("pssh: invalid job number: [%d]",job_num);
        return -1;
    }
    return 0;
}

void kill_func(Task T, Job *jobs, int *njobs){
    int argc = 0;
    while (T.argv[argc] != NULL) {
        argc++;
    }
    if (argc == 1){
        printf("Usage: kill [-s <signal>] <pid> | %%<job> ...\n");
    }
    else if(strcmp(T.argv[1],"-l") == 0){
        print_sig_list();
    }
    else if(strcmp(T.argv[1],"-s") == 0){
        int pid = atoi(T.argv[3]);
        int job = extract_job_number(T.argv[3]);
    
        if(check_valid_job_num(job,njobs)==-1){
            specific_signal(atoi(T.argv[2]),pid,0);
            return;
        }
        else {
            int pgid = find_job_pgid(jobs, job);
            specific_signal(atoi(T.argv[2]),pgid,1);
            jobs[job].status = TERM;
            return;
        }
        
    }
    else{
        int pid = atoi(T.argv[1]);
        int job = extract_job_number(T.argv[1]);
        if(check_valid_job_num(job,njobs)==-1){
            def((pid_t) pid,0);
            return;
        }
        if(job !=- 1){
            pid_t pgid = find_job_pgid(jobs, job);
            def(pgid,1);
            jobs[job].status = TERM;
            return;
        }
    }
}

void fg_func(Task T, Job *jobs, int *njobs){
    int argc = 0;
    while (T.argv[argc] != NULL) {
        argc++;
    }
    if (argc == 1){
        printf("Usage: fg %%<job number>\n");
        return;
    }

    int job_num = extract_job_number(T.argv[1]);
    if(check_valid_job_num(job_num,njobs)==-1){
        return;
    }
    kill(-jobs[job_num].pgid, SIGCONT);
    set_fg_pgrp(jobs[job_num].pgid);
    jobs[job_num].status = FG;
    print_job(jobs,job_num);
    while(jobs[job_num].status != TERM || jobs[job_num].status != STOPPED){}
}

void bg_func(Task T, Job *jobs, int *njobs){
    int argc = 0;
    while (T.argv[argc] != NULL) {
        argc++;
    }
    if (argc == 1){
        printf("Usage: fg %%<job number>\n");
        return;
    }

    int job_num = extract_job_number(T.argv[1]);
    if(check_valid_job_num(job_num,njobs)==-1){
        return;
    }
    kill(-jobs[job_num].pgid, SIGCONT);
    jobs[job_num].status = BG;
    //print_job(jobs,job_num);
    wait(NULL);
}

void builtin_execute (Parse* P, Job *jobs, int *njobs, int index)
{
    Task T = P->tasks[index];
    if (!strcmp (T.cmd, "exit")) {
        exit (EXIT_SUCCESS);
    }
    else if (!strcmp (T.cmd, "which")) {
        which (P,index);
    }
    else if (!strcmp (T.cmd, "jobs")) {
        jobs_func (P,jobs,njobs);
    }
    else if (!strcmp (T.cmd, "kill")) {
        kill_func (T,jobs,njobs);
    }
    else if (!strcmp (T.cmd, "fg")) {
        fg_func (T,jobs,njobs);
    }
    else if (!strcmp (T.cmd, "bg")) {
        bg_func (T,jobs,njobs);
    }
    else {
        printf ("pssh: builtin command: %s (not implemented!)\n", T.cmd);
    }
    kill(getpid(),SIGTERM);
}
