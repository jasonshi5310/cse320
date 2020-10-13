#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>


#include "debug.h"
#include "polya.h"
#include "unistd.h"
#include "stdio.h"

struct state {
    volatile int prev;
    volatile int curr;
};

struct master {
    volatile int alive;
    volatile int stopped;
    volatile int idle;
    volatile int prob_solved;
    volatile int prob_failed;
};

struct pipe {
    int read_result[2];
    int write_problem[2];
};

volatile sig_atomic_t done = 0;
// volatile sig_atomic_t changed_worker_index = -1;
volatile sig_atomic_t aborted = 0;
int wstatus;
pid_t* worker_ids;
struct state* worker_states;
volatile int wcount;
struct master* m;
struct problem* current_problem = NULL;


int w_index(pid_t pid);

void worker_initialzing_handler(int sig)
{
    pid_t wpid;
    debug("[%d:Master] Received signal 17", getpid());
    while ((wpid = waitpid(-1, &wstatus, WUNTRACED|WNOHANG|WCONTINUED))!=0){
        if(wpid == -1)
        {
            //debug("[%d:Master] waitpid() error", getpid());
            return;
        }
        int wkr_i = w_index(wpid); 
        debug("worker index %i", wkr_i);
        struct state* s = worker_states+wkr_i;
        if(WIFSTOPPED(wstatus))
        {
            debug("[%d:Master] Worker #%d (pid = %d, state = %d) has stopped"
                , getpid(), wkr_i, wpid, s->curr);
            if(s->curr == WORKER_STARTED)
            {
                s->prev = s->curr;
                s->curr = WORKER_IDLE;
                sf_change_state(wpid, WORKER_STARTED, WORKER_IDLE);
                m->idle++;
                //changed_worker_index = (sig_atomic_t) wkr_i;
            }
            else if(s->curr== WORKER_RUNNING || s->curr == WORKER_CONTINUED)
            {
                s-> prev = s->curr;
                s->curr = WORKER_STOPPED;
                sf_change_state(wpid, WORKER_RUNNING, WORKER_STOPPED);
                m->stopped++;
            }
            
        }
        else if(WIFEXITED(wstatus))
        {
            if (WEXITSTATUS(wstatus)==0)
            {
                s-> prev = s->curr;
                s->curr = WORKER_EXITED;
                sf_change_state(wpid, (worker_states+wkr_i)->prev, WORKER_EXITED);
            }
            else
            {
                aborted += 1;
                s-> prev = s->curr;
                s->curr = WORKER_ABORTED;
                sf_change_state(wpid, (worker_states+wkr_i)->prev, WORKER_ABORTED);
            }
            
        }
        else if(WIFCONTINUED(wstatus))
        {
            if(s->curr == WORKER_CONTINUED||done==1)
            {
                s-> prev = s->curr;
                s->curr = WORKER_RUNNING;
                sf_change_state(wpid, (worker_states+wkr_i)->prev, WORKER_RUNNING);
            }
        }
        // else
        // {
        //     aborted = 1;
        //     s-> prev = s->curr;
        //     s->curr = WORKER_ABORTED;
        //     sf_change_state(wpid, (worker_states+wkr_i)->prev, WORKER_ABORTED);
        // } 
        // changed_worker_index = (sig_atomic_t) wkr_i;
    }
}

int w_index(pid_t pid)
{
    for(int i = 0; i<wcount;i++)
    {
        if(*(worker_ids+i)==pid) return i;
    }
    return -1;
}

void cancel_other_working(int index)
{
    for (int i = 0; i<wcount; i++)
    {
        if (i != index && ((worker_states+i)->curr == WORKER_RUNNING 
        ||(worker_states+i)->curr == WORKER_CONTINUED))
        {
            sf_cancel(*(worker_ids+i));
            kill(*(worker_ids+i), SIGHUP);
            // (worker_states+i)->curr = WORKER_STOPPED;
            // (worker_states+i)->prev = WORKER_RUNNING;
            // sf_change_state(worker_ids+i, WORKER_RUNNING, WORKER_STOPPED);
        }
    }
}

int is_all_idle()
{
    if (wcount==m->idle) return 1;
    return 0;
}

/*
 * master
 * (See polya.h for specification.)
 */
int master(int workers) {
    // TO BE IMPLEMENTED
    sf_start();
    worker_ids = (pid_t*) malloc(sizeof(pid_t)*workers);
    worker_states = (struct state*) malloc(sizeof(struct state)*workers);
    m = (struct master*) malloc(sizeof(struct master));
    m->prob_solved = 0;
    m->alive = 0;
    m->idle = 0;
    m->stopped = 0;
    wcount = workers;
    sigset_t mask, prev;
    sigemptyset(&mask);
    sigemptyset(&prev);
    sigaddset(&mask, SIGCHLD);

    // pipes = (struct pipe*) malloc(sizeof(struct pipe)*workers);
    struct pipe pipes[workers];
    signal(SIGCHLD, worker_initialzing_handler);
    // printf("ss");
    // exit(0);
    // for(int i = 0;i<workers; i++)
    // {
    //     debug("[%i:Master] Starting worker %i", getpid(), i);
    //     (worker_states+i)->curr = WORKER_STARTED;
    //     (worker_states+i)->prev = 0;
    //     pipe(pipes[i].read_result);
    //     pipe(pipes[i].write_problem);
        
    //     if ((*(worker_ids+i)=fork())==0)
    //     {
    //         dup2(pipes[i].write_problem[0], STDIN_FILENO);
    //         close(pipes[i].write_problem[1]);
    //         dup2(pipes[i].read_result[1], STDOUT_FILENO);
    //         close(pipes[i].read_result[0]);
    //         //debug("[%i:Master] String", getpid());
    //         if (execl("bin/polya_worker", "polya_worker",NULL)==-1)
    //             debug("[%i:Worker] execl failed", getpid()); 
    //         //sleep(1);
    //         // exit(EXIT_SUCCESS);
    //     }// Initializing workers
    //     else if(worker_ids[i]< 0) {debug("worker failed to fork"); done = 1;}
    //     else{
    //         sf_change_state(*(worker_ids+i),0,WORKER_STARTED);
    //         m->alive++;
    //         debug("[%i:Master] Workers alive: %i", getpid(), m->alive);
    //     }
    // }
    // sigset_t mask;
    // sigemptyset(&mask);
    // sigaddset(&mask, SIGCHLD);
    int i = 0;
    while(!done)
    {
        // sigsuspend(&mask);
        // printf("ss");
        // exit(0);
        //if(changed_worker_index!= -1)
        if (i < workers)
        {
            debug("[%i:Master] Starting worker %i", getpid(), i);
            (worker_states+i)->curr = WORKER_STARTED;
            (worker_states+i)->prev = 0;
            pipe(pipes[i].read_result);
            pipe(pipes[i].write_problem);
            
            if ((*(worker_ids+i)=fork())==0)
            {
                dup2(pipes[i].write_problem[0], STDIN_FILENO);
                close(pipes[i].write_problem[1]);
                dup2(pipes[i].read_result[1], STDOUT_FILENO);
                close(pipes[i].read_result[0]);
                //debug("[%i:Master] String", getpid());
                if (execl("bin/polya_worker", "polya_worker",NULL)==-1)
                    debug("[%i:Worker] execl failed", getpid()); 
                //sleep(1);
                // exit(EXIT_SUCCESS);
            }// Initializing workers
            else if(worker_ids[i]< 0) {debug("worker failed to fork"); done = 1;}
            else{
                sf_change_state(*(worker_ids+i),0,WORKER_STARTED);
                m->alive++;
                debug("[%i:Master] Workers alive: %i", getpid(), m->alive);
                i++;
            }
        }
        if(is_all_idle())
        {
            debug("[%i:Master] %i Workers are all idle", getpid(), m->idle);
            for (int j = 0; j<workers; j++)
            {
                int wkr_i = j;
                pid_t wkr_pid = *(worker_ids+wkr_i);
                struct state *cur_wkr_state = (worker_states + wkr_i);
                debug("[%i:Master] Creating new problem for worker %i (pid = %i)"
                , getpid(), wkr_i, wkr_pid);
                current_problem = get_problem_variant(wcount, wkr_i);
                if (current_problem==NULL)
                {
                    done = 1;
                    debug("[%i:Master] No more problems", getpid());
                    break;
                }
                else
                {
                    debug("[%i:Master] Assinging problem to worker %i (pid = %i) "
                    , getpid(), wkr_i, wkr_pid);
                    int p_size = current_problem->size;
                    //fwrite(current_problem, p_size, 1, &((pipes+wkr_i)->fd[1]));
                    write((pipes[wkr_i].write_problem[1]),current_problem, p_size);
                    sf_send_problem(wkr_pid, current_problem);
                    debug("[%i:Master] Problem writed to the worker pipe", getpid());
                    sf_change_state(wkr_pid, WORKER_IDLE, WORKER_CONTINUED);
                    cur_wkr_state->curr = WORKER_CONTINUED;
                    cur_wkr_state->prev = WORKER_IDLE;
                    m->idle--;
                    kill(wkr_pid, SIGCONT);
                }
            }
        }
        if(m->stopped !=0)
            for (int k = 0; k<workers; k++)
            {
                int wkr_i = k;
                pid_t wkr_pid = *(worker_ids+wkr_i);
                struct state *cur_wkr_state = (worker_states + wkr_i);
                if(cur_wkr_state->curr == WORKER_STOPPED)
                {
                    struct result* r = malloc(sizeof(struct result));
                    //fread(r, sizeof(struct result), 1,&((pipes+wkr_i)->fd[0]));
                    read(pipes[wkr_i].read_result[0], r, sizeof(struct result));
                    // sigprocmask(SIG_BLOCK, &mask, &prev);
                    if (r!= NULL)
                    {
                        r = realloc(r, r->size);
                        int d_size = r->size - sizeof(struct result);
                        //fread(r, d_size, 1, (pipes+wkr_i)->fd[0]);
                        if (d_size>0)
                            read(pipes[wkr_i].read_result[0], r->data, d_size);
                        sf_recv_result(wkr_pid, r);
                        if (r->failed==0)
                        {
                            int solved = post_result(r, current_problem);
                            debug("[%i:Master] Result posted", getpid());
                            if(solved == 0)
                            {
                                cancel_other_working(wkr_i);
                                m->prob_solved++;
                            }
                        }
                    }
                    // sigprocmask(SIG_SETMASK, &prev, NULL);
                    free(r);
                    m->idle++;
                    m->stopped--;
                    cur_wkr_state->prev = WORKER_STOPPED;
                    cur_wkr_state->curr = WORKER_IDLE;
                    sf_change_state(wkr_pid, WORKER_STOPPED, WORKER_IDLE);
                }
        }
    }
    for (int w = 0; w<workers;w++)
    {
        //printf("%i pid\n", (int)*(worker_ids+w));
        if ((worker_states+w)->curr == WORKER_IDLE)
        {
            pid_t wpid = *(worker_ids+w);
            kill(wpid, SIGCONT);
            kill(wpid, SIGTERM);
            // printf("%i pidsss", (int)wpid);
        }
    }
    if (m->idle == wcount && m->prob_failed == 0)
    {
        debug("[%i:Master] All problems solved and no workers failed!", getpid());
    }
    if(m->prob_failed>0)
    {
        debug("[%i:Master] Not all problems are solved", getpid());
    }
    if(m->idle != wcount){
        debug("[%i:Master] Not all worker are terminating correctly", getpid());
    }
    free(worker_ids);
    free(worker_states);
    free(m);
    sf_end();
    return EXIT_SUCCESS;
    //return EXIT_FAILURE;
}
