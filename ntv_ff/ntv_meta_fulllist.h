/*
* ffmpeg��չ��������
* ���������playlist��������ֱ����
*  ����ļ�ֻ�Ǳ�������������û��ʹ�á�
* 
*/
#include <string.h>
#include "../libavformat/avio.h"

#include "../ntv/ntv_extern.h"
#include "../ntv/ntv_queue.h"

static char ntv_meta_filename[256]={0};
static AVIOContext *ntv_meta_io = NULL;
static int ntv_hls_version      = 3;
static int ntv_target_duration  = 5;
static int64_t ntv_sequence     = 1;
static ntv_queue  ntv_live_list;

static int ntv_write_meta(const char* buf);

#define NTV_META(format,...) {static char str[512];sprintf(str,format,##__VA_ARGS__);ntv_write_meta(str);}
//extern char ntv_info_filename[256];
/**
* ��������Ļؿ��б�
* ntv_max_nb_segments>0 ��ʾ��ֱ���б�
*/
static int ntv_open_meta(void){
	static int infolen = -1;
	static int haserror= 0;

	if(infolen==-1){
		infolen=strlen(ntv_info_filename);
	}

	//printf("\n---%d -- %d ---\n",infolen,ntv_max_nb_segments);
	if(!haserror && infolen && ntv_max_nb_segments && ntv_meta_io==NULL){
		strcpy(ntv_meta_filename,ntv_info_filename);
		if(strrchr(ntv_meta_filename,'/')){
			strrchr(ntv_meta_filename,'/')[1] = '\0';
		}
		strcat(ntv_meta_filename,"index.m3u8");

		avio_open(&ntv_meta_io,ntv_meta_filename, AVIO_FLAG_WRITE /* AVIO_FLAG_READ_WRITE */ ); //AVIO_FLAG_WRITE
		if(ntv_meta_io){
			printf("open index file: %s\n",ntv_meta_filename);
			//avio_seek(ntv_meta_io,0,SEEK_END);
			//if(avio_tell(ntv_meta_io)==0){
				avio_printf(ntv_meta_io, "#EXTM3U\n");
				avio_printf(ntv_meta_io, "#EXT-X-VERSION:%d\n", ntv_hls_version);
				avio_printf(ntv_meta_io, "#EXT-X-TARGETDURATION:%d\n", ntv_target_duration);
				avio_printf(ntv_meta_io, "#EXT-X-MEDIA-SEQUENCE:%"PRId64"\n", ntv_sequence);
				avio_flush(ntv_meta_io);
			//}
		}else{
			haserror = 1;
			printf("open index file failed: %s",ntv_meta_filename);
		}
	}
	return 0;
}

static int ntv_write_meta(const char* buf){
	if(ntv_meta_io){
		avio_printf(ntv_meta_io,"%s",buf);
	}
	return 0;
}

static void ntv_write_flush(void){
	if(ntv_meta_io){
		avio_flush(ntv_meta_io);
	}
}


/**
����ֱ�����������ļ��ĸ��£��ļ��Ƿ���ͨ��ff_hls_write_file_entry д��ģ��������ڹ����ظ�����д�������ļ�
ֻ֧���ļ��������������ҵ��������hls->max_nb_segments = 0;
*/
static int ntv_new_file(const char* filename){
	static int lastseq = -1;
	
	int seq;
	const char* name = strrchr(filename,'/');
	if(name)
		name++;
	else
		name = filename;
	
	seq = atoi(name);
	if(seq>lastseq){
		lastseq = seq;
		return 1;
	}else{
		return 0;
	}
}
