#!/bin/bash

cmd="lsmcb/db_bench \
--db=/mnt/hdd/lsmcb --monitor_log=/tmp/mlog --compaction_min_score=0.99 --file_size=16 --write_buffer_size=40  --data_merged_each_round=8 \
--compaction_buffer_use_length=1 --compaction_buffer_use_length=2 --compaction_buffer_use_length=3 \
--range_portion=0 --range_size=100 --max_print_level=3  --print_version_info=1 --print_compaction_buffer=1 --print_dash=1 --hitratio_interval=1 \
--hot_ratio=100 --read_portion=100 --num=-1 --throughput=-1 --benchmarks=separate --preload_metadata=1 --open_files=30000 --hot_file_threshold=1000 \
\
 --read_key_from=0 --read_key_upto=3000000 --write_key_from=0 --write_key_upto=20971520 --key_from=0 --key_upto=20971520 \
 --block_cache_size=0 --key_cache_size=0 --warmup=1 --read_workload=uniform --write_workload=uniform --readspeed=-1 --writespeed=1000 \
--read_threads=8 --random_reads=-1 --writes=-1 --countdown=25000 --noise_percent=2  --buffered_merge=0"

echo "$cmd"&&$cmd

