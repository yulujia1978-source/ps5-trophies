/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "profile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(HOST_BUILD) || defined(PROFILE_TEST)
#include <pthread.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>
extern int sceUserServiceInitialize(void *params);
extern int sceUserServiceGetUserName(int user_id,char *name,size_t size);
#define CACHE_MAX 64
/* One background worker bounds the effect of a stalled daemon IPC call.
 * Buffers remain owned by the worker; timeouts never release them underneath it. */
typedef struct {char uid[9],name[128];int pending;time_t checked;} Entry;
static Entry cache[CACHE_MAX];
static int count,started;
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed=PTHREAD_COND_INITIALIZER;
static void diagnostic(const char *uid,int init_rc,int rc){
#ifndef PROFILE_TEST
    FILE *f=fopen("/data/ps5-trophies/users.log","a");
    if(f){fprintf(f,"uid=%s init=0x%08x name=0x%08x\n",uid,(unsigned)init_rc,(unsigned)rc);fclose(f);}
#else
    (void)uid;(void)init_rc;(void)rc;
#endif
}
static void *worker(void *unused){
    (void)unused;
    /* GetUserName is still attempted if a loader already initialized the service. */
    int init_rc=sceUserServiceInitialize(NULL);
    for(;;){
        pthread_mutex_lock(&mutex);int index=-1;
        while(index<0){for(int i=0;i<count;i++)if(cache[i].pending){index=i;break;}if(index<0)pthread_cond_wait(&changed,&mutex);}
        char uid[9];memcpy(uid,cache[index].uid,sizeof(uid));pthread_mutex_unlock(&mutex);
        char name[128]={0};int rc=sceUserServiceGetUserName((int32_t)strtoul(uid,NULL,16),name,sizeof(name));
        /* A successful call must supply a terminated, non-empty printable name. */
        int valid=rc==0&&name[0]&&memchr(name,0,sizeof(name));
        for(size_t i=0;valid&&name[i];i++)if((unsigned char)name[i]<32)valid=0;
        diagnostic(uid,init_rc,rc);
        pthread_mutex_lock(&mutex);if(valid)snprintf(cache[index].name,sizeof(cache[index].name),"%s",name);
        cache[index].checked=time(NULL);cache[index].pending=0;pthread_cond_broadcast(&changed);pthread_mutex_unlock(&mutex);
    }
    return NULL;
}
#endif
void profile_name(const char *uid,char *out,size_t size){
    if(!size)return;
    snprintf(out,size,"本地用户 %s",uid);
#if !defined(HOST_BUILD) || defined(PROFILE_TEST)
    pthread_mutex_lock(&mutex);
    int index=-1;for(int i=0;i<count;i++)if(!strcmp(uid,cache[i].uid)){index=i;break;}
    if(index<0&&count<CACHE_MAX){index=count++;snprintf(cache[index].uid,sizeof(cache[index].uid),"%s",uid);cache[index].pending=1;}
    if(index<0){pthread_mutex_unlock(&mutex);return;}
    if(!cache[index].pending&&time(NULL)-cache[index].checked>=30)cache[index].pending=1;
    if(!started){pthread_t thread;if(!pthread_create(&thread,NULL,worker,NULL)){pthread_detach(thread);started=1;}}
    pthread_cond_broadcast(&changed);
    /* Preserve the HTTP service even if UserService is slow or unavailable. */
    if(started&&!cache[index].name[0]&&cache[index].pending){
        struct timeval now;gettimeofday(&now,NULL);
        struct timespec until={now.tv_sec,now.tv_usec*1000L+300000000L};
        if(until.tv_nsec>=1000000000L){until.tv_sec++;until.tv_nsec-=1000000000L;}
        while(cache[index].pending)if(pthread_cond_timedwait(&changed,&mutex,&until))break;
    }
    if(cache[index].name[0])snprintf(out,size,"%s",cache[index].name);
    pthread_mutex_unlock(&mutex);
#endif
}
