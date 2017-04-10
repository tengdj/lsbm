#!/bin/bash

~/dlsm/db_bench \
--db=/mnt/hdd/dlsm --monitor_log=/tmp/mlog --compaction_min_score=0.99 --compaction_buffer_use_length=1 --compaction_buffer_use_length=252 --compaction_buffer_use_length=3 \
--range_threads=0 --range_size=100 --range_reads=-1  --max_print_level=3  --print_version_info=1 --print_compaction_buffer=1 --print_dash=1 --hitratio_interval=1 \
--hot_ratio=100 --read_portion=100 --num=-1 --throughput=-1 --benchmarks=separate \
\
 --read_key_from=0 --read_key_upto=10485760 --write_key_from=0 --write_key_upto=12485760 --key_from=0 --key_upto=104857600 \
 --block_cache_size=3000 --key_cache_size=0 --warmup=0 --read_workload=zipfian --write_workload=uniform --readspeed=-1 --writespeed=1000 \
--random_threads=1 --random_reads=-1 --writes=-1 --countdown=25000 --noise_percent=0 --pre_caching=0


