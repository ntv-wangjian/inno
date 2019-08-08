/*
���������࣬������������ļ�ִ��once��service���
������֮ǰ�汾��service����Ĵ�����������prun���̣�������ֻ��������prun���̣����ַ�ʽ���ڳ���ļ򻯺��ȶ�����Ҫ���á�
prun�Ƿ�����������������

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

#include "miscs.h"
#include "queue.c"
#include "log.c"

using namespace std; 

#define MAX_COMMAND_LENGTH       2000
#define MAX_FILENAME_LENGTH      256

#define STATUS_INIT     0        //��ʼ״̬
#define STATUS_RUNNING  1        //��������
#define STATUS_STOPPED  2		 //û�����У����ߴ�����״̬תΪ��ֹͣ״̬
#define STATUS_CLOSING  3		 //���ڹرս���
#define STATUS_CLOSED   4		 //�����ѹر�
#define STATUS_ERROR    10       //�쳣��ֹ
#define STATUS_NULL     100

#define TYPE_SERVICE    1
#define TYPE_ONCE       2
#define TYPE_NULL       0

#define PROC_RUNNER "ntv_prun"    //ִ�з���������

const static char* g_workpath; 
class ProcessMgr{
private:

public:
	ProcessMgr(const char* workpath,LogMgr* logger){
		char path[255];

		logMgr     = logger;
		g_workpath = workpath;
		
		sprintf(path,"%s/pids",g_workpath);
		if(!IsFileExist(path)){
			CreateDir(path);
		}

		sprintf(path,"%s/backup",g_workpath);
		if(!IsFileExist(path)){
			CreateDir(path);
		}

		chdir(g_workpath);		
	}

	~ProcessMgr(){
		queue.clear();
	}

	/*
	��Ŀ¼�����������ļ�
	*/
	int  scanCommands()
	{ 
		DIR *dp;
		struct dirent *entry;
		int  type,count=0;
		//struct stat statbuf;
		if((dp = opendir(g_workpath)) == NULL) {
			return 1;
		}
		while((entry = readdir(dp)) != NULL) {
			//lstat(entry->d_name,&statbuf);
			type = processType(entry->d_name);
			if(type>0){
				createProcess(entry->d_name,type);
				count++;
			}
		}
		closedir(dp);
		return count;	
	}

	//������������ļ�������ʱ֪ͨprun�ر�����
	int  checkCommands(){
		for (std::map<string,S_PROCESS*>::iterator it=queue.map_process.begin(); it!=queue.map_process.end(); ++it)
		{
			S_PROCESS* proc = it->second;
			const char* filename = proc->cmd_file.c_str();
			if(!isDutyExists(filename)){
				stopProcess(filename);
				delete it->second;
				queue.map_process.erase(it);
				log2("Service is closed... ",filename);
			}
		}
		return 0;
	}

	int stopAll(){
		for (std::map<string,S_PROCESS*>::iterator it=queue.map_process.begin(); it!=queue.map_process.end(); ++it)
		{
			S_PROCESS* proc = it->second;
			const char* filename = proc->cmd_file.c_str();
			stopProcess(filename);
			delete it->second;
			queue.map_process.erase(it);
			log2("Stop service(stop all)... ",filename);
		}
		return 0;
	}

	int  procNum(){
		return queue.size();
	}
private:
	/**
	��������������̣����ս�����ȫ����prun���̡�
	ntv_prun pidfile ...
	*/
	int runService(const char* pidfile,const char* errfile,const char* cmd){
		static char prun_cmd[MAX_COMMAND_LENGTH];
		if(strlen(pidfile)*2 + strlen(cmd) + strlen(PROC_RUNNER) + 32 >= MAX_COMMAND_LENGTH){
			log("command is too long to load!");
			log(cmd);
			return -1;
		}
		sprintf(prun_cmd,"%s s %s %s %s",PROC_RUNNER,pidfile,errfile,cmd);
		//���������null�����������ʾ���Ƿ�ᵼ����Щ�����޷����������� Ŀǰ�����ᣬֻ���ض����׼�����
		if(!strstr(cmd,"/dev/null")){
			strcat(prun_cmd," >/dev/null 2>&1");
		}
		//log(prun_cmd);
		
		return system(prun_cmd);
	}

	int runOnce(const char* cmd){
		static char prun_cmd[MAX_COMMAND_LENGTH];
		if(strlen(cmd) + 32 >= MAX_COMMAND_LENGTH){
			log("command is too long to load!");
			log(cmd);
			return -1;
		}
		if(strstr(cmd,">")){
			sprintf(prun_cmd,"%s &",cmd);
		}else{
			sprintf(prun_cmd,"%s >/dev/null 2>&1 &",cmd);
		}
		//log(prun_cmd);
		return system(prun_cmd);
	}

	static int processType(const char* filename){
		const char* suf = strrchr(filename,'.');
		if(!suf){
			return TYPE_NULL;
		}else if(strcmp(suf,".service")==0){
			return TYPE_SERVICE;
		}else if(strcmp(suf,".once")==0){
			return TYPE_ONCE;
		}else{
			return TYPE_NULL; 
		}
	}

	//���������ļ���������
	void createProcess(const char* filename,int type){
		static char cmdtxt[MAX_COMMAND_LENGTH+1];

		//�Ѿ���������
		if(queue.find(filename)){
			return;
		}

		char* fullname = fullFilename(filename);
		int read = ReadFile(fullname,cmdtxt,MAX_COMMAND_LENGTH);
		if(read<=0){
			log("Error: Process cmd file is empty!");
			log(fullname);
			return;
		}
		cmdtxt[read] = '\0';

		const char* pidfile = pidFilename(filename);
		const char* errfile = errFilename(filename);
		if(type == TYPE_ONCE){
			log2("Run command... ",filename);
			if(deleteCmdFile(filename)!=0){
				log("Error: Failed! Can't remove cmd file!");
				log(filename);
				return;
			}
			if(runOnce(cmdtxt)!=0){
				log2("Run command failed! ",filename);
				return;
			}
		}else if(type == TYPE_SERVICE){
			log2("Run service... ",filename);
			int pid = isProcRunning(filename);
			if(pid>0){
				//�����Ѿ������ˡ� BUG �� �����жϴ��ڷ��գ�Ҳ�п�������һ������ռ����������̺š�
				log2("Service is running now,bind it: " ,filename);
				S_PROCESS* proc   = new S_PROCESS;
				proc->cmd_file    = filename;
				proc->cmd         = cmdtxt;
				proc->type        = type;
				proc->start       = time(NULL);
			
				queue.add(proc);
				return;
			}

			//���������Ϣ
			deletePidFile(filename);
			deleteErrFile(filename);

			//run it;
			if(runService(pidfile,errfile,cmdtxt)!=0){
				writeErrFile(filename,"Start process failed!");
				log2("Error: Start process failed! ",filename);
				log(cmdtxt);
				//������������һ�������������⣬ֱ��ɾ�������ٽ�����������
				deleteCmdFile(filename);
			}else{		
				S_PROCESS* proc   = new S_PROCESS;
				proc->cmd_file    = filename;
				proc->cmd         = cmdtxt;
				proc->type        = type;
				proc->start       = time(NULL);

				queue.add(proc);
			}
		}
	}

	//��ֹ���������
	void stopProcess(const char* filename){
		//֪ͨprun���̽���
		deletePidFile(filename);
		//deleteErrFile(filename);
	}

	//pid�Ľ����Ƿ����
	static int isProcExists(int pid) 
	{
		int ret=kill(pid,0);
		return  0==ret?1:0;
	}

	/*
	���������ļ����жϽ����Ƿ����:
	  pid>0  ���̴���
	  pid<0  ��������ֹ
	  pid=0  ����û��������
	*/
	static int isProcRunning(const char* filename){
		static char spid[32];
		const char* pidfile = pidFilename(filename);
		if(IsFileExist(pidfile)){
			if(ReadFile(pidfile,spid,32)>0){
				int pid = atoi(spid);
				if(isProcExists(pid)){
					return pid;
				}else{
					return 0-pid;
				}
			}
		}

		return 0;
	}

	//�ж������ļ��Ƿ񻹴���
	static int isDutyExists(const char* filename){
		const char* fullfilename = fullFilename(filename);
		return IsFileExist(fullfilename);
	}

	//ɾ�������ļ�
	static int deleteCmdFile(const char* filename){
		return backupCmdFile(filename);

		char* fullname = fullFilename(filename);
		return DeleteFile(fullname);
	}
	static int backupCmdFile(const char* filename){
		static char des[MAX_FILENAME_LENGTH];
		char* fullname = fullFilename(filename);
		if(IsFileExist(fullname)){
			sprintf(des,"%s/backup/%s",g_workpath,filename);
			return rename(fullname,des);
		}
		return 0;
	}

	static int deletePidFile(const char* filename){
		char* fullname = pidFilename(filename);
		return DeleteFile(fullname);
	}

	/*
	  д������Ϣ
	*/
	static int writeErrFile(const char* filename,const char* error)
	{
		char* fullname = errFilename(filename);
		if(!IsFileExist(fullname)){
			return WriteFile(fullname,error,strlen(error));
		}else{
			return 0;
		}
	}

	static int deleteErrFile(const char* filename){
		char* fullname = errFilename(filename);
		return DeleteFile(fullname);
	}

	//�������ļ�����������pid�ļ�
	static char* pidFilename(const char* filename){
		static char fullname[MAX_FILENAME_LENGTH];
		sprintf(fullname,"%s/pids/%s.pid",g_workpath,filename);
		return fullname;
	}

	static char* fullFilename(const char* filename){
		static char fullname[MAX_FILENAME_LENGTH];
		sprintf(fullname,"%s/%s",g_workpath,filename);
		return fullname;
	}

	static char* errFilename(const char* filename){
		static char fullname[MAX_FILENAME_LENGTH];
		sprintf(fullname,"%s/%s.error",g_workpath,filename);
		return fullname;
	}

private:
	QueueMgr    queue;
	LogMgr*     logMgr;
	void log(const char* text){
		logMgr->log(text);
	}
	void log2(const char* text,const char* text2){
		static char logstr[512];
		if(strlen(text)+strlen(text2)>=512){
			log(text);
			log(text2);
			return;
		}

		sprintf(logstr,"%s%s",text,text2);
		logMgr->log(logstr);
	}
};