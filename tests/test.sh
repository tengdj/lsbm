#!/bin/bash

~/lsmcb/db_bench \
--db=/mnt/hdd/lsmcb --monitor_log=/tmp/mlog --compaction_min_score=0.99 --compaction_buffer_management_interval=101 --compaction_buffer_management_interval=5002 --compaction_buffer_management_interval=50003 \
--compaction_buffer_use_length=1 --compaction_buffer_use_length=2 --compaction_buffer_use_length=3 \
--range_portion=0 --range_size=100 --max_print_level=3  --print_version_info=1 --print_compaction_buffer=1 --print_dash=1 --hitratio_interval=1 \
--hot_ratio=100 --read_portion=100 --num=-1 --throughput=-1 --benchmarks=separate --preload_metadata=0 --open_files=60000 --hot_file_threshold=2000\
\
 --read_key_from=0 --read_key_upto=5048576 --write_key_from=0 --write_key_upto=11485760 --key_from=0 --key_upto=104857600 \
 --block_cache_size=9000 --key_cache_size=0 --warmup=1 --read_workload=uniform --write_workload=uniform --readspeed=-1 --writespeed=1000 \
--read_threads=8 --random_reads=-1 --writes=-1 --countdown=25000 --noise_percent=2
