/*
实现基于map的一个任务管理器，管理所有进程的信息。
created by wangjian 2018-12-20
*/
#include <iostream>
#include <map>
#include <string>
using namespace std; 

typedef struct S_PROCESS
{
    string   cmd_file;
	string   cmd;
	int      type;
    int      status;             //see STATUS_
    int      mount_count;
    int      pid;    
    time_t    start,end;
} S_PROCESS;

class QueueMgr{
public:
	std::map<string,S_PROCESS*> map_process;

	void add(S_PROCESS* proc){
		map_process.insert(std::pair<string, S_PROCESS*>(proc->cmd_file,proc));
	}

	S_PROCESS* find(string key){
		std::map<string,S_PROCESS*>::iterator it = map_process.find(key);
		if(it != map_process.end()){
			return it->second;
		}else{
			return NULL;
		}
	}

	void del(string key){
		std::map<string,S_PROCESS*>::iterator it = map_process.find(key);
		if(it != map_process.end()){
			delete it->second;
			map_process.erase(it);
		}
	}

	int size(){
		return map_process.size();
	}

	void clear(){
		for (std::map<string,S_PROCESS*>::iterator it=map_process.begin(); it!=map_process.end(); ++it)
		{
			delete it->second;
		}
	}

	/*
	// show content:
	  for (std::map<char,int>::iterator it=mymap.begin(); it!=mymap.end(); ++it)
			std::cout << it->first << " => " << it->second << '\n';
	*/

private:
	
};