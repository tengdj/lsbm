/*
 * dlsm_param.cc


 *
 *  Created on: Mar 17, 2015
 *      Author: teng
 */

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
int bloom_bits_use = -1;

//teng: end level for dlsm mode
int dlsm_end_level = 6;

}

namespace runtime{

bool two_phase_compaction = true;
int warm_up_status = 0;
bool need_warm_up = false;
bool print_version_info = false;
int hitratio_interval = 100;

}


}




