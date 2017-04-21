#!/bin/bash

lsbm/db_bench \
--db=/home/teng/lsbmdb --monitor_log=/tmp/mlog --compaction_min_score=0 --file_size=16 --data_merged_each_round=100 \
--compaction_buffer_length=1 --compaction_buffer_length=2 --compaction_buffer_length=3 \
--range_portion=0 --range_size=100 --max_print_level=3  --print_version_info=1 --print_compaction_buffer=1 --print_dash=1 --hitratio_interval=1 \
--hot_ratio=100 --read_portion=100 --num=-1 --throughput=-1 --benchmarks=separate --preload_metadata=1 --open_files=20000 --hot_file_threshold=1000 \
\
 --read_key_from=0 --read_key_upto=2097152 --write_key_from=0 --write_key_upto=20971520 --key_from=0 --key_upto=20971520 \
 --block_cache_size=4500 --key_cache_size=0 --warmup=1 --read_workload=uniform --write_workload=rcounter --readspeed=-1 --writespeed=-1 \
--read_threads=0 --random_reads=-1 --writes=20971520 --countdown=25000 --noise_percent=0  --buffered_merge=0
