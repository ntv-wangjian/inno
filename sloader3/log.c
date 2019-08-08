/**
version: 1  on 2012-12-1
*/

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define MAX_LOG_ITEM 2000

class LogMgr{
public:
	LogMgr(const char* filename,int isdeamon){
		strcpy(logfilename,filename);
		fp     = NULL;
		deamon = isdeamon;
	}

	~LogMgr(){
		if(fp){
			fclose(fp);
		}
	}

	void append(const char* text){
		static const char* endstr = "\n";
		static int   count   = 0;
		
		FILE *fhandle = NULL;
		if(count++>MAX_LOG_ITEM){
			fhandle = fopen(logfilename,"w");
			count   = 0;
		}else{
			fhandle = fopen(logfilename,"a");
		}

		if(fhandle){
			const char* strt = formatTime();
			fwrite(strt,1,strlen(strt),fhandle);
			fwrite(text,  1,strlen(text),  fhandle);
			fwrite(endstr,1,strlen(endstr),fhandle);
			fclose(fhandle);
		}
		
		if(!deamon){
			printf("%s%s",text,endstr);
		}
	}

	const char* formatTime(){
	  static char buffer[128];
	  time_t rawtime;
      struct tm * timeinfo;
       
	  time (&rawtime);
	  timeinfo = localtime(&rawtime);
      strftime(buffer,sizeof(buffer),"%Y/%m/%d %H:%M:%S ",timeinfo);
	  return buffer;
	}

	void log(const char* text){
		static int   count   = 0;
		static const char* endstr = "\n";

		if(count>MAX_LOG_ITEM){
			fclose(fp);
			count = 0;
			fp = NULL;
		}
		if(!fp){
			fp = fopen(logfilename,"w");
		}
		if(fp){
			const char* strt = formatTime();
			fwrite(strt,1,strlen(strt),fp);
			fwrite(text,1,strlen(text),fp);
			fwrite(endstr,1,strlen(endstr),fp);
			fflush (fp);
			count++;
		}
		
		if(!deamon){
			printf("%s%s",text,endstr);
		}
	}
private:
	int     deamon;
	FILE	*fp;
	char    logfilename[256];
};