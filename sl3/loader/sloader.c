/**
���������࣬������������ļ�ִ��once��service���
������֮ǰ�汾��service����Ĵ�����������prun���̣�������ֻ��������prun���̣����ַ�ʽ���ڳ���ļ򻯺��ȶ�����Ҫ���á�
���У� 
	ntv_sloader workpath isdemon

created by wangjian 2019-3-5
*/


#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <signal.h> 
#include <time.h>
#include <fcntl.h>
#include<sys/wait.h>

#include "process.c"

//////////misc functions
#define MAXFILE 65535
volatile sig_atomic_t exit_flag  =0;
void sign_handle(int signal)
{
	exit_flag  =1;
}
void reg_sign_handle()
{
	struct sigaction act, oldact;
	act.sa_handler = sign_handle;
	act.sa_flags   = SA_RESETHAND | SA_NODEFER;
	
	sigaddset(&act.sa_mask, SIGINT);		//2  
	sigaddset(&act.sa_mask, SIGQUIT);       //3
	sigaddset(&act.sa_mask, SIGKILL);       //9      //Ψ��9�޷���Ӧ�����������ԣ�killallҲ����
	sigaddset(&act.sa_mask, SIGTERM );      //15

	sigaction(SIGINT,  &act, &oldact);
	sigaction(SIGQUIT, &act, &oldact);
	sigaction(SIGKILL, &act, &oldact);
	sigaction(SIGTERM, &act, &oldact);
}

void run_as_daemon()
{
	pid_t pc;
    pc = fork();
	if(pc<0){
		exit(1);
	}
	else if(pc>0){
		exit(0);
	}

	setsid();
	chdir("/");
	umask(0);
}

const char* currentProcName(){
	static char strProcessPath[1024] = {0};
	if(readlink("/proc/self/exe", strProcessPath,1024) <=0)
	{
		return NULL;
	}
	const char* pname = strrchr(strProcessPath, '/');
	if(pname){
		return ++pname;
	}else{
		return NULL;
	}

	return pname;
}

void showHelp(){
	const char* sname =  currentProcName();
	if(!sname){
		sname = "ntv_sloader";
	}
	printf("Usage:\n");
	printf("    %s <work_path> [deamon]\n",sname);
	printf(" such as:\n    %s /etc/noveltv/services \n",sname);
	printf(" or run as demaon:\n    %s /etc/noveltv/services daemon\n",sname);
}

int main(int argc,char* argv[])
{
	if(argc<2){
		showHelp();
		return 1;
	}

	int isdeamon = 0;
	if(strstr(argv[argc-1],"daemon"))
	{
		isdeamon = 1;
		run_as_daemon();
	}

	const char* logfile = "/etc/noveltv/mserver/logs/service_loader.log";
	DeleteFile(logfile);

	LogMgr*    logMgr = new LogMgr(logfile,isdeamon);
	logMgr->log("NOVEL-TV Service Loader 3.0 Start!");

	char workpath[MAX_FILENAME_LENGTH];
	strcpy(workpath,argv[1]);
	int len = strlen(workpath);
	if(workpath[len-1]=='/'){
		workpath[len-1]='\0';
	}
	if(!IsFileExist(workpath)){
		logMgr->log("Error: Work path does not exist!");
		logMgr->log("Error: Service Loader Exit!");
		delete logMgr;
		return 2;
	}

	reg_sign_handle();

	ProcessMgr procMgr(workpath,logMgr);
	char  str[64];
	int   lastNum = 0;
	int   procNum = 0; 
	while(1){
		if(exit_flag==1){
			break;
		}
		procMgr.scanCommands();
		procMgr.checkCommands();

		//100����
		usleep(1000*100);
		procNum=procMgr.procNum();
		if(procNum!=lastNum){
			sprintf(str,"%d process is running.",procNum);
			logMgr->log(str);
			lastNum = procNum;
		}
	}

	//ֹͣ���з������
	logMgr->log("To stop all services...");
	procMgr.stopAll();
	logMgr->log("Service Loader Exit normally!");
	delete logMgr;

	system("stty echo");
	//printf("\n");
	return 0;
}
