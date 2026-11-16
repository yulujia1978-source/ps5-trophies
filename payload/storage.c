/* SPDX-License-Identifier: GPL-3.0-or-later
 * Format references: PS5-Suite, commit 9aebf59fa756dd2b7e86987debf1e72520a3c43d.
 * Conservative reader: only matched definition/state rows, no bitmask guessing.
 */
#include "storage.h"
#include "profile.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
const char *storage_root="";
static uint32_t be32(const unsigned char *p){return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
static uint64_t be64(const unsigned char *p){return (uint64_t)be32(p)<<32|be32(p+4);}
int valid_uid(const char *s){size_t n=strlen(s);if(!n||n>8)return 0;for(size_t i=0;i<n;i++)if(!isxdigit((unsigned char)s[i]))return 0;return 1;}
int valid_set(const char *s){if(strlen(s)!=12||strncmp(s,"NPWR",4)||s[9]!='_')return 0;for(int i=4;i<12;i++)if(i!=9&&!isdigit((unsigned char)s[i]))return 0;return 1;}
unsigned char *read_file(const char *path,size_t limit,size_t *size){
    *size=0;FILE *f=fopen(path,"rb");if(!f)return NULL;
    struct stat before,after;
    if(fstat(fileno(f),&before)||!S_ISREG(before.st_mode)||before.st_size<=0||(uint64_t)before.st_size>limit){fclose(f);return NULL;}
    size_t n=(size_t)before.st_size;unsigned char *p=malloc(n+1);if(!p){fclose(f);return NULL;}
    int ok=fread(p,1,n,f)==n&&!fstat(fileno(f),&after)&&before.st_size==after.st_size&&before.st_mtime==after.st_mtime;
    fclose(f);if(!ok){free(p);return NULL;}p[n]=0;*size=n;return p;
}
static unsigned char *archive_read(const char *set,const char *entry,int icon_variants,size_t *size){
    *size=0;if(!valid_set(set))return NULL;
    char path[1024];snprintf(path,sizeof(path),"%s/user/trophy2/nobackup/conf/%s/TROPHY.UCP",storage_root,set);
    FILE *f=fopen(path,"rb");if(!f)return NULL;
    struct stat st;unsigned char h[64];unsigned char *out=NULL;
    if(fstat(fileno(f),&st)||st.st_size<64||fread(h,1,64,f)!=64||be32(h)!=0xb228c60a)goto done;
    uint32_t count=be32(h+16),toc=be32(h+20);
    if(count>4096||toc<64||(uint64_t)toc+32+(uint64_t)count*64>(uint64_t)st.st_size)goto done;
    for(uint32_t i=0;i<count;i++){
        unsigned char e[64];if(fseek(f,(long)((uint64_t)toc+32+(uint64_t)i*64),SEEK_SET)||fread(e,1,64,f)!=64)break;
        char name[33];memcpy(name,e,32);name[32]=0;
        if(icon_variants){size_t l=strlen(name);if(l<9||strncasecmp(name,"icon0",5)||(name[5]!='_'&&name[5]!='.')||strcasecmp(name+l-4,".png"))continue;}
        else if(strcmp(name,entry))continue;
        uint64_t off=be64(e+32),len=be64(e+40);
        if(!len||len>16*1024*1024||off>(uint64_t)st.st_size||len>(uint64_t)st.st_size-off){if(icon_variants)continue;break;}
        out=malloc((size_t)len+1);if(!out)break;
        if(fseek(f,(long)off,SEEK_SET)||fread(out,1,(size_t)len,f)!=len){free(out);out=NULL;break;}
        out[len]=0;
        if(icon_variants&&(len<8||memcmp(out,"\x89PNG\r\n\x1a\n",8))){free(out);out=NULL;continue;}
        *size=(size_t)len;break;
    }
 done:fclose(f);return out;
}
unsigned char *archive_entry(const char *set,const char *entry,size_t *size){return archive_read(set,entry,0,size);}

/* Bind NPWR IDs to title IDs using actual npbind.dat contents, not guesses
 * from game names. Cache only this small mapping, never trophy state. */
#define ICON_MAP_MAX 512
typedef struct {char set[13],title[10];} IconMap;
static IconMap icon_map[ICON_MAP_MAX];
static size_t icon_map_count;
static time_t icon_map_checked;
static int valid_title(const char *s){
    if(strlen(s)!=9||(strncmp(s,"PPSA",4)&&strncmp(s,"CUSA",4)))return 0;
    for(int i=4;i<9;i++)if(!isdigit((unsigned char)s[i]))return 0;return 1;
}
static void refresh_icon_map(void){
    time_t now=time(NULL);if(icon_map_checked&&now-icon_map_checked<30)return;
    icon_map_checked=now;icon_map_count=0;
    const char *roots[]={"/system_data/priv/appmeta","/user/appmeta","/user/app"};
    for(size_t r=0;r<3;r++){
        char base[1024];snprintf(base,sizeof(base),"%s%s",storage_root,roots[r]);DIR *dir=opendir(base);if(!dir)continue;
        struct dirent *e;
        while((e=readdir(dir))&&icon_map_count<ICON_MAP_MAX){
            if(!valid_title(e->d_name))continue;char path[1200];
            snprintf(path,sizeof(path),"%s/%s/%strophy2/npbind.dat",base,e->d_name,r==2?"sce_sys/":"");
            size_t n;unsigned char *data=read_file(path,1024*1024,&n);if(!data)continue;
            for(size_t i=0;i+12<=n&&icon_map_count<ICON_MAP_MAX;i++){
                if(memcmp(data+i,"NPWR",4))continue;char set[13];memcpy(set,data+i,12);set[12]=0;if(!valid_set(set))continue;
                int duplicate=0;for(size_t j=0;j<icon_map_count;j++)if(!strcmp(icon_map[j].set,set)&&!strcmp(icon_map[j].title,e->d_name)){duplicate=1;break;}
                if(duplicate)continue;
                memcpy(icon_map[icon_map_count].set,set,13);memcpy(icon_map[icon_map_count].title,e->d_name,10);icon_map_count++;
            }
            free(data);
        }
        closedir(dir);
    }
}
static int png(const unsigned char *p,size_t n){return p&&n>=8&&!memcmp(p,"\x89PNG\r\n\x1a\n",8);}
static void icon_diagnostic(const char *set,const char *source){
#ifndef HOST_BUILD
    static char logged[ICON_MAP_MAX][13];static size_t count;
    for(size_t i=0;i<count;i++)if(!strcmp(logged[i],set))return;
    if(count>=ICON_MAP_MAX)return;memcpy(logged[count++],set,13);
    FILE *f=fopen("/data/ps5-trophies/icons.log","a");if(f){fprintf(f,"%s source=%s\n",set,source);fclose(f);}
#else
    (void)set;(void)source;
#endif
}
unsigned char *game_icon(const char *set,size_t *size){
    *size=0;if(!valid_set(set))return NULL;
    const char *entries[]={"icon0.png","icon0_zh-Hans.png","icon0_zh-CN.png","icon0_zh-Hant.png","icon0_zh-TW.png","icon0_en-US.png"};
    for(size_t i=0;i<sizeof(entries)/sizeof(*entries);i++){
        unsigned char *data=archive_entry(set,entries[i],size);
        if(png(data,*size)){icon_diagnostic(set,entries[i]);return data;}free(data);
    }
    unsigned char *data=archive_read(set,NULL,1,size);
    if(data){icon_diagnostic(set,"localized-archive");return data;}
    refresh_icon_map();
    const char *roots[]={"/system_data/priv/appmeta","/user/appmeta","/user/app"};
    for(size_t i=0;i<icon_map_count;i++)if(!strcmp(icon_map[i].set,set)){
        for(size_t r=0;r<3;r++){
            char path[1200];snprintf(path,sizeof(path),"%s%s/%s/%sicon0.png",storage_root,roots[r],icon_map[i].title,r==2?"sce_sys/":"");
            data=read_file(path,16*1024*1024,size);
            if(png(data,*size)){icon_diagnostic(set,roots[r]);return data;}free(data);
        }
    }
    *size=0;icon_diagnostic(set,"unavailable");return NULL;
}
/* Small checked JSON builder: all externally supplied text is an escaped string. */
typedef struct{char *p;size_t n,cap;int failed;} Json;
static void add(Json *j,const char *fmt,...){
    if(j->failed)return;
    va_list ap,cp;va_start(ap,fmt);va_copy(cp,ap);int n=vsnprintf(NULL,0,fmt,cp);va_end(cp);
    if(n<0||j->n+(size_t)n>16*1024*1024){j->failed=1;va_end(ap);return;}
    size_t need=j->n+(size_t)n+1;if(need>j->cap){size_t cap=need*2;char *p=realloc(j->p,cap);if(!p){j->failed=1;va_end(ap);return;}j->p=p;j->cap=cap;}
    vsnprintf(j->p+j->n,j->cap-j->n,fmt,ap);j->n+=(size_t)n;va_end(ap);
}
static void str(Json *j,const char *s){add(j,"\"");for(const unsigned char *p=(const unsigned char*)s;*p;p++){if(*p=='"'||*p=='\\')add(j,"\\%c",*p);else if(*p<32)add(j,"\\u%04x",*p);else add(j,"%c",*p);}add(j,"\"");}
static char *finish(Json *j){if(j->failed){free(j->p);return NULL;}return j->p;}
typedef struct{unsigned char defined,known,earned,conflict;uint64_t timestamp;} Row;
static Row *load_state(const char *uid,const char *set,int *total,int *earned,int *unknown){
    *total=*earned=*unknown=0;char path[1024];size_t n=0;
    snprintf(path,sizeof(path),"%s/user/home/%s/trophy2/nobackup/data/%s/TRPTITLE.DAT",storage_root,uid,set);
    unsigned char *d=read_file(path,1024*1024,&n);if(!d)return NULL;
    /* Reject a moving snapshot rather than mixing rows from an update. */
    size_t check_n=0;unsigned char *check=read_file(path,1024*1024,&check_n);
    int stable=check&&check_n==n&&!memcmp(d,check,n);free(check);
    if(!stable||n<0x200||memcmp(d,"T2PD",4)){free(d);return NULL;}
    Row *rows=calloc(TROPHY_MAX,sizeof(Row));if(!rows){free(d);return NULL;}
    for(size_t o=0x200;o+0xd0<=n;o+=4)if(be32(d+o)==0x500&&be32(d+o+4)==0xc0){uint32_t id=be32(d+o+16);if(id<TROPHY_MAX)rows[id].defined=1;}
    for(size_t o=0x800;o+0x60<=n;o+=4)if(be32(d+o)==0x800&&be32(d+o+4)==0x50){
        uint32_t id=be32(d+o+16);if(id>=TROPHY_MAX||!rows[id].defined)continue;
        Row *r=&rows[id];unsigned earnedFlag=be32(d+o+20)&1;uint64_t stamp=earnedFlag?be64(d+o+32):0;
        if(r->known&&(r->earned!=earnedFlag||r->timestamp!=stamp))r->conflict=1;
        r->known=1;r->earned=(unsigned char)earnedFlag;r->timestamp=stamp;
    }
    free(d);
    for(int i=0;i<TROPHY_MAX;i++)if(rows[i].defined){(*total)++;if(rows[i].conflict)rows[i].known=0;if(rows[i].known)*earned+=rows[i].earned;else(*unknown)++;}
    if(!*total){free(rows);return NULL;}return rows;
}
char *state_json(const char *uid,const char *set){
    if(!valid_uid(uid)||!valid_set(set))return NULL;
    int total,earned,unknown;Row *rows=load_state(uid,set,&total,&earned,&unknown);if(!rows)return NULL;
    Json j={0};add(&j,"{\"total\":%d,\"earned\":%d,\"unknown\":%d,\"rows\":[",total,earned,unknown);int comma=0;
    for(int i=0;i<TROPHY_MAX;i++)if(rows[i].defined){Row *r=&rows[i];add(&j,"%s{\"id\":%d,\"unlocked\":%s,\"timestampUs\":\"%llu\"}",comma++?",":"",i,r->known?(r->earned?"true":"false"):"null",(unsigned long long)r->timestamp);}
    add(&j,"]}");free(rows);return finish(&j);
}
char *catalog_json(void){
    Json j={0};char home[1024];snprintf(home,sizeof(home),"%s/user/home",storage_root);
    DIR *hd=opendir(home);if(!hd)return NULL;
#ifdef HOST_BUILD
    const char *source="fixture";
#else
    const char *source="console";
#endif
    add(&j,"{\"version\":\"0.2.3\",\"source\":\"%s\",\"platform\":\"PS5\",\"users\":[",source);
    struct dirent *u;int uc=0;
    while((u=readdir(hd))){if(!valid_uid(u->d_name))continue;
        char base[1200];snprintf(base,sizeof(base),"%s/%s/trophy2/nobackup/data",home,u->d_name);
        errno=0;DIR *sets=opendir(base);int err=errno;
        add(&j,"%s{\"id\":",uc++?",":"");str(&j,u->d_name);add(&j,",\"name\":");char name[128];profile_name(u->d_name,name,sizeof(name));str(&j,name);
        add(&j,",\"error\":");if(!sets&&err!=ENOENT)str(&j,strerror(err));else add(&j,"null");
        add(&j,",\"sets\":[");int sc=0;struct dirent *s;
        while(sets&&(s=readdir(sets))){if(!valid_set(s->d_name))continue;
            int total,earned,unknown;Row *rows=load_state(u->d_name,s->d_name,&total,&earned,&unknown);
            add(&j,"%s{\"id\":",sc++?",":"");str(&j,s->d_name);
            add(&j,",\"earned\":%d,\"total\":%d,\"unknown\":%d,\"readable\":%s}",earned,total,unknown,rows?"true":"false");free(rows);
        }
        if(sets)closedir(sets);add(&j,"]}");
    }
    closedir(hd);add(&j,"]}");return finish(&j);
}
