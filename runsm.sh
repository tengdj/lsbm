#!/bin/bash

~/dlsm/db_bench \
--db=/mnt/hdd/sm --monitor_log=/tmp/mlog --max_print_level=7 --dlsm_end_level=6  --level0_max_score=1.3 --compaction_min_score=0.99 \
--range_threads=0 --range_size=100 --range_reads=-1 --dbmode=2 --print_version_info=0 --print_lazy_version_info=0 --hitratio_interval=1 \
--hot_ratio=100 --read_portion=100 --num=-1 --throughput=-1 --benchmarks=separate \
\
 --read_key_from=0 --read_key_upto=1000000 --write_key_from=0 --write_key_upto=104857600 --key_from=0 --key_upto=104857600 \
 --block_cache_size=4000 --key_cache_size=0 --warmup=1 --read_workload=uniform --write_workload=uniform --readspeed=-1 --writespeed=-1 \
--random_threads=1 --random_reads=-1 --writes=1 --countdown=100 --noise_percent=0 --pre_caching=0 \

