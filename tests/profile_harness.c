#include "profile.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
static const char *mode;
static int calls;
int sceUserServiceInitialize(void *p){(void)p;return !strcmp(mode,"already")?-1:0;}
int sceUserServiceGetUserName(int uid,char *name,size_t size){
    calls++;
    if(uid!=0x10000001)return -2;
    if(!strcmp(mode,"slow"))usleep(700000);
    if(!strcmp(mode,"failure"))return -3;
    if(!strcmp(mode,"unterminated")){memset(name,'x',size);return 0;}
    snprintf(name,size,"测试用户 Player");return 0;
}
int main(int argc,char **argv){
    if(argc!=2)return 1;mode=argv[1];char name[128];
    profile_name("10000001",name,sizeof(name));puts(name);
    if(!strcmp(mode,"slow"))usleep(800000);
    profile_name("10000001",name,sizeof(name));puts(name);
    printf("calls=%d\n",calls);return 0;
}
