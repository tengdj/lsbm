/*
 * dlsm_param.cc


 *
 *  Created on: Mar 17, 2015
 *      Author: teng
 */

#include "dlsm_param.h"

namespace leveldb{

namespace config{

const char *db_path;
int dbmode;

//teng: target file size, default 8 MB
int kTargetFileSize = 8 * 1048576;

//teng: level 0 size, default 100 M
int kL0_size = 100;

//teng: run compaction
bool run_compaction = true;

//teng: bloom filter in use
int bloom_bits_use = 15;

int compaction_buffer_length[]{0,15,35,0};// = size_ratio + 5;
int compaction_buffer_use_length[]{0,12,15,0};// = size_ratio + 5;
//teng: key-value cache
int key_cache_size = 0;
}

namespace runtime{

double compaction_min_score = 1;
int warm_up_status = 0;
bool need_warm_up = false;


//parameters for print info
bool print_version_info = false;
bool print_compaction_buffer = false;
bool print_dash = false;
int hitratio_interval = 100;
int max_print_level = config::kNumLevels-1;

bool pre_caching = false;
}//runtime


}//leveldb




