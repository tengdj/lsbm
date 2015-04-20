#ifndef DLSM_PARAM_H
#define DLSM_PARAM_H

/************************** Constants *****************************/
#define BLKSIZE 4096
#define HLSM_LOGICAL_LEVEL_NUM leveldb::config::kNumLevels/2

/************************** Configuration *****************************/

namespace leveldb {

namespace config {


extern const char *db_path;
//0 for LSM mode, 1 for dlsm mode, 2 for sm mode
extern int dbmode;

//teng: target file size, default 2 MB
extern int kTargetFileSize;

//teng: level 0 size, default 100 M
extern int kL0_size;

//teng: run compaction
extern bool run_compaction;

//teng: bloom filter in use
extern int bloom_bits_use;

extern int dlsm_end_level;

//make enough room for two phase compaction
const static int LogicalLevelnum = 7*2+1;
const static int levels_per_logical_level = 25;
//level 0 + other levels with two phase
const static int kNumLevels = (LogicalLevelnum-1)*levels_per_logical_level+1;


inline bool isSM(){
   return dbmode==2;
}
inline bool isLSM(){
	return dbmode==0;
}
inline bool isdLSM(){
	return dbmode==1;
}


} // config

namespace runtime {

extern bool two_phase_compaction;
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
extern int hitratio_internal;

} // runtime

} // hlsm


#endif

