#ifndef NTV_DEF_H
#define NTV_DEF_H

//全局变量声明
#include <string.h> 
/**
* 这两个变量要在其他地方使用
* 不要定义成static，static总是在每个编译单元产生副本。
*/
char ntv_info_filename[256]={0};
char ntv_enc_key_info[512]={0};
#endif