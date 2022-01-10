#ifndef DLSM_PARAM_H
#define DLSM_PARAM_H
#include <unistd.h>
#include <memory>
#include <map>
#include "util/mutexlock.h"
#include <time.h>
#include <sys/stat.h>


/************************** Constants *****************************/
#define BLKSIZE 4096
/************************** Configuration *****************************/

namespace leveldb {

namespace config {

// Level-0 compaction is started when we hit this many files.
static const int kL0_CompactionTrigger = 35;

// Soft limit on number of level-0 files.  We slow down writes at this point.
static const double kL0_SlowdownWritesTrigger = 2;

// Maximum number of level-0 files.  We stop writes at this point.
static const double kL0_StopWritesTrigger = 3;

// Approximate gap in bytes between samples of data read during iteration.
static const int kReadBytesPeriod = 1048576;


extern uint64_t hot_file_threshold;
extern bool preload_metadata;
extern const char *db_path;

//teng: target file size, default 2 MB
extern int kTargetFileSize;

//teng: level 0 size, default 100 M
extern int kL0_size;

//teng: run compaction
extern bool run_compaction;

//teng: bloom filter in use
extern int bloom_bits_use;


extern int key_cache_size;

extern uint64_t num_blocks_cached_threshold;


//we set it
const static int kNumLevels = 4;
const static int size_ratio = 10;

extern int data_merged_each_round;
extern bool buffered_merge;
extern double level0_max_score;

/*
 * Bloom Filter
 */
inline size_t get_bloom_filter_probe_num (int bits_per_key) {
	int raw_probe_num = (bloom_bits_use < bits_per_key &&
			bloom_bits_use > 0)?
			bloom_bits_use : bits_per_key;
	// We intentionally round down to reduce probing cost a little bit
	return static_cast<size_t>(raw_probe_num * 0.69);  // 0.69 =~ ln(2)
}


} // config

namespace runtime {

extern std::map<uint64_t, uint64_t> cached_num;


extern int compaction_buffer_length[];
extern int compaction_buffer_use_length[];
extern int compaction_buffer_management_interval[];
extern int compaction_buffer_trim_interval;


extern uint64_t num_reads_;
extern uint64_t num_found_;
extern uint64_t num_writes_;
extern double compaction_min_score;

//0 not started, 1 warmup started, 2 warmup done
extern int warm_up_status;
extern bool need_warm_up;
inline bool notStartWarmUp(){
    return warm_up_status==0;
}
inline bool isWarmingUp(){
	return warm_up_status==1;
}
inline bool doneWarmUp(){
	return warm_up_status==2;
}
inline bool needWarmUp(){
	return need_warm_up;
}

extern bool print_version_info;
extern bool print_compaction_buffer;
extern bool print_dash;
extern int hitratio_interval;
extern int max_print_level;
extern bool pre_caching;

extern uint64_t write_cursor;
extern uint64_t read_cursor_[];
extern std::string read_cursor_key_[];

static void setReadCursor(){
	for(int i=0;i<config::kNumLevels;i++){
		char read_cursor_ch[100];
		snprintf(read_cursor_ch, sizeof(read_cursor_ch), "user%019ld", runtime::read_cursor_[i]);
		read_cursor_key_[i] = std::string(read_cursor_ch);
	}
}




extern uint64_t read_from_;
extern uint64_t read_upto_;
extern std::string read_upto_key_;
extern std::string read_from_key_;
static void setReadKeys(){
	char read_from_ch[25];
	char read_upto_ch[25];

	snprintf(read_from_ch, sizeof(read_from_ch), "user%019ld", runtime::read_from_);
	read_from_key_ = std::string(read_from_ch);

	snprintf(read_upto_ch, sizeof(read_upto_ch), "user%019ld", runtime::read_upto_);
	read_upto_key_ = std::string(read_upto_ch);

}


extern uint64_t write_upto_;
extern uint64_t write_from_;

extern uint64_t key_upto_;
extern uint64_t key_from_;

extern time_t global_time_begin_;

static void resetTimer(){
	time(&global_time_begin_);
}

static int timePassed(){
	time_t now;
	time(&now);
	return (int)difftime(now,global_time_begin_);

}

inline void gettime(timespec &time){
	clock_gettime(CLOCK_REALTIME,&time);
}
inline double diftimesec(timespec start){
	timespec end;

	clock_gettime(CLOCK_REALTIME,&end);
	return ((double)(end.tv_sec -  start.tv_sec)* 1000*1000*1000 + end.tv_nsec - start.tv_nsec)/(1000*1000);
}

inline double diftimemsec(timespec start){
	timespec end;

	clock_gettime(CLOCK_REALTIME,&end);
	return ((double)(end.tv_sec -  start.tv_sec)* 1000*1000*1000 + end.tv_nsec - start.tv_nsec)/1000;
}

} // runtime

} // leveldb


#endif

