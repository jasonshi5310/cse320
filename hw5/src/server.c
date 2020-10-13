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

const char* PICKUP = "pickup\r\n";
const char* HANGUP = "hangup\r\n";


int dial_number(char* op, int len)
{
    if (len<8) return -1;
    int port_number = 0;
    for(int i =5;i<len-2;i++)
    {
        char c = *(op+i);
        if (c<'0' || c>'9') return -1;
        port_number *= 10;
        port_number += c - '0';
    }
    if (port_number > 1023) return -1; 
    return port_number;
}


void *pbx_client_service(void *arg)
{
    int connfd = *((int*) arg);
    pthread_detach(pthread_self());
    free(arg);
    TU* tu = pbx_register(pbx, connfd);
    // debug("Thread %i created and registered", tu_extension(tu));
    int client_fd = tu_fileno(tu);
    const char* type = "r";
    FILE* client_file = fdopen(client_fd, type);
    char input;
    char* op;
    char* cursur;
    int op_size = 0;
    int is_end = 0; // is the end of the file, default as false
    op = malloc(sizeof(char)*1024);
    int current_size = 1024;
    while(1)
    {
        // debug("Thread %i reading in", tu_extension(tu));
        cursur = op;
        while((input=fgetc(client_file)))
        {
            //debug("reading %c", input);
            if(input == EOF)
            {
                // debug("EOF reached in thread %li", (long)pthread_self());
                is_end = -1;
                break;
            }
            else if(input == '\n' && *(cursur-1) == '\r')
            {
                op_size += 1;
                // debug("New Line reached in thread %li. The size is %i",
                //     (long)pthread_self(), op_size);
                *cursur++ = input;
                *cursur = '\0';
                // op = realloc(op, op_size);
                break;
            }
            *cursur++ = input;
            op_size += 1;
            if (op_size == current_size)
            {
                current_size *= 2;
                op = realloc(op, sizeof(char)*current_size);
                cursur = op + op_size;
            }
            //debug("readin: %c", input);
        }
        if(is_end)
        {
            op_size = 0;
            // free(op);
            break;
        }
        // If not enough lenth
        // debug("the original op %s", op);
        if (strlen(op)<6)
        {
            // debug("Not long enough. the original op length %i", (int)strlen(op));
            op_size =0;
            //free(op);
            continue;
        }
        char* c_or_d = malloc(sizeof(char)*5);
        for (int i = 0; i<4; i++)
        {
            *(c_or_d+i) = *(op+i);
        }
        *(c_or_d+4) = '\0';
        // pickup
        if(!strcmp(op, PICKUP))
        {
            // debug("echo %s",op);
            tu_pickup(tu);
        }
        // hangup
        else if (!strcmp(op, HANGUP))
        {
            // debug("echo %s", op);
            tu_hangup(tu);
        }
        // chat
        else if(!strcmp(c_or_d, tu_command_names[TU_CHAT_CMD]))
        {
            // debug("echo %s", op);
            char* msg = op+5;
            tu_chat(tu, msg);
        }
        // dial
        else if(!strcmp(c_or_d, tu_command_names[TU_DIAL_CMD]))
        {
            int ext = dial_number(op, strlen(op));
            // debug("echo %s %i", c_or_d, ext);
            if (ext!= -1) tu_dial(tu, ext);
        }
        op_size =0;
        free(c_or_d);
    }
    free(op);
    pbx_unregister(pbx, tu);
    fclose(client_file);
    // debug("hahahahhahhahahahahahahahahahahahah %i", x);
    return NULL;
}