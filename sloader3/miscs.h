#ifndef SEGMENT_MISCS_H
#define SEGMENT_MISCS_H
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h> 

//miscs
int ReplaceString(char* source,const char* replace_from,const char* replace_to);
int create_dir_r(const char* output_file);
int create_dir_r2(const char* path);
int WriteFile(const char* filename,const char* buf,int bufsize);
int write_textfile(const char* filename,const char* contenxt);
int AppendFile(const char* filename,const char* buf,int bufsize);
int ReadFile(const char* filename,char* buf,int max_bufsize);
int DeleteFile(const char* filename);
int IsFileExist(const char* filename);
int GetFileSize(const char* filename);
int GetTempFileName(const char* root,char* filename);
int get_url_para(const char* url,const char* key,char* val,int val_buf_len);

int write_pid_file(const char* filename);

int CreateDir(const char* dir);

#endif