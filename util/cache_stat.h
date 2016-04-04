/*
 * cache_stat.h
 *
 *  Created on: Apr 2, 2016
 *      Author: teng
 */

#ifndef UTIL_CACHE_STAT_H_
#define UTIL_CACHE_STAT_H_

namespace leveldb{


	void updateCache_stat(int kvcache, int blockcache, int hdd);

}


#endif /* UTIL_CACHE_STAT_H_ */
