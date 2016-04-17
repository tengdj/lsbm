#!/bin/bash

~/dlsm/db_bench \
--db=/mnt/hdd/dlsm --monitor_log=/tmp/mlog --max_print_level=6 --dlsm_end_level=6  --level0_max_score=1.3 --compaction_min_score=0.99 \
--range_threads=0 --range_size=0.005 --range_reads=-1  --dbmode=0 --print_version_info=0 --print_lazy_version_info=0 --hitratio_interval=1 \
--bloom_bits=30 --bloom_bits_use=20 --hot_ratio=100 --read_portion=100 --num=-1 --throughput=-1 --benchmarks=separate \
\
 --read_key_from=0 --read_key_upto=1048576 --write_key_from=0 --write_key_upto=10485760 --key_from=0 --key_upto=15485760 \
 --block_cache_size=2000 --key_cache_size=0 --warmup=1 --read_workload=uniform --write_workload=uniform --readspeed=-1 --writespeed=1000 \
--random_threads=1 --random_reads=-1 --writes=-1 --countdown=150000  --hash_key=0 --noise_percent=10 --pre_caching=1
