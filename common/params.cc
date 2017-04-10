/*
 * dlsm_param.cc


 *
 *  Created on: Mar 17, 2015
 *      Author: teng
 */

#include "leveldb/params.h"

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
int data_merged_each_round = 8;

//teng: key-value cache
int key_cache_size = 0;
uint64_t num_blocks_cached_threshold = (kTargetFileSize/4096);

bool preload_metadata = false;
uint64_t hot_file_threshold = 2000;
bool buffered_merge = true;

double level0_max_score = 1.05;

}

namespace runtime{
std::map<uint64_t, uint64_t> cached_num;


bool pre_caching = false;
int compaction_buffer_length[]{0,20,20,3};// = size_ratio + 5;
int compaction_buffer_use_length[]{0,20,20,0};// = size_ratio + 5;
int compaction_buffer_management_interval[]{0,100,1000,10000};
int compaction_buffer_trim_interval = 30;
uint64_t num_reads_ = 0;
uint64_t num_found_ = 0;
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
uint64_t read_upto_ = 4000000;
uint64_t read_from_ = 0;
std::string read_upto_key_;
std::string read_from_key_;

uint64_t read_cursor_[config::kNumLevels]{0,0,0,0};

uint64_t write_upto_ = 4000000;
uint64_t write_from_ = 0;

uint64_t key_upto_ = 4000000;
uint64_t key_from_ = 0;

std::string read_cursor_key_[config::kNumLevels];

time_t global_time_begin_ = 0;


}//runtime


}//leveldb




