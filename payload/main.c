/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "storage.h"
#include "lifecycle.h"
#include "assets.h"
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
static volatile sig_atomic_t quitting;
static char session_id[96];
__attribute__((used)) static const char elf_identity[]="PS5Trophies ELF version=0.2.3";
static void stop(int sig){quitting=sig;}
static void startup_status(const char *stage,int error_number){
#ifndef HOST_BUILD
    FILE *log=fopen("/data/ps5-trophies/startup.txt","w");
    if(log){fprintf(log,"PS5 Trophies 0.2.3 pid=%d: %s (errno=%d)\n",(int)getpid(),stage,error_number);fclose(log);}
#else
    fprintf(stderr,"%s (errno=%d)\n",stage,error_number);
#endif
}
static long long milliseconds(void){struct timeval t;gettimeofday(&t,NULL);return (long long)t.tv_sec*1000+t.tv_usec/1000;}
static int ready(int fd,int writing,long long deadline){
    while(!quitting){long long ms=deadline-milliseconds();if(ms<=0)return 0;struct timeval tv={ms/1000,(ms%1000)*1000};fd_set fds;FD_ZERO(&fds);FD_SET(fd,&fds);int n=select(fd+1,writing?NULL:&fds,writing?&fds:NULL,NULL,&tv);if(n<0&&errno==EINTR)continue;return n>0;}return 0;
}
static int send_all(int fd,const void *data,size_t n,long long deadline){const unsigned char *p=data;while(n){if(!ready(fd,1,deadline))return -1;ssize_t got=send(fd,p,n,0);if(got<0&&(errno==EAGAIN||errno==EINTR))continue;if(got<=0)return -1;p+=got;n-=(size_t)got;}return 0;}
static void reply(int fd,int status,const char *type,const void *data,size_t n){
    char h[1024];int len=snprintf(h,sizeof(h),"HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nX-Frame-Options: DENY\r\nContent-Security-Policy: default-src 'self'; img-src 'self' data:; style-src 'self' 'unsafe-inline'; script-src 'self'; connect-src 'self'; frame-ancestors 'none'\r\n\r\n",status,status==200?"OK":"Error",type,n);
    long long deadline=milliseconds()+5000;if(!send_all(fd,h,(size_t)len,deadline))send_all(fd,data,n,deadline);
}
static void error(int fd,int code,const char *message){char body[256];snprintf(body,sizeof(body),"{\"error\":\"%s\"}",message);reply(fd,code,"application/json; charset=utf-8",body,strlen(body));}
static void route(int fd,char *path){
    char *query=strchr(path,'?');if(query)*query=0;
    if(!strcmp(path,"/"))path="/index.html";
    for(size_t i=0;i<sizeof(assets)/sizeof(*assets);i++)if(!strcmp(path,assets[i].path)){reply(fd,200,assets[i].type,assets[i].data,assets[i].size);return;}
    if(!strcmp(path,"/api/status")){char body[192];snprintf(body,sizeof(body),"{\"version\":\"0.2.3\",\"session\":\"%s\"}",session_id);reply(fd,200,"application/json; charset=utf-8",body,strlen(body));return;}
    if(!strcmp(path,"/api/catalog")){char *j=catalog_json();if(!j){error(fd,503,"Cannot read /user/home; check payload permissions");return;}reply(fd,200,"application/json; charset=utf-8",j,strlen(j));free(j);return;}
    /* Strict positional route: no filesystem paths accepted from browsers. */
    char uid[16],set[24],kind[32],extra;int used=0;
    if(sscanf(path,"/api/set/%15[^/]/%23[^/]/%31[^/]%n%c",uid,set,kind,&used,&extra)!=3||used!=(int)strlen(path)||!valid_uid(uid)||!valid_set(set)){error(fd,404,"Unknown route");return;}
    if(!strcmp(kind,"state")){char *j=state_json(uid,set);if(!j){error(fd,422,"Unsupported, changed or unreadable trophy state");return;}reply(fd,200,"application/json; charset=utf-8",j,strlen(j));free(j);return;}
    const char *entry=NULL,*mime="application/json; charset=utf-8";
    if(!strcmp(kind,"conf"))entry="tropconf.json";
    else if(!strcmp(kind,"meta")){
        const char *langs[]={"tropmeta_zh-Hans.json","tropmeta_zh-CN.json","tropmeta_zh-Hant.json","tropmeta_zh-TW.json","tropmeta_en-US.json","tropmeta_ja-JP.json"};
        for(size_t i=0;i<sizeof(langs)/sizeof(*langs);i++){size_t n;unsigned char *p=archive_entry(set,langs[i],&n);if(p){reply(fd,200,mime,p,n);free(p);return;}}
        error(fd,404,"No supported metadata language in archive");return;
    }else{
        unsigned icon;int n=0;
        if(!strcmp(kind,"icon")){
            size_t size;unsigned char *data=game_icon(set,&size);
            if(!data){error(fd,404,"No local game icon found");return;}
            reply(fd,200,"image/png",data,size);free(data);return;
        }
        else if(sscanf(kind,"trop%4u.png%n",&icon,&n)==1&&n==(int)strlen(kind)&&strlen(kind)==12&&icon<TROPHY_MAX)entry=kind;
        else{error(fd,404,"Unknown asset");return;}
        mime="image/png";
    }
    size_t n;unsigned char *p=archive_entry(set,entry,&n);if(!p){error(fd,404,"Archive entry unavailable");return;}
    if(!strcmp(mime,"image/png")&&(n<8||memcmp(p,"\x89PNG\r\n\x1a\n",8))){free(p);error(fd,422,"Invalid PNG");return;}
    reply(fd,200,mime,p,n);free(p);
}
int main(int argc,char **argv){
    int port=6082;
#ifdef HOST_BUILD
    if(argc!=3){fprintf(stderr,"usage: trophy-server ROOT PORT\n");return 2;}storage_root=argv[1];port=atoi(argv[2]);if(port<1||port>65535)return 2;
#else
    (void)argc;(void)argv;
#endif
    signal(SIGPIPE,SIG_IGN);signal(SIGTERM,stop);signal(SIGINT,stop);
    char directory[1200],stop_file[1400];
    int length=snprintf(directory,sizeof(directory),"%s/data",storage_root);if(length<0||(size_t)length>=sizeof(directory))return 1;mkdir(directory,0777);
    length=snprintf(directory,sizeof(directory),"%s/data/ps5-trophies",storage_root);if(length<0||(size_t)length>=sizeof(directory))return 1;
    if(mkdir(directory,0777)&&errno!=EEXIST){startup_status("cannot create service directory",errno);return 1;}
    unsigned timeout=15000;
#ifdef HOST_BUILD
    const char *test_timeout=getenv("TROPHY_TEST_WAIT_MS");if(test_timeout){unsigned n=(unsigned)strtoul(test_timeout,NULL,10);if(n>=100&&n<=15000)timeout=n;}
#endif
    Instance instance;
    if(instance_acquire(&instance,directory,"0.2.3",timeout)){startup_status(instance.error,errno);return 1;}
    startup_status(instance.replaced?"previous version stopped; starting replacement":"starting",0);
    snprintf(stop_file,sizeof(stop_file),"%s/STOP",directory);
    snprintf(session_id,sizeof(session_id),"%d-%lld",(int)getpid(),milliseconds());
    InstallWatch watch;install_watch_init(&watch,storage_root,"0.2.3");
    int listener=socket(AF_INET,SOCK_STREAM,0);if(listener<0){startup_status("socket failed",errno);instance_close(&instance);return 1;}int one=1;setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one));
    struct sockaddr_in addr;memset(&addr,0,sizeof(addr));addr.sin_family=AF_INET;addr.sin_port=htons((unsigned short)port);
#ifdef HOST_BUILD
    addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
#else
    addr.sin_addr.s_addr=htonl(INADDR_ANY);
#endif
    if(bind(listener,(struct sockaddr*)&addr,sizeof(addr))||listen(listener,8)||fcntl(listener,F_SETFL,O_NONBLOCK)<0){startup_status("cannot listen on port 6082",errno);close(listener);instance_close(&instance);return 1;}
#ifndef HOST_BUILD
    FILE *log=fopen("/data/ps5-trophies/startup.txt","w");if(log){fprintf(log,"PS5 Trophies 0.2.3 pid=%d listening port=%d watch=%s replaced=%d\n",(int)getpid(),port,watch.file[0]?"enabled":"unavailable",instance.replaced);fclose(log);}
#endif
    printf("PS5 Trophies 0.2.3 http://127.0.0.1:%d\n",port);fflush(stdout);
    instance_ready(&instance);
    const char *exit_reason="stopped";
    while(!quitting){
        struct stat st;if(!stat(stop_file,&st)){exit_reason="stopped by STOP request or upgrade";break;}
        if(install_watch_removed(&watch)){exit_reason="installed ELF deleted; web service stopped";break;}
        if(!ready(listener,0,milliseconds()+500))continue;
        int fd=accept(listener,NULL,NULL);if(fd<0)continue;if(fd>=FD_SETSIZE||fcntl(fd,F_SETFL,O_NONBLOCK)<0){close(fd);continue;}
        char req[4096];size_t used=0;long long deadline=milliseconds()+2000;int complete=0;
        while(used<sizeof(req)-1&&ready(fd,0,deadline)){
            ssize_t n=recv(fd,req+used,sizeof(req)-1-used,0);if(n<0&&(errno==EAGAIN||errno==EINTR))continue;if(n<=0)break;if(memchr(req+used,0,(size_t)n))break;used+=(size_t)n;req[used]=0;if(strstr(req,"\r\n\r\n")){complete=1;break;}
        }
        char method[8],path[256],version[16];if(!complete||sscanf(req,"%7s %255s %15s",method,path,version)!=3)error(fd,400,"Invalid request");
        else if(strcmp(method,"GET"))error(fd,405,"Read-only service: GET required");
        else if(strcmp(version,"HTTP/1.1")&&strcmp(version,"HTTP/1.0"))error(fd,400,"Invalid HTTP version");
        else route(fd,path);
        close(fd);
    }
    startup_status(exit_reason,0);close(listener);instance_close(&instance);return 0;
}
