/**
Ŀ�꣺����������ת����̡�
version: 1  on 2019-03-03

��������������
	�����ļ���
	ת�����pid�ļ���

���ؽ�����������������ת����̣�Ȼ��
	1����������ļ���һ����ɾ���͹ر�ת����̣�
	2�����ת����̣�һ�����̲��٣��ͳ�������������
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

//pid�Ľ����Ƿ����
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
        //�����̽�ʬ����
		signal(SIGCHLD,SIG_IGN);
        
		//����ԭ���ն��ն�
		setsid();
		umask(0);
		chdir("/tmp");

		//�ر��������  ����ĳЩ�����޷�����ִ��
		unsigned int i = 0;
		for(i = 0; i < 1024; i++)
		{
			//close(i);
		}
		
		//���PID�ļ�
		pid = write_pid(pidfile);
		ret = run_exec(execname,argv);
		
		/*
		 *  ʵ�������������ȷִ�У����´���û�л���ִ�С�
		 *  pidfile��ɾ�������ⲿ���̹��������
		 */
		unlink(pidfile);
		printf("sub process exit %d",pid);
	}

	return fd; 
}

/*
�������Ҫ��
exe pidfile exename args...
����ֵ��
	>0 ��ʾ�ӽ��̵�pid
	-1 ��������ʾ���д���
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
