/**
目标：启动并看守转码进程。
version: 1  on 2019-03-03

程序启动参数：
	任务文件名
	转码进程pid文件名

看守进程启动后，首先启动转码进程，然后：
	1）监控任务文件，一旦被删除就关闭转码进程；
	2）监控转码进程，一旦进程不再，就尝试重新启动。
*/

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <signal.h>
//#include <dirent.h>
//#include <time.h>
//#include <fcntl.h>
//#include <sys/wait.h>

//pid的进程是否存在
static int isProcExists(int pid) 
{
	int ret=kill(pid,0);
	return  0==ret?1:0;
}

int write_pid(const char* filename)
{
	char strpid[16];
	int  pid = getpid();

	sprintf(strpid,"%d",pid);
	FILE* fp = fopen(filename,"w");
	if(fp)
	{
		fwrite(strpid,1,strlen(strpid),fp);
		fclose(fp);
		return pid;
	}
	else
	{
		return -1;
	}
}

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
        //避免编程僵尸进程
		signal(SIGCHLD,SIG_IGN);
        
		//脱离原有终端终端
		setsid();
		umask(0);
		chdir("/tmp");

		//关闭输入输出  导致某些进程无法正常执行
		unsigned int i = 0;
		for(i = 0; i < 1024; i++)
		{
			//close(i);
		}
		
		//输出PID文件
		pid = write_pid(pidfile);
		ret = run_exec(execname,argv);
		
		/*
		 *  实际上如果以上正确执行，以下代码没有机会执行。
		 *  pidfile的删除，有外部进程管理程序负责。
		 */
		unlink(pidfile);
		printf("sub process exit %d",pid);
	}

	return fd; 
}

/*
传入参数要求：
exe pidfile exename args...
返回值：
	>0 表示子进程的pid
	-1 及其他表示运行错误
*/
int main(int argc,char* argv[])
{
	int fd;
	if(argc<3){
		printf("needs more params...\n");
		return 0;
	}
	fd = run_sub_progress(argv[2],argv+2,argv[1]);
	return fd;
}
