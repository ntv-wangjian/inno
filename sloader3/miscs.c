#include "miscs.h"
//miscs

int WriteFile(const char *filename,const char* buf,int bufsize)
{
	FILE	*fp = NULL;
	if(NULL == filename || NULL == buf) {
		return -1;
	}
	fp = fopen(filename,"w");
	if(NULL == fp) {
		return -1;
	}
	fwrite(buf,1,bufsize,fp);
	fclose(fp);
	return 0;
}

int AppendFile(const char* filename,const char* buf,int bufsize)
{
	int	    ret = 0;
	FILE	*fp = NULL;
	if(NULL == filename || NULL == buf) {
		return -1;
	}
	fp = fopen(filename,"a+");
	if(NULL == fp) {
		return -1;
	}
	ret = fwrite(buf,1,bufsize,fp);
	fclose(fp);
	return 0;
}

int ReadFile(const char* filename,char* buf,int max_bufsize)
{
	int      ret = 0;
	FILE	*fp = NULL;
	if(NULL == filename || NULL == buf) {
		return -1;
	}
	if(access(filename,0) != 0) {
		return -1;
	}
	fp = fopen(filename,"r");
	if(NULL == fp) {
		return -1;
	}
	ret = fread(buf,1,max_bufsize,fp);
	fclose(fp);
	return ret;
}

int DeleteFile(const char* filename)
{
	int	ret = 0;
	if(NULL == filename) {
		return -1;
	}
	if(access(filename,0) != 0) {
		return 0;
	}
	ret = remove(filename);
	if(ret == 0) {
		return 0;
	}
	else {
		return -1;
	}
}

int IsFileExist(const char* filename)
{
	if(!filename || access(filename,0)!=0)
	{
		return 0;
	}
	return 1;
}

int GetFileSize(const char* filename)
{
	int size = 0;
	FILE *fp = NULL;
	if(NULL == filename) {
		return -1;
	}
	if(access(filename,0) != 0) {
		return -1;
	}
	fp = fopen(filename,"rb");
	if(fp) {
		fseek(fp,0,SEEK_END);
		size = ftell(fp);
		fclose(fp);
	}	
	return size;
}

int GetTempFileName(const char* RootDir,char* filename)
{
	int	fp = 0;
	char	fname[256]={0};
	if(NULL == RootDir || NULL  == filename) 
	{
		return -1;
	}
	
	sprintf(fname,"%s/%s",RootDir,filename);
	fp = mkstemp(fname);
	unlink(fname);
	return 0;
}

int CreateDir(const char* dir){
	return mkdir(dir, 0755);
}

int create_dir_r2(const char* path)
{
	char cmd[512];
    if(access(path,0)!=0)
    {
        sprintf(cmd,"mkdir -p %s",path);
        return system(cmd);
    }

    return 0;
}

int create_dir_r(const char* output_file)
{
    char tmp[256];
    strcpy(tmp,output_file);
    strrchr(tmp,'/')[1] = 0;

    return create_dir_r2(tmp);
}

int ReplaceString(char* source,const char* replace_from,const char* replace_to)
{
    char tmp[512];
    char* pos  = NULL;
    strcpy(tmp,source);
    pos = strstr(tmp,replace_from);
    if(pos)
    {
        pos[0] = 0;
        strcpy(source,tmp);
        strcat(source,replace_to);
        pos = pos + strlen(replace_from);
        strcat(source,pos);
        return 0;
    }

    return 1;
}

int write_textfile(const char* filename,const char* contenxt)
{
	FILE* fp = fopen(filename,"w");
    if(fp==NULL) 
    {
        return -1;
    }
	fwrite(contenxt,1,strlen(contenxt),fp);
    fclose(fp);
	return 0;
}

int get_url_para(const char* url,const char* key,char* val,int val_buf_len)
{
	const char  *tmp;
	const char  *end;
	char  vkey[32];
	if(!url || strlen(key)>=30){
		return -1;
	}
	sprintf(vkey,"%s=",key);
	tmp = strstr(url,vkey);
	if(!tmp){
		return -1;
	}
	tmp+=strlen(vkey);
	end = tmp;
	while(*end!='&' && *end!='\0') end++;
	if(end-tmp<1 || end-tmp>=val_buf_len){
		return -1;
	}
	strncpy(val,tmp,end-tmp);
	val[end-tmp] = 0;
	return 0;
}

int write_pid_file(const char* filename)
{
	char pidt[16];
	int  pidi = getpid();
	sprintf(pidt,"%d",pidi);
	return write_textfile(filename,pidt);
}
