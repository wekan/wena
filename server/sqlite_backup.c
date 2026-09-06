#include "sqlite_backup.h"
#include "sha256.h"
#include "sqlite_storage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(_WIN32)
#include <sys/statvfs.h>
#endif

#define WENA_BACKUP_PATH_MAX 1024
#define WENA_BACKUP_MARGIN 1048576ul

static int scalar(sqlite3 *db,const char *sql,unsigned long *value)
{
    sqlite3_stmt *s;int ok;
    if(sqlite3_prepare_v2(db,sql,-1,&s,NULL)!=SQLITE_OK)return 0;
    ok=sqlite3_step(s)==SQLITE_ROW&&sqlite3_column_int64(s,0)>=0;
    if(ok)*value=(unsigned long)sqlite3_column_int64(s,0);
    sqlite3_finalize(s);return ok;
}

static int exists(const char *path)
{FILE *f=fopen(path,"rb");if(!f)return 0;fclose(f);return 1;}

static int file_hash(const char *path,char output[65])
{
    FILE *f;unsigned char data[16384];size_t n;WenaSha256 sha;
    f=fopen(path,"rb");if(!f)return 0;wena_sha256_init(&sha);
    while((n=fread(data,1,sizeof(data),f))>0)wena_sha256_update(&sha,data,n);
    if(ferror(f)){fclose(f);return 0;}if(fclose(f)!=0)return 0;
    wena_sha256_final_hex(&sha,output);return 1;
}

int wena_sqlite_backup_posix_free_space(void *context,const char *path,unsigned long *bytes)
{
    (void)context;(void)path;
#if defined(_WIN32)
    (void)bytes;return 0;
#else
    {
        struct statvfs value;
        if(!bytes||statvfs(path,&value)!=0)return 0;
        if(value.f_bavail!=0&&value.f_frsize>((unsigned long)-1)/value.f_bavail)
            *bytes=(unsigned long)-1;
        else *bytes=(unsigned long)(value.f_bavail*value.f_frsize);
        return 1;
    }
#endif
}

int wena_sqlite_backup_create(sqlite3 *source,const char *path,
                              WenaBackupFreeSpace free_space,void *context)
{
    char temporary[WENA_BACKUP_PATH_MAX],sidecar[WENA_BACKUP_PATH_MAX];
    char side_temp[WENA_BACKUP_PATH_MAX],hash[65];
    unsigned long pages,page_size,available,needed;sqlite3 *copy=NULL;
    sqlite3_backup *backup=NULL;FILE *sum=NULL;int step,ok=0;
    if(!source||!path||!free_space||path[0]!='/'||strlen(path)>WENA_BACKUP_PATH_MAX-16u)return 0;
    sprintf(temporary,"%s.tmp",path);sprintf(sidecar,"%s.sha256",path);
    sprintf(side_temp,"%s.sha256.tmp",path);
    if(exists(path)||exists(sidecar)||exists(temporary)||exists(side_temp))return 0;
    if(!wena_sqlite_integrity(source)||!scalar(source,"PRAGMA page_count",&pages)||
       !scalar(source,"PRAGMA page_size",&page_size))return 0;
    if(pages&&page_size>((unsigned long)-1)/pages)return 0;
    needed=pages*page_size;
    if(needed>(unsigned long)-1-WENA_BACKUP_MARGIN)return 0;
    needed+=WENA_BACKUP_MARGIN;
    if(!free_space(context,path,&available)||available<needed)return 0;
    if(sqlite3_open_v2(temporary,&copy,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_FULLMUTEX,NULL)!=SQLITE_OK)goto done;
    backup=sqlite3_backup_init(copy,"main",source,"main");if(!backup)goto done;
    step=sqlite3_backup_step(backup,-1);if(sqlite3_backup_finish(backup)!=SQLITE_OK||step!=SQLITE_DONE)goto done;backup=NULL;
    if(!wena_sqlite_integrity(copy)||!scalar(copy,"PRAGMA user_version",&pages)||pages!=1ul)goto done;
    if(sqlite3_close(copy)!=SQLITE_OK){copy=NULL;goto done;}copy=NULL;
    if(!file_hash(temporary,hash))goto done;
    sum=fopen(side_temp,"wb");if(!sum)goto done;
    if(fprintf(sum,"%s\n",hash)!=65||fflush(sum)!=0||fclose(sum)!=0){sum=NULL;goto done;}sum=NULL;
    if(rename(temporary,path)!=0)goto done;
    if(rename(side_temp,sidecar)!=0){remove(path);goto done;}
    ok=1;
done:
    if(backup)sqlite3_backup_finish(backup);
    if(copy)sqlite3_close(copy);
    if(sum)fclose(sum);
    if(!ok){remove(temporary);remove(side_temp);}return ok;
}
