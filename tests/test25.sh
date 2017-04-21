#!/bin/bash

~/dlsm/db_bench \
--db=/mnt/hdd/dlsm --monitor_log=/tmp/mlog --compaction_min_score=0.99 --compaction_buffer_management_interval=301 --compaction_buffer_management_interval=3002 --compaction_buffer_management_interval=80003 \
--compaction_buffer_length=111 --compaction_buffer_length=182 --compaction_buffer_length=3 \
--range_portion=100 --range_size=100 --max_print_level=3  --print_version_info=1 --print_compaction_buffer=1 --print_dash=1 --hitratio_interval=1 \
--hot_ratio=100 --read_portion=100 --num=-1 --throughput=-1 --benchmarks=separate --preload_metadata=1 --open_files=13000  --hot_file_threshold=2000\
\
 --read_key_from=0 --read_key_upto=2000000 --write_key_from=0 --write_key_upto=11485760 --key_from=4048576 --key_upto=11485760 \
 --block_cache_size=5000 --key_cache_size=0 --warmup=1 --read_workload=uniform --write_workload=uniform --readspeed=-1 --writespeed=1000 \
--read_threads=6 --random_reads=-1 --writes=-1 --countdown=20000 --noise_percent=2 --manage_compaction_buffer=1
