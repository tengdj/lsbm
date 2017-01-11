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
int files_merged_each_round = 2;

//teng: key-value cache
int key_cache_size = 0;
uint64_t num_blocks_cached_threshold = ((kTargetFileSize/1024)/4);

bool preload_metadata = false;
uint64_t hot_file_threshold = 2000;
bool manage_compaction_buffer = true;

double level0_max_score = 1.05;

}

namespace runtime{

bool pre_caching = false;
int compaction_buffer_length[]{0,12,25,2};// = size_ratio + 5;
int compaction_buffer_use_length[]{0,12,15,0};// = size_ratio + 5;
int compaction_buffer_management_interval[]{0,10,500,5000};
uint64_t num_reads_ = 0;
uint64_t num_writes_ = 0;

double compaction_min_score = 1;
int warm_up_status = 0;
bool need_warm_up = false;


//parameters for print info
bool print_version_info = false;
bool print_compaction_buffer = false;
bool print_dash = false;
int hitratio_interval = 100;
int max_print_level = config::kNumLevels-1;

uint64_t write_cursor = 0;

}//runtime


}//leveldb




