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


typedef struct tu{
    TU_STATE state;
    int this_fd;
    int connected_fd;
}TU;

typedef struct pbx{
    sem_t mutex;
    TU tu_array[PBX_MAX_EXTENSIONS];
}PBX;

volatile sig_atomic_t stop_flag = 0;


void state_print(TU* tu);


PBX *pbx_init()
{
    PBX* p = (PBX*) malloc(sizeof(PBX));
    Sem_init(&(p->mutex), 0, 1);
    TU* array = p->tu_array;
    for(int i = 0;i<PBX_MAX_EXTENSIONS;i++)
    {
        TU* tu = array+i;
        tu->state = -1;
        tu->connected_fd = -1;
        tu->this_fd = -1;
    }
    // debug("pbx_init finished");
    return p;
}

void pbx_shutdown(PBX *pbx)
{
    P(&(pbx->mutex));
    stop_flag = -1;
    for(int i=0; i< PBX_MAX_EXTENSIONS; i++)
    {
        TU* tu = pbx->tu_array + i;
        if(tu->this_fd != -1) shutdown(tu->this_fd, SHUT_RD);
    }
    V(&(pbx->mutex));
    int check = -1;
    while(check)
    {
        check = 0;
        for(int j = 0; j<PBX_MAX_EXTENSIONS; j++)
        {
            TU* tu = pbx->tu_array + j;
            if(tu->this_fd != -1) {check = -1; break;}
        }
    }
    free(pbx);
    // debug("Pbx shutdown");
}

TU *pbx_register(PBX *pbx, int fd)
{
    P(&(pbx->mutex));
    if(stop_flag) {V(&(pbx->mutex)); return NULL;}
    TU *array = pbx->tu_array;
    TU *tu = array + fd;
    tu->this_fd = fd;
    tu->connected_fd = -1;
    tu->state = TU_ON_HOOK;
    char m[20];
    //write(tu->this_fd, tu_state_names[TU_ON_HOOK], 7);
    int count = sprintf(m, "%s %i\n", tu_state_names[TU_ON_HOOK], tu->this_fd);
    write(tu->this_fd, m, count);
    V(&(pbx->mutex));
    return tu;
}

int pbx_unregister(PBX *pbx, TU *tu)
{
    // debug("Unregister tu");
    P(&(pbx->mutex));
    if (tu==NULL || tu->this_fd == -1) {V(&(pbx->mutex));return -1;}
    // debug("the other fd %i", tu->connected_fd);
    if(tu->connected_fd != -1)
    {
        int count;
        char m[20];
        TU* the_other_guy = pbx->tu_array + tu->connected_fd;
        the_other_guy->connected_fd = -1;
        if (the_other_guy->state == TU_CONNECTED 
            || the_other_guy->state == TU_RING_BACK)
        {
            the_other_guy->state = TU_DIAL_TONE;
            write(the_other_guy->this_fd, tu_state_names[TU_DIAL_TONE], 9);
            write(the_other_guy->this_fd, "\n", 1);
        }
        else if(the_other_guy->state == TU_RINGING)
        {
            the_other_guy->state = TU_ON_HOOK;
            count = sprintf(m, "%s %i\n", tu_state_names[TU_ON_HOOK], the_other_guy->this_fd);
            write(the_other_guy->this_fd, m, count);
        }
    }
    tu->this_fd = -1;
    tu->connected_fd = -1;
    tu->state = -1;
    V(&(pbx->mutex));
    return 0;
}

int tu_fileno(TU *tu)
{
    return tu->this_fd;
}

int tu_extension(TU *tu)
{
    return tu->this_fd;
}

int tu_pickup(TU *tu)
{
    // debug("Pickup called");
    P(&(pbx->mutex));
    if(stop_flag) {V(&(pbx->mutex)); return -1;}
    if(tu==NULL || tu->this_fd == -1) {V(&(pbx->mutex));return -1;}
    // debug("in thread %i", tu->this_fd);
    if(tu->state == TU_ON_HOOK)
    {
        tu->state = TU_DIAL_TONE;
        write(tu->this_fd, tu_state_names[TU_DIAL_TONE], 9);
        write(tu->this_fd, "\n", 1);
    }
    else if(tu->state == TU_RINGING)
    {
        tu->state = TU_CONNECTED;
        TU* the_other_guy = pbx->tu_array + tu->connected_fd;
        the_other_guy->state = TU_CONNECTED;
        char m[25];
        int count = sprintf(m, "%s %i\n", tu_state_names[TU_CONNECTED], tu->connected_fd);
        write(tu->this_fd, m, count);
        count = sprintf(m, "%s %i\n", tu_state_names[TU_CONNECTED], the_other_guy->connected_fd);
        write(the_other_guy->this_fd, m, count);
    }
    else
    {
        state_print(tu);
    }
    V(&(pbx->mutex));
    return 0;
}

int tu_hangup(TU *tu)
{
    // debug("Hangup called");
    P(&(pbx->mutex));
    if(stop_flag) {V(&(pbx->mutex)); return -1;}
    if(tu==NULL || tu->this_fd == -1) {V(&(pbx->mutex));return -1;}
    if (tu->state == TU_ON_HOOK)
    {
        state_print(tu);
        V(&(pbx->mutex));
        return 0;
    }
    char m[20];
    int count;
    if (tu->connected_fd != -1)
    {
        TU* the_other_guy = pbx->tu_array + tu->connected_fd;
        the_other_guy->connected_fd = -1;
        if (the_other_guy->state == TU_CONNECTED 
            || the_other_guy->state == TU_RING_BACK)
        {
            the_other_guy->state = TU_DIAL_TONE;
            write(the_other_guy->this_fd, tu_state_names[TU_DIAL_TONE], 9);
            write(the_other_guy->this_fd, "\n", 1);
        }
        else if(the_other_guy->state == TU_RINGING)
        {
            the_other_guy->state = TU_ON_HOOK;
            count = sprintf(m, "%s %i\n", tu_state_names[TU_ON_HOOK], the_other_guy->this_fd);
            write(the_other_guy->this_fd, m, count);
        }
    }
    tu->state = TU_ON_HOOK;
    count = sprintf(m, "%s %i\n", tu_state_names[TU_ON_HOOK], tu->this_fd);
    write(tu->this_fd, m, count);
    tu->connected_fd = -1;
    V(&(pbx->mutex));
    return 0;
}

int tu_dial(TU *tu, int ext)
{
    // debug("Dial called");
    P(&(pbx->mutex));
    if(stop_flag) {V(&(pbx->mutex)); return -1;}
    if(tu==NULL || tu->this_fd == -1) {V(&(pbx->mutex));return -1;}
    if(tu->state!= TU_DIAL_TONE)
    {
        state_print(tu);
        V(&(pbx->mutex));
        return 0;
    }
    TU* the_other_guy = pbx->tu_array + ext;
    // If the other guy doesn't exist
    if(the_other_guy==NULL){V(&(pbx->mutex));return -1;}
    if(the_other_guy->state==-1)
    {
        tu->state = TU_ERROR;
        write(tu->this_fd, tu_state_names[TU_ERROR], 5);
        write(tu->this_fd, "\n", 1);
    }
    // If the other guy is busy
    else if(the_other_guy->state != TU_ON_HOOK)
    {
        tu->state = TU_BUSY_SIGNAL;
        write(tu->this_fd, tu_state_names[TU_BUSY_SIGNAL], 11);
        write(tu->this_fd, "\n", 1);
    }
    // If the other guy is free
    else if(the_other_guy->state == TU_ON_HOOK)
    {
        tu->state = TU_RING_BACK;
        tu->connected_fd = the_other_guy->this_fd;
        the_other_guy->state = TU_RINGING;
        the_other_guy->connected_fd = tu->this_fd;
        write(tu->this_fd, tu_state_names[TU_RING_BACK], 9);
        write(tu->this_fd, "\n", 1);
        write(the_other_guy->this_fd, tu_state_names[TU_RINGING], 7);
        write(the_other_guy->this_fd, "\n", 1);
    }
    V(&(pbx->mutex));
    return 0;
}

int tu_chat(TU *tu, char *msg)
{
    // debug("Chat called");
    P(&(pbx->mutex));
    if(stop_flag) {V(&(pbx->mutex)); return -1;}
    if(tu==NULL || tu->this_fd == -1) {V(&(pbx->mutex));return -1;}
    // if(tu->state!=TU_CONNECTED)
    // {
    //     state_print(tu);
    // }
    if(tu->state==TU_CONNECTED && tu->connected_fd!= -1)
    {
        TU* the_other_guy = pbx->tu_array + tu->connected_fd;
        write(the_other_guy->this_fd, "CHAT ", 5);
        for(int i=0; i<strlen(msg);i++)
        {
            if(*(msg+i)!= '\r' && *(msg+i) != '\n')
            {
                write(the_other_guy->this_fd, (msg+i), 1);
            }
        }
        write(the_other_guy->this_fd, "\n", 1);
    }
    state_print(tu);
    V(&(pbx->mutex));
    return 0;
}

void state_print(TU* tu)
{
    // debug("Print state");
    char m[20];
    int count;
    switch (tu->state)
    {
    case TU_ON_HOOK:
        count = sprintf(m, "%s %i\n", tu_state_names[TU_ON_HOOK], tu->this_fd);
        write(tu->this_fd, m, count);
        break;
    case TU_RINGING:
        write(tu->this_fd, tu_state_names[TU_RINGING], 7);
        write(tu->this_fd, "\n", 1);
        break;
    case TU_DIAL_TONE:
    case TU_RING_BACK:
        write(tu->this_fd, tu_state_names[tu->state], 9);
        write(tu->this_fd, "\n", 1);
        break;
    case TU_BUSY_SIGNAL:
        write(tu->this_fd, tu_state_names[TU_BUSY_SIGNAL], 11);
        write(tu->this_fd, "\n", 1);
        break;
    case TU_ERROR:
        write(tu->this_fd, tu_state_names[TU_ERROR], 5);
        write(tu->this_fd, "\n", 1);
        break;
    case TU_CONNECTED:
        count = sprintf(m, "%s %i\n", tu_state_names[TU_CONNECTED], tu->connected_fd);
        write(tu->this_fd, m, count);
        break;
    default:
        break;
    }
}
