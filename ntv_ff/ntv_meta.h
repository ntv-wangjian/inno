/*
* ffmpeg扩展辅助方法
* 输出直播列表。
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
static int ntv_playlist_type    = 0;
static int ntv_is_write_playlist= 0;

static ntv_queue  ntv_live_list;

//#define NTV_META(format,...) {static char str[512];sprintf(str,format,##__VA_ARGS__);ntv_write_meta(str);}

static int ntv_open_meta(void){
	static int infolen = -1;
	static int opened  = 0;
	if(infolen==-1){
		infolen=strlen(ntv_info_filename);
	}
	if(infolen==0){
		//没有提供info参数
		return 0;
	}

	if(opened){
		return ntv_is_write_playlist;
	}
	
	if(ntv_playlist_type==PLAYLIST_TYPE_EVENT){
		strcpy(ntv_meta_filename,ntv_info_filename);
		if(strrchr(ntv_meta_filename,'/')){
			strrchr(ntv_meta_filename,'/')[0] = '\0';
			//上一层目录
			if(strrchr(ntv_meta_filename,'/')){
				strrchr(ntv_meta_filename,'/')[1] = '\0';
			}
		}
		strcat(ntv_meta_filename,"index.m3u8");

		init_queue(&ntv_live_list);
		ntv_is_write_playlist = 1;
		opened = 1;
	}else{
		opened = 2;
	}

	//printf("\n---%d -- %d --- %s\n",ntv_is_write_playlist,ntv_playlist_type,ntv_meta_filename);

	return ntv_is_write_playlist;
}


static int ntv_write_file_entry(AVIOContext *out, int insert_discont,
                             int byterange_mode,
                             double duration, int round_duration,
                             int64_t size, int64_t pos,
                             char *baseurl,
                             char *filename) {
    if (!out || !filename)
        return AVERROR(EINVAL);

    if (insert_discont) {
        avio_printf(out, "#EXT-X-DISCONTINUITY\n");
    }
    if (round_duration){
        avio_printf(out, "#EXTINF:%ld,\n",  lrint(duration));
	}else{
        avio_printf(out, "#EXTINF:%f,\n", duration);
	}
    if (byterange_mode){
        avio_printf(out, "#EXT-X-BYTERANGE:%"PRId64"@%"PRId64"\n", size, pos);
	}

    if (baseurl){
        avio_printf(out, "%s", baseurl);
	}
    avio_printf(out, "%s\n", filename);
    return 0;
}

static int write_live_list(int write_end){
	static tmp_file[512];

	ntv_queue_item* item = ntv_live_list.head;
	if(!item){
		return 0;
	}

	sprintf(tmp_file,"%s.tmp",ntv_meta_filename);
	avio_open(&ntv_meta_io,tmp_file, AVIO_FLAG_WRITE);
	if(!ntv_meta_io){
		printf("open index file failed! %s \n",ntv_meta_filename);
		return 0;
	}
			
	avio_printf(ntv_meta_io, "#EXTM3U\n");
	avio_printf(ntv_meta_io, "#EXT-X-VERSION:%d\n",               ntv_hls_version);
	avio_printf(ntv_meta_io, "#EXT-X-TARGETDURATION:%d\n",        ntv_target_duration);
	avio_printf(ntv_meta_io, "#EXT-X-MEDIA-SEQUENCE:%"PRId64"\n", ntv_sequence++);
    if(strlen(ntv_enc_key_info)>10){
		avio_printf(ntv_meta_io, "%s", ntv_enc_key_info);
	}

	while(item)
	{
		ntv_write_file_entry(ntv_meta_io,item->insert_discont,item->byterange_mode,item->duration,
			item->round_duration,item->size,item->pos,item->baseurl,item->filename);
		item = item->next;
	}

	if(write_end){
		//写入end，导致终端收看下次直播时不会刷新列表。最好保持直播列表状态。
		//avio_printf(ntv_meta_io,"#EXT-X-ENDLIST\n");
	}
	avio_flush(ntv_meta_io);
	avio_close(ntv_meta_io);

	if(rename(tmp_file,ntv_meta_filename)!=0){
		printf("rename index file failed! %s \n",ntv_meta_filename);
	}
	return 1;
}

static int add_list_item(ntv_queue_item* item){
	int size = add_queue_item(&ntv_live_list,item);
	if(size>3){
		size = remove_queue_head(&ntv_live_list);
	}
	write_live_list(0);
	/*}else if(size==3){
		write_live_list(0);
	}*/
	return size;
}


/**
索引文件的更新，文件是反复通过ff_hls_write_file_entry 写入的，这里用于过滤重复，仅写入最新文件
*/
static int ntv_is_new_file(const char* filename){
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
