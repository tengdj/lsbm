#ifndef DLSM_PARAM_H
#define DLSM_PARAM_H
#include <unistd.h>
/************************** Constants *****************************/
#define BLKSIZE 4096
/************************** Configuration *****************************/

namespace leveldb {

namespace config {


// Level-0 compaction is started when we hit this many files.
static const int kL0_CompactionTrigger = 50;

// Soft limit on number of level-0 files.  We slow down writes at this point.
static const double kL0_SlowdownWritesTrigger = 1.1;

// Maximum number of level-0 files.  We stop writes at this point.
static const double kL0_StopWritesTrigger = 1.2;

// Approximate gap in bytes between samples of data read during iteration.
static const int kReadBytesPeriod = 1048576;

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


//we set it
const static int kNumLevels = 4;
const static int size_ratio = 10;


extern int compaction_buffer_length[];
extern int compaction_buffer_use_length[];

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

} // runtime

} // leveldb


#endif

