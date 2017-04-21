#!/bin/bash

~/dlsm/db_bench \
--db=/mnt/hdd/dlsm_range --monitor_log=/tmp/mlog --compaction_min_score=0.99 --compaction_buffer_management_interval=80001 --compaction_buffer_management_interval=100002 --compaction_buffer_management_interval=100003 \
--compaction_buffer_length=1 --compaction_buffer_length=2 --compaction_buffer_length=3 \
--range_threads=6 --range_size=100 --range_reads=-1  --max_print_level=3  --print_version_info=1 --print_compaction_buffer=1 --print_dash=1 --hitratio_interval=1 \
--hot_ratio=100 --read_portion=100 --num=-1 --throughput=-1 --benchmarks=separate --preload_metadata=1 --open_files=7000  --hot_file_threshold=2000\
\
 --read_key_from=0 --read_key_upto=12485760 --write_key_from=0 --write_key_upto=12485760 --key_from=2048576 --key_upto=20485760 \
 --block_cache_size=4000 --key_cache_size=0 --warmup=0 --read_workload=uniform --write_workload=uniform --readspeed=-1 --writespeed=1000 \
--random_threads=0 --random_reads=-1 --writes=-1 --countdown=10000 --noise_percent=2
