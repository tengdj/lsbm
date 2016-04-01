#!/bin/bash

ANALYZER=`dirname $0`/io-analysis.sh;
EXEC=`dirname $0`/../db_bench;

CONF=$1;
TDIR=$2;
TAIL_PARAMS="${*:3}";

MLOG=$TDIR/mlog;

MODE=Default;
FILE_SIZE=8;	# in MiB
VALUE_SIZE=1024; # default 100
BUFFER_SIZE=8388608; #16777216; 
THREADS=8;
LEVEL0_SIZE=100;
OPEN_FILES=64000;

READ_FROM=0;
WRITE_FROM=0;
READ_UPTO=104857600;
WRITE_UPTO=104857600;

USE_DB=0;
RRATIO=0;
STORE=/tmp;
SEC_STORAGE=/tmp;
NUM=20000000;
COUNTDOWN=-1;
BLOOM_BITS=15;
CACHE_SIZE=256; # guaranteed to fit one compaction in, and do not consume too much memory

RUN_COMPACTION=1;

WORKLOAD=uniform;
RTHROUGHPUT=1000000;
WTHROUGHPUT=1000000;
BENCHMARKS=rwrandom;
RANGESIZE=0.0001;
RANGETHREADS=0;
WRITENUM=-1;

prep_rwrandom() {
	ARGS="--db=$STORE --benchmarks=$BENCHMARKS --num=$NUM --use_existing_db=$USE_DB --value_size=$VALUE_SIZE --read_percent=$RRATIO --threads=$THREADS --read_key_from=$READ_FROM --read_key_upto=$READ_UPTO --write_key_from=$WRITE_FROM --write_key_upto=$WRITE_UPTO --write_buffer_size=$BUFFER_SIZE --open_files=$OPEN_FILES --bloom_bits=$BLOOM_BITS --hlsm_mode=$MODE --hlsm_secondary_storage_path=$SEC_STORAGE --level_ratio=$LEVEL_RATIO --file_size=$FILE_SIZE --histogram=1 --countdown=$COUNTDOWN --compression_ratio=1 --debug_level=$DEBUG_LEVEL --monitor_log=$MLOG --bloom_bits_use=$BLOOM_BITS_USE --level0_size=$LEVEL0_SIZE --preload_metadata=$PRELOAD_META --debug_file=/tmp/hlsm_log --max_level=$MAX_LEVEL --run_compaction=$RUN_COMPACTION --iterator_prefetch=$ITERATOR_PREFETCH --cache_size=$(($CACHE_SIZE * 1024 * 1024)) --raw_prefetch=$RAW_PREFETCH --restrict_level0_score=$RESTRICT_LEVEL0_SCORE --ycsb_compatible=$YCSB_COMPATIBLE --compaction_limit_mb_per_sec=$COMPACTION_LIMIT_MB_PER_SEC --level0_stop_write_trigger=$LEVEL0_STOP_WRITE_TRIGGER --workload=$WORKLOAD --readspeed=$RTHROUGHPUT --writespeed=$WTHROUGHPUT --sequential_read_from_primary=$SEQUENTIALONPRIMARY --random_read_from_primary=$RANDOMONPRIMARY  --range_size=$RANGESIZE --range_flexible=$RANGEFLEXIBLE --range_threads=$RANGETHREADS --writes=$WRITENUM ";
	echo "$EXEC $ARGS";
}


Default() {
	MODE=Default;
	
	prep_rwrandom;
	$EXEC $ARGS &
}

FullMirror() {
	MODE=FullMirror;
	
	prep_rwrandom;
	$EXEC $ARGS &
}

bLSM() {
	MODE=bLSM;
	
	prep_rwrandom;
	$EXEC $ARGS &
}

hLSM() {
	MODE=hLSM;
	
	prep_rwrandom;
	$EXEC $ARGS &
}

LSM(){
       MODE=LSM;
      
       prep_rwrandom;
       $EXEC $ARGS &
}
multi-instance() {
	INSTANCE_NUM=$1;
	echo "Num=$INSTANCE_NUM	Benchmark=$BENCHMARK"

	STORE_ROOT=$STORE;
	SEC_ROOT=$SEC_STORAGE;

	for i in `seq 1 $INSTANCE_NUM`; do
		if [ "$BUFFER_SIZES" != "" ]; then
			BUFFER_SIZE="$(echo $BUFFER_SIZES| awk -v ss=$i '{print $ss}')";
			echo $BUFFER_SIZE
		fi
			echo $BUFFER_SIZE

		SEC_STORAGE=$SEC_ROOT/${i};
		STORE=$STORE_ROOT/${i};
		echo $SEC_STORAGE $STORE;
		mkdir -p $SEC_STORAGE $STORE;

		$BENCHMARK;
		sleep 1;
	done
}

# entry
source $CONF; #test, arguments
cat $CONF;
$TEST $TAIL_PARAMS;

# sync
wait;

