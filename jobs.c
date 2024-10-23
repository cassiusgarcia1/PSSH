#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#include "jobs.h"

void add_job(Job *jobs, int *njobs, pid_t *pids, int ntasks){
    int index = *njobs;
    jobs[index].pids = malloc(sizeof(pid_t) * ntasks);
    memcpy(jobs[index].pids, pids, sizeof(pid_t) * ntasks);
    jobs[index].npids = ntasks;
    jobs[index].pgid = pids[0];
    jobs[index].status = BG;
    jobs[index].nterm = 0;
    jobs[index].nsus = 0;
    jobs[index].ncont = 0;
    (*njobs)++;
}

void initialize_job(Job *jobs, int *njobs, pid_t *pids, int ntasks){
    int index = *njobs;
    jobs[index].pids = malloc(sizeof(pid_t) * ntasks);
    memcpy(jobs[index].pids, pids, sizeof(pid_t) * ntasks);
    jobs[index].npids = ntasks;
    jobs[index].pgid = pids[0];
    jobs[index].status = BG;
    jobs[index].nterm = 0;
    jobs[index].nsus = 0;
    jobs[index].ncont = 0;
}

int find_job_pid(pid_t child, Job *jobs, int *njobs){
    int i, j;
    for(i = 0; i < *njobs; i++){
        for(j = 0; j < jobs[i].npids; j++)
            if(child == jobs[i].pids[j])
                return i;
    }
    return -1;
}

void print_job(Job *jobs, int i){
    if(i == -1) printf("-1 is not a job lol\n");
    if(jobs[i].status == TERM) return;
    printf("[%d] + ", i);
        
    switch(jobs[i].status){
        case(STOPPED):
            printf("stopped");
            break;
        case(TERM):
            printf("done");
            break;
        case(BG):
            printf("running");
            break;
        case(FG):
            printf("running");
            break;
    }
    printf("\t%s\n", jobs[i].name);
    fflush(stdout);  // Flush the output buffer
    
}

void print_suspended(Job *jobs, int i){
    printf("[%d] + suspended", i);
    printf("\t%s\n", jobs[i].name);
    fflush(stdout);  // Flush the output buffer
}

void print_continued(Job *jobs, int i){
    printf("[%d] + continued", i);
    printf("\t%s\n", jobs[i].name);
    fflush(stdout);  // Flush the output buffer
}

void print_done(Job *jobs, int i){
    printf("[%d] + done", i);
    printf("\t%s\n", jobs[i].name);
    fflush(stdout);  // Flush the output buffer
}

void set_fg_pgrp(pid_t pgrp)
{
    void (*sav)(int sig);

    if (pgrp == 0)
        pgrp = getpgrp();

    sav = signal(SIGTTOU, SIG_IGN);
    tcsetpgrp(STDOUT_FILENO, pgrp);
    signal(SIGTTOU, sav);
}

void remove_job(Job *jobs, int job_num, int *njobs){
    int i;
    for(i = job_num; i < *njobs; i++){
        jobs[i] = jobs[i + 1];
    }
    (*njobs)--;
}