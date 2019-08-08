/**
Ŀ�꣺ʵ��һ��ͨ�õĽ��̹������
	  1�����PID�ļ�pidfile��
	  2�������½��̣�������ԭ�е��ó��� 
	  3) ����½��̵�ִ�У�������������
	  4���յ�kill�ź�ʱ�ر��ӽ���
	  5��pidfile��ɾ��ʱ�ر��ӽ��̣��ϲ�������ϵͳ��Ҫ��Ψһ��ʽ��ͨ��ɾ��pid�ļ���ֹͣ�ӽ��̡�

	  �������д��󣬻�дһ��errorfile����ļ����������ԭ�������
���У�
	ntv_prun s/c pidfile errfile exe paras...
	   
	    s/c     �������ͣ� s Ҫ��Ϊ�����������У�ֹͣ��Ҫ���¼��� cһ��������
		pidfile pid�ļ���
		errfile ��������ļ���
		exe     ��ִ�г�����
		paras... ����ִ�в���

version: 1  on 2019-3-5
*/

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <signal.h>
#include <dirent.h>
#include <time.h>
#include <fcntl.h>
#include <sys/wait.h>

#include "log.c"
#include "miscs.h"

LogMgr*  logMgr = NULL;

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

	//�����̽�ʬ����
	signal(SIGCHLD,SIG_IGN);
	
	setsid();
	chdir("/tmp");
	umask(0);
}

//pid�Ľ����Ƿ����
int pidProcExists(int pid) 
{
	int ret=kill(pid,0);
	return  0==ret?1:0;
}

/**
	���������ļ����жϽ����Ƿ����:
	  pid>0  ���̴���
	  pid<0  ��������ֹ
	  pid=0  ����û��������
*/
/**
int isProcRunning(const char* pidfile){
	static char spid[32];
	if(IsFileExist(pidfile)){
		if(ReadFile(pidfile,spid,32)>0){
			int pid = atoi(spid);
			if(pidProcExists(pid)){
				return pid;
			}else{
				return 0-pid;
			}
		}
	}
	
	return 0;
}
*/

int run_exec(const char* execname,char *const argv[]){
	if(strstr(execname,"/")){
        return execv(execname,argv);
    }else{
        return execvp(execname,argv);
    }
}

int run_sub_progress(const char* execname,char *const argv[],const char* pidfile)
{
    int fd,pid,ret;
    fd = fork();
	if(fd == 0){
		//���ӽ���

        //�����̽�ʬ����
		signal(SIGCHLD,SIG_IGN);
        
		//����ԭ���ն��ն�
		setsid();
		umask(0);

		chdir("/tmp");  //?

		//�ر�������������ܵ���ĳЩ�����޷�����ִ��
		/*
		unsigned int i = 0;
		for(i = 0; i < 1024; i++)
		{
			close(i);
		}
		*/
		
		//���PID�ļ�
		pid = write_pid_file(pidfile);
		ret = run_exec(execname,argv);
		
		/*
		 *  �����߼���
		 *  ʵ�������������ȷִ�У����´���û�л���ִ�С�
		 *  ��Զ���᷵�ص�������
		 */
		exit(1);
	}

	//�ڸ����̣����ش����pid
	return fd;
}

/**
��γ��Թرս��̣� 0 �رճɹ�
*/
int tryKillProcess(int pid,int tryTimes){
	if(pid<0 || !pidProcExists(pid)){
		return 0;
	}
	
	int trys = 0;
	while(pidProcExists(pid)){
		if(++trys>tryTimes){
			return 1;
		}
		kill(pid,SIGTERM);
		sleep(1); 
	}
	return 0;
}

static void sig_child(int signo)
 {
      pid_t        pid;
      int        stat;
	  /**
	  ����ʬ���̣�������Ҫ����Ȼ��ʬ����һֱ���ڣ��жϽ���ʱ��������
	  waitpid�ķ���ֵһ����3������� ��
		���������ص�ʱ��waitpid�����ռ������ӽ��̵Ľ���ID��
		���������ѡ��WNOHANG����������waitpid����û�����˳����ӽ��̿��ռ����򷵻�0�� ���� ����
		��������г����򷵻�-1����ʱerrno�ᱻ���ó���Ӧ��ֵ��ָʾ�������ڣ���pid��ָʾ���ӽ��̲����ڣ���˽��̴��ڣ������ǵ��ý��̵��ӽ��̣�waitpid�ͻ�����أ���ʱerrno������ΪECHILD 
	  */
      while ((pid = waitpid(-1, &stat, WNOHANG)) >0)
             printf("child %d terminated.\n", pid);
 }

int main(int argc,char* argv[])
{
	int    fd, haserror = 0;
	time_t tmNow,tm = time(NULL);

	if(argc<5){
		return 1;
	}

	//����������
	run_as_daemon();
	//ע����Ϣ
	reg_sign_handle();

	const char  ctype   = argv[1][0];
	const char* pidFile = argv[2];
	const char* errFile = argv[3];
	const char* exeFile = argv[4];

	logMgr = new LogMgr(errFile,1);
	//��дһ��α�ļ���ɾ�����ļ���ͬ��Ҫ�����ӽ��̡�
	if(write_textfile(pidFile,"-1")!=0){
		logMgr->append("Can't open sub process, write pid file failed!");
		delete logMgr;
		return 2;
	}

	//�ӽ����źŻ�ȡ,������Ҫ
	signal(SIGCHLD,sig_child);

	//����������̣�fd���½���pid
	printf("start............\n");  
	fd = run_sub_progress(exeFile,argv+4,pidFile);
	if(fd<0){
		//���˴���
		logMgr->append("Can't open sub process, FORK failed!");
		unlink(pidFile);
		delete logMgr;
		return 3;
	}

	//printf("start.....%d.......OK\n",fd);  

	//��������
	if(ctype=='s'){
		//printf("service............\n");           //�����ͣ���Ҫ���ַ���
	}else{
		//printf("once...........\n");    
		goto END;   //������,�����Ѿ����
	}

	while(1){
		sleep(1);
		//printf("working....\n");

		if( fd<0 || !pidProcExists(fd) ){
			//printf("program exit!\n");
			if(!haserror){  //һ��ʱ�����״γ���
				logMgr->append("Process exit unnormally,restart it...");     //�жδ���TODO....���޸�
			}
			
			haserror = 1;
			//5���ӳ���һ������
			tmNow = time(NULL);
			if(tmNow-tm>5){			
				//printf("restart...\n");
				fd = run_sub_progress(exeFile,argv+4,pidFile);
				tm = tmNow;
			}
		}else if(haserror){
			haserror = 0;
			DeleteFile(errFile);
		}

		if(exit_flag==1){
			break;
		}

		if(!IsFileExist(pidFile)){
			break;
		}
		
	}

	if(tryKillProcess(fd,3)){
		logMgr->append("Can't close sub process!" );
	}else{
		//DEBU
		//logMgr->append("Process exit!" );
	}

END:
	if(IsFileExist(pidFile)){
		unlink(pidFile);
	}

	delete logMgr;
	return 0;
}
