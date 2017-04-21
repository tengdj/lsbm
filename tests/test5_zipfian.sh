#!/bin/bash

~/lsmcb/db_bench \
--db=/mnt/hdd/lsmcb --monitor_log=/tmp/mlog --compaction_min_score=0.99 --compaction_buffer_management_interval=801 --compaction_buffer_management_interval=5002 --compaction_buffer_management_interval=50003 \
--compaction_buffer_length=111 --compaction_buffer_length=182 --compaction_buffer_length=13 --data_merged_each_round=8 \
--compaction_buffer_use_length=111 --compaction_buffer_use_length=132 --compaction_buffer_length=13 \
--range_portion=0 --range_size=100 --max_print_level=3  --print_version_info=1 --print_compaction_buffer=1 --print_dash=1 --hitratio_interval=1 \
--hot_ratio=100 --read_portion=100 --num=-1 --throughput=-1 --benchmarks=separate --preload_metadata=1 --open_files=20000 --hot_file_threshold=2000 \
\
 --read_key_from=0 --read_key_upto=11485760 --write_key_from=0 --write_key_upto=11485760 --key_from=0 --key_upto=104857600 \
 --block_cache_size=4500 --key_cache_size=0 --warmup=0 --read_workload=zipfian --zipfian_constant=0.999 --write_workload=uniform --readspeed=-1 --writespeed=500 \
--read_threads=8 --random_reads=-1 --writes=-1 --countdown=25000 --noise_percent=2  --manage_compaction_buffer=0
