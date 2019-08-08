/**
目标：实现一个通用的进程管理程序
	  1）输出PID文件pidfile，
	  2）启动新进程，不阻塞原有调用程序。 
	  3) 监控新进程的执行，可以重新启动
	  4）收到kill信号时关闭子进程
	  5）pidfile被删除时关闭子进程，上层服务管理系统主要（唯一方式）通过删除pid文件来停止子进程。

	  出现运行错误，会写一个errorfile标记文件，具体错误原因不清楚。
运行：
	ntv_prun s/c pidfile errfile exe paras...
	   
	    s/c     命令类型， s 要做为持续服务运行，停止需要重新加载 c一次性命令
		pidfile pid文件名
		errfile 错误输出文件名
		exe     可执行程序名
		paras... 程序执行参数

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
	sigaddset(&act.sa_mask, SIGKILL);       //9      //唯有9无法响应，其他都可以，killall也可以
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

	//避免编程僵尸进程
	signal(SIGCHLD,SIG_IGN);
	
	setsid();
	chdir("/tmp");
	umask(0);
}

//pid的进程是否存在
int pidProcExists(int pid) 
{
	int ret=kill(pid,0);
	return  0==ret?1:0;
}

/**
	根据任务文件名判断进程是否存在:
	  pid>0  进程存在
	  pid<0  进程已终止
	  pid=0  从来没有起来过
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
		//在子进程

        //避免编程僵尸进程
		signal(SIGCHLD,SIG_IGN);
        
		//脱离原有终端终端
		setsid();
		umask(0);

		chdir("/tmp");  //?

		//关闭输入输出，可能导致某些进程无法正常执行
		/*
		unsigned int i = 0;
		for(i = 0; i < 1024; i++)
		{
			close(i);
		}
		*/
		
		//输出PID文件
		pid = write_pid_file(pidfile);
		ret = run_exec(execname,argv);
		
		/*
		 *  出错逻辑：
		 *  实际上如果以上正确执行，以下代码没有机会执行。
		 *  永远不会返回到父进程
		 */
		exit(1);
	}

	//在父进程，返回错误或pid
	return fd;
}

/**
多次尝试关闭进程， 0 关闭成功
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
	  处理僵尸进程，至关重要，不然僵尸进程一直存在，判断进程时产生误判
	  waitpid的返回值一共有3种情况： 　
		当正常返回的时候，waitpid返回收集到的子进程的进程ID；
		如果设置了选项WNOHANG，而调用中waitpid发现没有已退出的子进程可收集，则返回0； 　　 　　
		如果调用中出错，则返回-1，这时errno会被设置成相应的值以指示错误所在；当pid所指示的子进程不存在，或此进程存在，但不是调用进程的子进程，waitpid就会出错返回，这时errno被设置为ECHILD 
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

	//脱离主进程
	run_as_daemon();
	//注册消息
	reg_sign_handle();

	const char  ctype   = argv[1][0];
	const char* pidFile = argv[2];
	const char* errFile = argv[3];
	const char* exeFile = argv[4];

	logMgr = new LogMgr(errFile,1);
	//先写一个伪文件，删除该文件等同于要结束子进程。
	if(write_textfile(pidFile,"-1")!=0){
		logMgr->append("Can't open sub process, write pid file failed!");
		delete logMgr;
		return 2;
	}

	//子进程信号获取,至关重要
	signal(SIGCHLD,sig_child);

	//启动命令进程，fd是新进程pid
	printf("start............\n");  
	fd = run_sub_progress(exeFile,argv+4,pidFile);
	if(fd<0){
		//极端错误
		logMgr->append("Can't open sub process, FORK failed!");
		unlink(pidFile);
		delete logMgr;
		return 3;
	}

	//printf("start.....%d.......OK\n",fd);  

	//命令类型
	if(ctype=='s'){
		//printf("service............\n");           //服务型，需要保持服务
	}else{
		//printf("once...........\n");    
		goto END;   //命令型,任务已经完成
	}

	while(1){
		sleep(1);
		//printf("working....\n");

		if( fd<0 || !pidProcExists(fd) ){
			//printf("program exit!\n");
			if(!haserror){  //一段时间内首次出错
				logMgr->append("Process exit unnormally,restart it...");     //有段错误，TODO....已修复
			}
			
			haserror = 1;
			//5秒钟尝试一次重启
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
