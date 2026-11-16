/* SPDX-License-Identifier: GPL-3.0-or-later
 * Cooperative lock/STOP handoff follows the PS5 Turbo upgrade design.
 * No PID-based termination and no changes to another plugin's files.
 */
#include "lifecycle.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
static int join(char *out,size_t n,const char *dir,const char *leaf){int got=snprintf(out,n,"%s/%s",dir,leaf);return got<0||(size_t)got>=n?-1:0;}
static int parse_version(const char *text,unsigned v[3]){
    for(int i=0;i<3;i++){unsigned n=0,k=0;while(isdigit((unsigned char)*text)){if(++k>5)return -1;n=n*10+(unsigned)(*text++-'0');}if(!k)return -1;v[i]=n;if(i<2&&*text++!='.')return -1;}return *text?-1:0;
}
static int same_or_newer(const char *current,const char *next){unsigned a[3],b[3];if(parse_version(current,a)||parse_version(next,b))return 1;for(int i=0;i<3;i++)if(a[i]!=b[i])return a[i]>b[i];return 1;}
void instance_ready(Instance *s){if(s->upgrade_fd>=0){close(s->upgrade_fd);s->upgrade_fd=-1;}}
void instance_close(Instance *s){if(s->service_fd>=0){if(s->owned)ftruncate(s->service_fd,0);close(s->service_fd);s->service_fd=-1;s->owned=0;}instance_ready(s);}
static int fail(Instance *s,const char *why){snprintf(s->error,sizeof(s->error),"%s",why);instance_close(s);return -1;}
int instance_acquire(Instance *s,const char *dir,const char *version,unsigned timeout_ms){
    memset(s,0,sizeof(*s));s->service_fd=s->upgrade_fd=-1;unsigned parsed[3];
    if(parse_version(version,parsed)||strlen(dir)>=sizeof(s->directory))return fail(s,"invalid version or service directory");
    snprintf(s->directory,sizeof(s->directory),"%s",dir);char path[1400],stop_path[1400];
    if(join(path,sizeof(path),dir,"upgrade.lock")||join(stop_path,sizeof(stop_path),dir,"STOP"))return fail(s,"service directory path too long");
    s->upgrade_fd=open(path,O_CREAT|O_RDWR|O_NOFOLLOW,0600);
    if(s->upgrade_fd<0||flock(s->upgrade_fd,LOCK_EX|LOCK_NB))return fail(s,"another launch is in progress; try again shortly");
    join(path,sizeof(path),dir,"service.lock");s->service_fd=open(path,O_CREAT|O_RDWR|O_NOFOLLOW,0600);
    if(s->service_fd<0)return fail(s,"cannot open service lock");
    if(flock(s->service_fd,LOCK_EX|LOCK_NB)){
        if(errno!=EWOULDBLOCK&&errno!=EAGAIN)return fail(s,"cannot acquire service lock");
        char record[192]={0},current[32];int pid;
        ssize_t n=pread(s->service_fd,record,sizeof(record)-1,0);if(n<0)return fail(s,"cannot read existing service record");
        /* Old releases held an empty lock file: their STOP path is supported. */
        if(n>0&&(sscanf(record,"PS5Trophies version=%31s pid=%d",current,&pid)!=2||pid<=0||same_or_newer(current,version)))return fail(s,"same/newer version or unknown service already running; no stop sent");
        int marker=open(stop_path,O_CREAT|O_EXCL|O_WRONLY|O_NOFOLLOW,0600);int owned=marker>=0;struct stat identity={0};
        if(!owned&&errno!=EEXIST)return fail(s,"cannot request previous version shutdown");
        if(owned&&fstat(marker,&identity)){close(marker);return fail(s,"cannot verify stop marker");}
        unsigned elapsed=0;int acquired=0;
        for(;;){
            if(!flock(s->service_fd,LOCK_EX|LOCK_NB)){acquired=1;break;}
            if((errno!=EWOULDBLOCK&&errno!=EAGAIN)||elapsed>=timeout_ms)break;
            usleep(50000);elapsed+=50;
        }
        if(owned){struct stat live;if(!lstat(stop_path,&live)&&live.st_dev==identity.st_dev&&live.st_ino==identity.st_ino)unlink(stop_path);close(marker);}
        if(!acquired)return fail(s,"previous version did not stop in time; old process retained");s->replaced=1;
    }
    s->owned=1;
    struct stat st;
    if(!lstat(stop_path,&st)){
        if(!S_ISREG(st.st_mode)||unlink(stop_path))return fail(s,"cannot clear previous STOP marker");
    }else if(errno!=ENOENT)return fail(s,"cannot inspect STOP marker");
    char record[192];int n=snprintf(record,sizeof(record),"PS5Trophies version=%s pid=%d\n",version,(int)getpid());
    if(ftruncate(s->service_fd,0)||pwrite(s->service_fd,record,(size_t)n,0)!=n||fsync(s->service_fd))return fail(s,"cannot record active version");
    return 0;
}
static char *regular(const char *path,size_t cap,size_t *n){
    *n=0;int fd=open(path,O_RDONLY|O_NOFOLLOW);if(fd<0)return NULL;struct stat st;
    if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_size<0||(unsigned long long)st.st_size>cap){close(fd);return NULL;}
    char *b=malloc((size_t)st.st_size+1);if(!b){close(fd);return NULL;}
    size_t done=0;while(done<(size_t)st.st_size){ssize_t r=read(fd,b+done,(size_t)st.st_size-done);if(r<=0){free(b);close(fd);return NULL;}done+=(size_t)r;}
    b[done]=0;close(fd);*n=done;return b;
}
static int filename_is(const char *json,const char *file){
    const char *p=strstr(json,"\"filename\"");if(!p)return 0;p+=10;while(isspace((unsigned char)*p))p++;if(*p++!=':')return 0;while(isspace((unsigned char)*p))p++;
    size_t n=strlen(file);return *p=='"'&&strlen(p)>=n+2&&!memcmp(p+1,file,n)&&p[n+1]=='"';
}
static int verified(const char *path,const char *version){
    size_t n;char *b=regular(path,4*1024*1024,&n);if(!b)return 0;
    char marker[100];snprintf(marker,sizeof(marker),"PS5Trophies ELF version=%s",version);size_t len=strlen(marker);int found=0;
    if(n>=64&&!memcmp(b,"\177ELF\2\1",6)&&(unsigned char)b[18]==62&&b[19]==0)for(size_t i=64;i+len<n;i++)if(!memcmp(b+i,marker,len)&&!b[i+len]){found=1;break;}
    free(b);return found;
}
void install_watch_init(InstallWatch *w,const char *root,const char *version){
    memset(w,0,sizeof(*w));unsigned matches=0;char filename[96];snprintf(filename,sizeof(filename),"ps5-trophies-v%s.elf",version);
    const char *kinds[]={"local","repo"};
    for(unsigned k=0;k<2;k++){
        char base[1200];snprintf(base,sizeof(base),"%s/data/pldmgr/payloads/%s",root,kinds[k]);DIR *d=opendir(base);if(!d)continue;struct dirent *e;
        while((e=readdir(d))){
            if(e->d_name[0]=='.')continue;char slot[1400],path[1600];struct stat st;
            if(join(slot,sizeof(slot),base,e->d_name)||lstat(slot,&st)||!S_ISDIR(st.st_mode))continue;
            if(join(path,sizeof(path),slot,"meta.json"))continue;size_t n;char *meta=regular(path,16384,&n);if(!meta)continue;
            int valid=filename_is(meta,filename);free(meta);if(!valid||join(path,sizeof(path),slot,filename)||strlen(path)>=sizeof(w->file)||!verified(path,version))continue;
            matches++;snprintf(w->file,sizeof(w->file),"%s",path);
        }
        closedir(d);
    }
    /* Ambiguous/missing installations must not stop an unrelated launch. */
    if(matches!=1)w->file[0]=0;
}
int install_watch_removed(InstallWatch *w){
    if(!w->file[0])return 0;struct timespec now;if(clock_gettime(CLOCK_MONOTONIC,&now))return 0;
    unsigned long long ms=(unsigned long long)now.tv_sec*1000+now.tv_nsec/1000000;
    if(w->checked_ms&&ms-w->checked_ms<500)return 0;w->checked_ms=ms;
    struct stat st;if(!lstat(w->file,&st)){w->missing=0;return 0;}
    if(errno!=ENOENT&&errno!=ENOTDIR){w->missing=0;return 0;}
    /* Called once per 500ms tick: tolerate a replacement transaction gap. */
    return ++w->missing>=6;
}
