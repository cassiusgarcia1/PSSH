#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h> /* fork(), pipe(), close() */
#include <readline/readline.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <sys/wait.h> /* waitpid(), WEXITSTATUS() */


#include "builtin.h"
#include "parse.h"
#include "jobs.h"

/*******************************************
 * Set to 1 to view the command line parse *
 *******************************************/
#define DEBUG_PARSE 0

#define READ 0
#define WRITE 1

Job jobs[JOB_MAX];
int *njobs; 
pid_t shell_pid;

void print_banner ()
{
    printf ("                    ________   \n");
    printf ("_________________________  /_  \n");
    printf ("___  __ \\_  ___/_  ___/_  __ \\ \n");
    printf ("__  /_/ /(__  )_(__  )_  / / / \n");
    printf ("_  .___//____/ /____/ /_/ /_/  \n");
    printf ("/_/ Type 'exit' or ctrl+c to quit\n\n");
}


/* **returns** a string used to build the prompt
 * (DO NOT JUST printf() IN HERE!)
 *
 * Note:
 *   If you modify this function to return a string on the heap,
 *   be sure to free() it later when appropirate!  */
static char* build_prompt ()
{
    char cwd[PATH_MAX];
    char* dollar_sign = "$ ";
    if(getcwd(cwd, sizeof(cwd))==NULL){
        perror("getcwd()");
    }

    const size_t dollar_sign_len = strlen(dollar_sign);
    const size_t cwd_len = strlen(cwd);

    char* prompt = malloc(dollar_sign_len + cwd_len + 1);
    memcpy(prompt, cwd, cwd_len);
    memcpy(prompt + cwd_len, dollar_sign, dollar_sign_len + 1);

    return prompt;
}

void print_prompt ()
{
    char cwd[PATH_MAX];
    char* dollar_sign = "$ ";
    if(getcwd(cwd, sizeof(cwd))==NULL){
        perror("getcwd()");
    }

    const size_t dollar_sign_len = strlen(dollar_sign);
    const size_t cwd_len = strlen(cwd);

    char* prompt = malloc(dollar_sign_len + cwd_len + 1);
    memcpy(prompt, cwd, cwd_len);
    memcpy(prompt + cwd_len, dollar_sign, dollar_sign_len + 1);

    printf(prompt);
    free(prompt);
}


/* return true if command is found, either:
 *   - a valid fully qualified path was supplied to an existing file
 *   - the executable file was found in the system's PATH
 * false is returned otherwise */
static int command_found (const char* cmd)
{
    char* dir;
    char* tmp;
    char* PATH;
    char* state;
    char probe[PATH_MAX+1];

    int ret = 0;

    if (access (cmd, X_OK) == 0)
        return 1;

    PATH = strdup (getenv("PATH"));

    for (tmp=PATH; ; tmp=NULL) {
        dir = strtok_r (tmp, ":", &state);
        if (!dir)
            break;

        strncpy (probe, dir, PATH_MAX-1);
        strncat (probe, "/", PATH_MAX-1);
        strncat (probe, cmd, PATH_MAX-1);

        if (access (probe, X_OK) == 0) {
            ret = 1;
            break;
        }
    }

    free (PATH);
    return ret;
}

void close_safe(int fd){
    if( fd != STDIN_FILENO ){
        close(fd);
    }
}

int manipulate_fd_infile(char *infile){
    if(infile != NULL){
        int fd = open(infile, O_RDONLY);
        if (fd == -1){
            perror("fd = open(infile)");
            return -1;
        }
        if (dup2(fd, STDIN_FILENO) == -1) {
            perror("dup2(fd, STDIN_FILENO)");
            return -1;
        }
        close(fd);
    }
    return 0;
}

int manipulate_fd_outfile(char *outfile){
    if(outfile != NULL){
        int fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1){
            perror("fd = open(outfile)");
            return -1;
        }
        if (dup2(fd, STDOUT_FILENO) == -1) {
            perror("dup2(outfile, STDOUT_FILENO)");
            return -1;
        }
        close(fd);
        }
    return 0;
}

void handle_pipes(Parse* P, int index){
    int ntasks = P->ntasks;
    // manipulate stdin
    if ( prev_pipe == STDIN_FILENO ){
        if(manipulate_fd_infile(P->infile) == -1){
            perror("manipulate_fd_infile()");
            exit(EXIT_FAILURE);
        }
    }
    else{
        if (dup2(prev_pipe, STDIN_FILENO) == -1) {
            perror("dup2(prev_pipe, STDIN_FILENO)");
            exit(EXIT_FAILURE);
        }
        close(prev_pipe);
    }

    // manipulate stdout
    if (index == ntasks - 1){  
        if (manipulate_fd_outfile(P->outfile) == -1){
            perror("manipulate_fd_outfile()");
            exit(EXIT_FAILURE);
        }
    }
    else{
        if (dup2(cur_pipe[WRITE], STDOUT_FILENO) == -1) {
            perror("pssh.c: dup2(cur_pipe[WRITE], STDOUT_FILENO)");
            exit(EXIT_FAILURE);
        }
        close(cur_pipe[WRITE]);
    }

    close_safe(cur_pipe[READ]);
}

/* Called upon receiving a successful parse.
 * This function is responsible for cycling through the
 * tasks, and forking, executing, etc as necessary to get
 * the job done! */

int check_validity (Parse* P){
    unsigned int t;
    for (t = 0; t < P->ntasks; t++){
        if (!strcmp (P->tasks[t].cmd, "exit")) {
            exit (EXIT_SUCCESS);
        }
        else if (command_found (P->tasks[t].cmd) || is_builtin (P->tasks[t].cmd)) {
            //printf ("pssh: found but can't exec: %s\n", P->tasks[t].cmd);
            continue;
        }
        else {
            printf ("pssh: command not found: %s\n", P->tasks[t].cmd);
            return -1;
        }
    }
    return 0;
}


void handler(int sig)
{
    pid_t chld;
    int status;

    switch(sig) {
    case SIGTTOU:
        while(tcgetpgrp(STDOUT_FILENO) != getpgrp())
            pause();

        break;
    case SIGTTIN:
        while(tcgetpgrp(STDIN_FILENO) != getpgrp())
            pause();

        break;
    case SIGCHLD:
        while( (chld = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0 ) {
            int job_num = find_job_pid(chld,jobs,njobs);
            if (WIFSTOPPED(status)) {
                if(job_num != -1){
                    jobs[job_num].status = STOPPED;
                    jobs[job_num].nsus++;
                    if(jobs[job_num].nsus == jobs[job_num].npids){
                        printf("\n");
                        print_suspended(jobs, *njobs-1);
                    }
                }
                else{
                    jobs[(*njobs)].status = STOPPED;
                    jobs[(*njobs)++].nsus++;
                    if(jobs[*njobs-1].nsus == jobs[*njobs-1].npids){
                        printf("\n");
                        print_suspended(jobs, *njobs-1);
                    }
                }
                
            } else if (WIFCONTINUED(status)) {
                if(job_num != -1){
                    jobs[job_num].ncont++;
                    jobs[job_num].status = BG;
                    if(jobs[job_num].ncont == jobs[job_num].npids){
                        printf("\n");
                        print_job(jobs, job_num);
                    }
                }
            } else {
                if(job_num != -1){
                    jobs[job_num].nterm++;
                    jobs[job_num].status = TERM;
                    if(jobs[job_num].nterm == jobs[job_num].npids){
                        printf("\n");
                        print_done(jobs, job_num);
                        remove_job(jobs,job_num, njobs);
                    }
                }
            }
            set_fg_pgrp(shell_pid);
            fflush(stdout);  // Flush the output buffer
        }

        break;

    default:
        break;
    }
}

void execute_tasks (Parse* P, Job *jobs, int *njobs)
{
    unsigned int t, i;

    pid_t job_pids[P->ntasks];

    if(check_validity(P)!=0){
        return;
    }

    for (t = 0; t < P->ntasks; t++) {
        // only executable by child
        pipe(cur_pipe);

        job_pids[t] = fork();
        setpgid (job_pids[t], job_pids[0]);

        if(job_pids[t] == 0){
            handle_pipes(P, t);
            if (is_builtin (P->tasks[t].cmd)) {
                builtin_execute (P,jobs,njobs,t);
            }
            else {
                if (execvp(P->tasks[t].cmd, P->tasks[t].argv) == -1) {
                    perror("execvp()");
                    exit(EXIT_FAILURE);
                }
            }
        }
        else{
            close_safe(cur_pipe[WRITE]);
            close_safe(prev_pipe);
            prev_pipe = cur_pipe[READ];
            if(P->background == 1) set_fg_pgrp(0);
            else set_fg_pgrp(job_pids[0]);
        }
    }

    initialize_job(jobs, njobs ,job_pids, P->ntasks);

    // wait for all children in a loop
    /*
    if(!P->background){
        for (t = 0; t < P->ntasks; t++) {
            waitpid(-1,NULL,0);
        }
    }
    */
    
    if(P->background == 1){
        add_job(jobs, njobs ,job_pids, P->ntasks);
        int jn = *njobs-1;
        printf("[%d]", jn);
        for(i = 0; i < jobs[jn].npids; i++){
            printf(" %d", jobs[jn].pids[i]);
        }
        printf("\n");
    }

    close(prev_pipe); close(cur_pipe[READ]); close(cur_pipe[WRITE]);
    prev_pipe = STDIN_FILENO;
}


int main (int argc, char** argv)
{
    char* cmdline;
    Parse* P;
    
    int init = 0;
    njobs = &init;
    shell_pid = getpgid(0);

    signal(SIGTTOU, handler);
    signal(SIGCHLD, handler);

    print_banner();

    while (1) {
        char* line = build_prompt();
        cmdline = readline(line);
        free(line);
        if (!cmdline)       /* EOF (ex: ctrl-d) */
            exit (EXIT_SUCCESS);
        
        jobs[*njobs].name = strdup(cmdline);

        P = parse_cmdline (cmdline);
        if (!P)
            goto next;

        if (P->invalid_syntax) {
            printf ("pssh: invalid syntax\n");
            goto next;
        }

#if DEBUG_PARSE
        parse_debug (P);
#endif

        execute_tasks (P, jobs, njobs);

    next:
        parse_destroy (&P);
        free(cmdline);
    }
}
