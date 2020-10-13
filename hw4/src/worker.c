#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>


#include "debug.h"
#include "polya.h"
#include "unistd.h"
#include "stdio.h"


void sigcont_handler(int sig);
void sigterm_handler(int sig);
void sighup_handler(int sig);


volatile sig_atomic_t cont_flag = 0;
volatile sig_atomic_t canceledp = 0;
volatile sig_atomic_t term_flag = 0;
volatile sig_atomic_t hup_flag = 0;

static struct problem* p = NULL;
static struct result* r = NULL;

/*
 * worker
 * (See polya.h for specification.)
 */
int worker(void) {
    // TO BE IMPLEMENTED
    debug("[%d:Worker] has been initialized.", getpid());
    signal(SIGCONT, sigcont_handler);
    signal(SIGTERM, sigterm_handler);
    signal(SIGHUP, sighup_handler);
    while(1)
    {
        //raise(SIGSTOP);
        kill(getpid(),SIGSTOP);
        if(cont_flag)
        {
            debug("[%d:Worker] SIGCONT received in worker. Working on the problem.", getpid());
            p = (struct problem*) (malloc(sizeof(struct problem))); 
            fread(p, sizeof(struct problem), 1, stdin);
            if (ferror(stdin)) exit(EXIT_FAILURE);
            p = realloc(p, p->size);
            int data_size = p->size - sizeof(struct problem);
            if(data_size>0){
                fread(p->data, sizeof(char), data_size, stdin); // read the data
                if (ferror(stdin)) exit(EXIT_FAILURE);
            }
            short p_type = p->type;
            r = solvers[p_type].solve(p, &canceledp);
            canceledp = 0;
            if(!r)
            {
                debug("[%d:Worker] Canceled. Returning empty result", getpid());
                r = (struct result*) malloc(sizeof(struct result));
                r->size = sizeof(struct result);
                r->failed = -1;
                fwrite(r, sizeof(struct result), 1, stdout);
                fflush(stdout);
            }
            else
            {
                fwrite(r, sizeof(struct result), 1, stdout);
                int r_data_size = r->size - sizeof(struct result);
                if(r_data_size) fwrite(r->data, r_data_size, 1, stdout);
                fflush(stdout);
            }
            free(r);
            free(p);
            cont_flag = 0;
        }
        else if(term_flag)
        {
            debug("[%d:Worker] SIGTERM received. Terminating.", getpid());
            free(p);
            free(r);
            return EXIT_SUCCESS;
        }
    }
    return EXIT_FAILURE;
}


void sigcont_handler(int sig)
{
    debug("[%d:Worker] SIGCONT received in sigcont handler.", getpid());
    cont_flag = 1;
    //canceledp = 0;
}


void sigterm_handler(int sig)
{
    //sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
    debug("[%d:Worker] SIGTERM received in sigterm handler.", getpid());
    term_flag = 1;
    cont_flag = 0;
}

void sighup_handler(int sig)
{
    debug("[%d:Worker] SIGHUP received in sighup handler.", getpid());
    // hup_flag = 1;
    // cont_flag = 0;
    canceledp = 1;
}
