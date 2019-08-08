/*
* ffmpeg扩展辅助方法
* by WJ @ 云视睿博
* 2018-12-25 create
* 第二个参数必须是 -ntv_info
* 第三个参数必须是信息输出的文件名
* 命令执行例子：   exe -ntv_info myinfo.log -i ...
* 程序会输出info文件和pid文件。 pid文件名是info文件名加上.pid后缀。
* 删除pid文件触发程序主动退出
* 以下函数请写成static函数，否者需要明确声明
*/
#include <string.h>
#include "../ntv/ntv_define.h"

#define NTV_PROGRESSINT -100

//信息文件输出路径，该文件中输出转码的过程信息和结果。
static char ntv_pid_filename[256]={0};
static char ntv_err_filename[256]={0};
static char ntv_time[32]={0},ntv_bitrate[32]={0},ntv_speed[32]={0},ntv_duration[32]={0};
static void ffmpeg_cleanup(int ret);

static int write_file(const char* filename,const char* txt){
	int  ret = 0;
	FILE* fp = fopen(filename,"w");
	if(NULL == fp) {
		return 0;
	}
	ret = fwrite(txt,1,strlen(txt),fp);
	fclose(fp);
	return ret;
}

static int file_exists(const char* filename)
{
	if(!filename || access(filename,0)!=0)
	{
		return 0;
	}
	return 1;
}

/**
* 根据创建完整目录
*/
static int create_full_dir(const char* path)
{
	char cmd[512];
    if(access(path,0)!=0)
    {
		printf("create dir: %s\n",path);
        sprintf(cmd,"mkdir -p %s",path);
        return system(cmd);
    }

	return 0;
}

/**
* 根据文件名创建所在目录
*/
static int create_file_dir(const char* filename)
{
	static char path[512];
	char *pos = NULL;
	if(!filename){
		return 1;
	}
	strcpy(path,filename);
	pos = strrchr(path,'/');
	if(pos){
		pos[0] = '\0';
		return create_full_dir(path);
	}else{
		return 1;
	}
}



/*
* 从Log里分析出进度信息。
* 该函数取某个字符后，到下一个空格之间的内容。
* frame= 2062 fps=187 q=-1.0 Lsize=    3164kB time=00:01:27.14 bitrate= 297.4kbits/s dup=1 drop=0 speed=7.91x
*/
static const char* ntv_progress_info(const char* log,const char* key,char* buf/*,int buflen=32*/){
	static char skey[32];
	const char* p;
	const char* pend;
	sprintf(skey,"%s",key);
	if(p=strstr(log,skey)){
		p+=strlen(skey);
		while(*p==' ' || *p=='\t'){
			p++;
		}
		if(*p=='\0') return NULL;

		pend = p;
		while(*pend!=' ' && *pend!='\t' && *pend!='\0'){
			pend++;
		}
		if(pend-p>=32) return NULL;
		strncpy(buf,p,pend-p);
		buf[pend-p] = '\0';
		return buf;
	}else{
		return NULL;
	}
}

/*
* format = 0 json
* format = 1 key=value line
* exit = -100 表示正在转码
*/
static void ntv_output_info(int exit,int format){
	static char info[256];
	static int start   = 0;
	static int end     = 0;
	static int last    = 0;
	static pid_t pid   = 0;
	
	if(pid==0){
		pid   = getpid();
	}

	if(start==0){
		start = time(NULL);
	}
	
	//2秒更新一次
	end = time(NULL);
	if(exit==NTV_PROGRESSINT && end-last<2){
		return;
	}
	last = end;

	if(format==0){
		sprintf(info,"{\"pid\":%d,\"progress\":\"%s\",\"duration\":\"%s\",\"bitrate\":%.1f,\"speed\":%.1f,\"start\":%d,\"update\":%d,\"status\":%d}"
		         ,pid,ntv_time,ntv_duration,atof(ntv_bitrate),atof(ntv_speed),start,end,exit);
	}else{
		sprintf(info,
"pid=%d\n\
progress=%s\n\
duration=%s\n\
bitrate=%.1f\n\
speed=%.1f\n\
start=%d\n\
update=%d\nstatus=%d",pid,ntv_time,ntv_duration,atof(ntv_bitrate),atof(ntv_speed),start,end,exit);
	}

	printf("%s - %s\n",ntv_time,ntv_duration);
	
	write_file(ntv_info_filename,info);
}

/*
#define AV_LOG_ERROR    16  //can go on
#define AV_LOG_FATAL     8  //will exit
*/
static void log_callback_ntv(void *ptr, int level, const char *fmt, va_list vl)
{
	//How about Multi-Thread??
	static char buffer[5000];
	static int  hasDuration = 0;
	static pid_t pid   = 0;
	static char pidstr[16];

	if(pid==0){
		pid   = getpid();
		sprintf(ntv_pid_filename,"%s.pid",ntv_info_filename);
		sprintf(ntv_err_filename,"%s.err",ntv_info_filename);
		sprintf(pidstr,"%d",pid);
		if(write_file(ntv_pid_filename,pidstr)==0){
			printf("can't write pid file!\n");
			exit_program(255);
			return;
		}
	}

	if(!file_exists(ntv_pid_filename)){
		printf("stopped by somebody\n");
		exit_program(255);
		return;
	}

	vsprintf(buffer, fmt, vl);
	if (level==AV_LOG_INFO)
	{
		//Duration: 00:00:00.00, start: 5255.381000, bitrate: N/A  资源时长实际是分两次输出到日志，第一次仅输出 Duration:
		if(hasDuration==1){
			strncpy(ntv_duration,buffer,10);
			hasDuration = 2;
		}else if(hasDuration==0 && strstr(buffer,"Duration:")){
			ntv_progress_info(buffer,"Duration:",ntv_duration);
			if(strlen(ntv_duration)>=8){
				hasDuration=2;
			}else{
				hasDuration=1;
			}
		}else if(strstr(buffer,"time=")){
			ntv_time[0]    = 0;
			ntv_bitrate[0] = 0;
			ntv_speed[0]   = 0;
			ntv_progress_info(buffer,"time=",   ntv_time);
			ntv_progress_info(buffer,"bitrate=",ntv_bitrate);
			ntv_progress_info(buffer,"speed=",  ntv_speed);
			ntv_output_info(NTV_PROGRESSINT,1);
		}else{
			//printf("%s", buffer);
		}
		
	}else if (level<=AV_LOG_ERROR){
		write_file(ntv_err_filename,buffer);
	}
}

static void program_exit_ntv(int ret){
	ffmpeg_cleanup(ret);
	ntv_output_info(ret,1);
	printf("ntv transcoding engine 3.0 is stopped(%d)\n",ret);
	if(file_exists(ntv_pid_filename)){
		remove(ntv_pid_filename);
	}
}