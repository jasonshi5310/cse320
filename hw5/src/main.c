#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "pbx.h"
#include "server.h"
#include "debug.h"
#include "csapp.h"

static void terminate(int status);
static int is_port_valid(char* portp, int len);
void sighup_handler(int sig);

/*
 * "PBX" telephone exchange simulation.
 *
 * Usage: pbx <port>
 */
int main(int argc, char* argv[]){
    // Option processing should be performed here.
    // Option '-p <port>' is required in order to specify the port number
    // on which the server should listen.

    // printf("%i is_valid", is_port_valid(argv[2], strlen(argv[2])));
    // exit(0);

    // Perform required initialization of the PBX module.
    debug("Initializing PBX...");
    pbx = pbx_init();

    // TODO: Set up the server socket and enter a loop to accept connections
    // on this socket.  For each connection, a thread should be started to
    // run function pbx_client_service().  In addition, you should install
    // a SIGHUP handler, so that receipt of SIGHUP will perform a clean
    // shutdown of the server.

    if (argc!= 3)
    {
        fprintf(stderr, "You have to privide Option '-p <port>' correctly.\n");
        terminate(EXIT_FAILURE);
    }
    if (*argv[1] != '-' || *(argv[1]+1) != 'p' || *(argv[1]+2)!= '\0')
    {
        fprintf(stderr, "You have to privide Option '-p <port>' correctly.\n");
        terminate(EXIT_FAILURE);
    }
    if(!is_port_valid(argv[2], strlen(argv[2])))
    {
        fprintf(stderr, "You have to privide Option '-p <port>' correctly.\n");
        terminate(EXIT_FAILURE);
    }

    struct sigaction a;
    a.sa_handler = sighup_handler;
    if(sigaction(SIGHUP, &a, NULL)<0)
    {
        fprintf(stderr, "Failed to install SIGHUP handler");
        terminate(EXIT_FAILURE);
    }
    int listenfd;
    int* connfdp;
    socklen_t clientlen;
    pthread_t tid;
    struct sockaddr_storage clientaddr;
    listenfd = open_listenfd(argv[2]);
    while(1)
    {
        clientlen = sizeof(struct sockaddr_storage);
        connfdp = malloc(sizeof(int));
        *connfdp = accept(listenfd, (SA*) &clientaddr, &clientlen);
        pthread_create(&tid, NULL, (void*) pbx_client_service, connfdp);
        //pthread_join(tid, NULL);
    }
    terminate(EXIT_SUCCESS);

    // fprintf(stderr, "You have to finish implementing main() "
	//     "before the PBX server will function.\n");

    // terminate(EXIT_FAILURE);
}

/*
 * Function called to cleanly shut down the server.
 */
void terminate(int status) {
    debug("Shutting down PBX...");
    pbx_shutdown(pbx);
    debug("PBX server terminating");
    exit(status);
}

// check if the port number input is valid or not
// if valid, return -1; otherwise, 0;
int is_port_valid(char* portp, int len)
{
    if (len<4) return 0;
    int port_number = 0;
    for(int i =0;i<len;i++)
    {
        char c = *(portp+i);
        if (c<'0' || c>'9') return 0;
        port_number *= 10;
        port_number += c - '0';
    }
    if (port_number < 1024) return 0;
    return -1;
}

void sighup_handler(int sig)
{
    // debug("Server turned down successfully...");
    terminate(EXIT_SUCCESS);
}
