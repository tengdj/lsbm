#!/bin/bash

~/lsmcb/db_bench \
--db=/mnt/hdd/lsmcb --monitor_log=/tmp/mlog --compaction_min_score=0.99  \
--compaction_buffer_length=231 --compaction_buffer_length=232 --compaction_buffer_length=13 --data_merged_each_round=8 \
--range_portion=0 --range_size=100 --max_print_level=3  --print_version_info=1 --print_compaction_buffer=1 --print_dash=1 --hitratio_interval=1 \
--hot_ratio=100 --read_portion=100 --num=-1 --throughput=-1 --benchmarks=separate --preload_metadata=0 --open_files=20000 --hot_file_threshold=2000 \
\
 --read_key_from=0 --read_key_upto=2000000 --write_key_from=0 --write_key_upto=20485760 --key_from=0 --key_upto=20485760 \
 --block_cache_size=4500 --key_cache_size=0 --warmup=1 --read_workload=uniform --write_workload=uniform --readspeed=-1 --writespeed=-1 \
--read_threads=0 --random_reads=-1 --writes=11485760 --countdown=25000 --noise_percent=2  --manage_compaction_buffer=0
