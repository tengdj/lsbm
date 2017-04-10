/*
 * cache_stat.cc
 *
 *  Created on: Apr 2, 2016
 *      Author: teng
 */

#include "leveldb/env.h"
#include "port/port_posix.h"
#include <stdio.h>

#include "leveldb/params.h"

namespace leveldb{


static int total_served = 0;
static int kvcache_served = 0;
static int blockcache_served = 0;
static int oscache_served = 0;
static int hdd_served = 0;

static int prev_total_served = 0;
static int prev_kvcache_served = 0;
static int prev_blockcache_served = 0;
static int prev_oscache_served = 0;
static int prev_hdd_served = 0;

static port::Mutex stats_mu_;

static time_t start = 0;

void updateCache_stat(int kvcache, int blockcache, int oscache, int hdd, double keycache_used, double blockcache_used){

	  time_t now;
	  stats_mu_.Lock();

	  total_served += kvcache+blockcache+hdd;
	  kvcache_served += kvcache;
	  blockcache_served += blockcache;
	  hdd_served += hdd;
	  oscache_served += oscache;
	  time(&now);
	  if(difftime(now,start)>=runtime::hitratio_interval){
		  int gap_total_served = total_served - prev_total_served;
		  int gap_kvcache_served = kvcache_served - prev_kvcache_served;
		  int gap_blockcache_served = blockcache_served - prev_blockcache_served;
		  int gap_hdd_served = hdd_served - prev_hdd_served;
		  int gap_oscache_served = oscache_served - prev_oscache_served;

		  prev_total_served = total_served;
		  prev_kvcache_served = kvcache_served;
		  prev_oscache_served = oscache_served;
		  prev_blockcache_served = blockcache_served;
		  prev_hdd_served = hdd_served;

	      if(gap_total_served!=0){
	    	  //fprintf(stdout,"total: %8d kvcache: %2.2f %2.2f (%8d,%8d) blockcache: %2.4f %2.4f(%8d,%8d) disk:%2.4f (%8d,%8d) time: %d\n",
	    	    fprintf(stdout,"total: |%9d| kvcache: |%2.4f|%2.4f|%6d|%9d| blockcache: |%2.4f|%2.4f|%6d|%9d| disk:|%2.4f|%2.4f|%6d|%6d|%9d| time:|%5d|\n",

	    		 total_served,
				 (double)gap_kvcache_served/gap_total_served,keycache_used,gap_kvcache_served,kvcache_served,
				 (double)gap_blockcache_served/gap_total_served,blockcache_used,gap_blockcache_served,blockcache_served,
	    		 (double)gap_hdd_served/gap_total_served,gap_hdd_served==0?0:(double)gap_oscache_served/gap_hdd_served,gap_oscache_served,gap_hdd_served,hdd_served,
				 (int)difftime(now,runtime::global_time_begin_));
	      }
	      time(&start);
		}
		stats_mu_.Unlock();
}

}


