/*
 * cache_stat.cc
 *
 *  Created on: Apr 2, 2016
 *      Author: teng
 */

#include "leveldb/env.h"
#include "port/port_posix.h"
#include "db/dlsm_param.h"
#include <stdio.h>

namespace leveldb{


static int total_served = 0;
static int kvcache_served = 0;
static int blockcache_served = 0;
static int hdd_served = 0;

static int prev_total_served = 0;
static int prev_kvcache_served = 0;
static int prev_blockcache_served = 0;
static int prev_hdd_served = 0;

static port::Mutex stats_mu_;

static time_t start = 0;
static time_t begin = -1;
static bool begin_seted = false;

void updateCache_stat(int kvcache, int blockcache, int hdd){

	  if(!begin_seted){
		  begin_seted = true;
		  time(&begin);
	  }
	  time_t now;
	  stats_mu_.Lock();
	  total_served += kvcache+blockcache+hdd;
	  kvcache_served += kvcache;
	  blockcache_served += blockcache;
	  hdd_served += hdd;
	  time(&now);
	  if(difftime(now,start)>=runtime::hitratio_interval){
		  int gap_total_served = total_served - prev_total_served;
		  int gap_kvcache_served = kvcache_served - prev_kvcache_served;
		  int gap_blockcache_served = blockcache_served - prev_blockcache_served;
		  int gap_hdd_served = hdd_served - prev_hdd_served;

		  prev_total_served = total_served;
		  prev_kvcache_served = kvcache_served;
		  prev_blockcache_served = blockcache_served;
		  prev_hdd_served = hdd_served;

	      if(gap_total_served!=0){
	    	  printf("hitratio: total: %8d kvcache_served: %2.2f (%8d,%8d) blockcache_served: %2.2f (%8d,%8d) hdd_served:%2.2f (%8d,%8d) timepassed: %d\n",
	    		 total_served,
				 (double)gap_kvcache_served/gap_total_served,gap_kvcache_served,blockcache_served,
				 (double)gap_blockcache_served/gap_total_served,gap_blockcache_served,blockcache_served,
	    		 (double)gap_hdd_served/gap_total_served,gap_hdd_served,hdd_served,
				 (int)difftime(now,begin));
	      }
	      time(&start);
		}
		stats_mu_.Unlock();
	  }

}


