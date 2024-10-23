#ifndef _jobs_h_
#define _jobs_h_


#include <sys/types.h> /* pid_t */
#define JOB_MAX 264

typedef enum {
    STOPPED,
    TERM,
    BG,
    FG,
} JobStatus;

typedef struct {
    char* name;
    pid_t* pids;
    unsigned int npids;
    unsigned int nterm;
    unsigned int nsus;
    unsigned int ncont;
    pid_t pgid;
    JobStatus status;
} Job;

void add_job(Job *jobs, int *njobs, pid_t *pids, int ntasks);

void initialize_job(Job *jobs, int *njobs, pid_t *pids, int ntasks);

int find_job_pid(pid_t child, Job *jobs, int *njobs);

void print_job(Job *jobs, int i);

void print_suspended(Job *jobs, int i);

void print_continued(Job *jobs, int i);

void print_done(Job *jobs, int i);

void remove_job(Job *jobs, int job_num, int *njobs);

#endif /* _jobs_h_ */