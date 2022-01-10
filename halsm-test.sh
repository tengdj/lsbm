#!/bin/bash

# use_existing_db - 0、1
# compaction_min_score - leveldb 默认为 1，原测试大量使用了 0.99
# file_size - SST 大小 2MB，该方案的 block size 为 4KB
# write_buffer_size - Memtable 大小 64M
# data_merged_each_round - 每轮合并的数据量大小 单位 MB，该测试中大量使用了 8MB
# compaction_buffer_length - 一个数组，这里对应了 1-23， 2-23， 3-1，即 【0，23，23，1】的配置，
#                            每个 Level buffer 容纳的最大文件个数
# range_portion - 范围查询的比例，0-100
# range_size - 范围查询的大小：一般设置为 100
# max_print_level - 输出信息的最大 level，一般设置为 3
# print_version_info - 是否要输出对应的版本信息，即每一层的 DELETION_PART、INSERTION_PART、COMPACTION_BUFFER、WARMINGUP_BUFFER
# print_compaction_buffer - 实际没用
# print_dash - 对于 version 输出的信息的微调
# hitratio_interval - 命中率信息统计的间隔时间，单位 s
# hot_ratio - 一般设置为 100，对应在程序里变为 100/n=1，即 hotratio% 为热数据
# read_portion - 读写混和操作的读数据比例
# num - 读写混合操作的总数量
# throughput - 原测试中大量设置为了 -1，
# benchmarks - 两种负载类型，seperate 和 mix，对应只读只写和混和读写
# preload_metadata - 开启了的话，即为 1，在 Recover 时会加载 Table 的元数据
# open_files - LevelDB 最大打开文件数，我们其他实验中配置为了 1000
# hot_file_threshold - 判断一个 SST 是否为热表的阈值，即有对应多少个数据块被缓存即被认为是热表
#                    - 原文设置的 SST 大小为 16M，对应有 4096 个 Block，2000 个即为热表
#                    - 我们的 SST 大小为 2M，对应有 512 个 Block，可以考虑设置为 256 个即为热表

# read_key_from - 读负载的起始 key
# read_key_upto - 读负载的结束 Key
# write_key_from - 写负载的起始 key
# write_key_upto - 写负载的结束 Key
# --key_from=0 --key_upto=10485760 noise 负载的起始
# block_cache_size - 块缓存的大小，单位 MB
# key_cache_size - KV 缓存的大小，单位 MB
# warmup - 是否要预热
# read_workload - 读负载的分布，只读和混和读写都有有四种读分布
# zipfian_constant - 负载的系数
# write_workload - 写负载的分布，只写负载有两种分布，混和读写有四种分布
# readspeed - 对应 FLAGS_read_throughput，原测试中基本都设置为了 -1，如果为 0 那么将不会进行读操作
#           - 本身也有读速度的限制，只是原方案将代码注释掉了
# writespeed - 限制写操作的速度，太快了就休眠一会儿
# read_threads - 读线程数
# random_reads - 点查询个数
# writes - 写操作个数
# countdown - 测试的最长时间，单位 s
# noise_percent - 噪点数据比例，主要是针对不存在的数据
# buffered_merge - 是否开启 buffered_merge，0不开启，1开启

# Load - uniform
# rm -rf /home/shunzi/leveldb_test/lsbmdb/temp
# lsbm/db_bench \
# --db=/home/shunzi/leveldb_test/lsbmdb/temp --use_existing_db=0 --monitor_log=/home/shunzi/leveldb_test/lsbmdb/loadlog --compaction_min_score=1 --file_size=2 \
# --write_buffer_size=64 --data_merged_each_round=8 \
# --compaction_buffer_length=231 --compaction_buffer_length=232 --compaction_buffer_length=13 \
# --range_portion=0 --range_size=100 --max_print_level=3  --print_version_info=1 --print_compaction_buffer=1 --print_dash=1 --hitratio_interval=1 \
# --hot_ratio=100 --read_portion=100 --num=-1 --throughput=-1 --benchmarks=separate --preload_metadata=1 --open_files=1000 --hot_file_threshold=256 \
# \
#  --read_key_from=0 --read_key_upto=10485760 --write_key_from=0 --write_key_upto=10485760 --key_from=0 --key_upto=10485760 \
#  --block_cache_size=270 --key_cache_size=0 --warmup=0 --read_workload=zipfian --zipfian_constant=0.99 --write_workload=uniform --readspeed=-1 --writespeed=-1 \
# --read_threads=0 --random_reads=-1 --writes=10485760 --countdown=25000 --noise_percent=0  --buffered_merge=1

# Load - zipfian
# 需要使用 mix 才能对写操作指定 zipfian 分布
# Mix 时候几个关键参数
# --benchmarks=mix
# --throughput=-1
# --num 读写混合操作数量 本例中设置为 10485760
# --read_portion 读操作比例，因为是 load 设置为 0
# --range_portion 范围查询操作比例，因为是 load 设置为 0
# rm -rf /home/shunzi/leveldb_test/lsbmdb/temp-zip
# lsbm/db_bench \
# --db=/home/shunzi/leveldb_test/lsbmdb/temp-zip --use_existing_db=0 --monitor_log=/home/shunzi/leveldb_test/lsbmdb/loadlog-zip --compaction_min_score=1 --file_size=2 \
# --write_buffer_size=64 --data_merged_each_round=8 \
# --compaction_buffer_length=231 --compaction_buffer_length=232 --compaction_buffer_length=13 \
# --range_portion=0 --range_size=100 --max_print_level=3  --print_version_info=1 --print_compaction_buffer=1 --print_dash=1 --hitratio_interval=1 \
# --hot_ratio=100 --read_portion=0 --num=10485760 --throughput=-1 --benchmarks=mix --preload_metadata=1 --open_files=1000 --hot_file_threshold=256 \
# \
#  --read_key_from=0 --read_key_upto=10485760 --write_key_from=0 --write_key_upto=10485760 --key_from=0 --key_upto=10485760 \
#  --block_cache_size=270 --key_cache_size=0 --warmup=0 --read_workload=zipfian --zipfian_constant=0.99 --write_workload=zipfian --readspeed=-1 --writespeed=-1 \
# --read_threads=0 --random_reads=-1 --writes=10485760 --countdown=25000 --noise_percent=0  --buffered_merge=1


# YCSB-A 
# r:w = 0.5:0.5 
# zipfian 0.99
# 混和读写
# --benchmarks=mix
# --throughput=-1
# --use_existing_db=1
# --num 读写混合操作数量 本例中设置为 10485760
# --read_portion 读操作比例，设置为 50
# --range_portion 范围查询操作比例，设置为 0
# --hot_ratio=100 全都是热数据，即按照 zipfian 去访问

# cp -rf /home/shunzi/leveldb_test/lsbmdb/temp /home/shunzi/leveldb_test/lsbmdb/temp-test
# lsbm/db_bench \
# --db=/home/shunzi/leveldb_test/lsbmdb/temp-test --use_existing_db=1 --monitor_log=/home/shunzi/leveldb_test/lsbmdb/A-log --compaction_min_score=1 --file_size=2 \
# --write_buffer_size=64 --data_merged_each_round=8 \
# --compaction_buffer_length=231 --compaction_buffer_length=232 --compaction_buffer_length=13 \
# --range_portion=0 --range_size=100 --max_print_level=3  --print_version_info=1 --print_compaction_buffer=1 --print_dash=1 --hitratio_interval=1 \
# --hot_ratio=100 --read_portion=50 --num=10485760 --throughput=-1 --benchmarks=mix --preload_metadata=1 --open_files=1000 --hot_file_threshold=256 \
# \
#  --read_key_from=0 --read_key_upto=10485760 --write_key_from=0 --write_key_upto=10485760 --key_from=0 --key_upto=10485760 \
#  --block_cache_size=270 --key_cache_size=0 --warmup=0 --read_workload=zipfian --zipfian_constant=0.99 --write_workload=zipfian --readspeed=-1 --writespeed=-1 \
# --read_threads=0 --random_reads=-1 --writes=10485760 --countdown=25000 --noise_percent=0  --buffered_merge=1


# YCSB-B
# r:w 0.95:0.05
# zipfian 0.99
# 混和读写

# cp -rf /home/shunzi/leveldb_test/lsbmdb/temp /home/shunzi/leveldb_test/lsbmdb/temp-test
# lsbm/db_bench \
# --db=/home/shunzi/leveldb_test/lsbmdb/temp-test --use_existing_db=1 --monitor_log=/home/shunzi/leveldb_test/lsbmdb/B-log --compaction_min_score=1 --file_size=2 \
# --write_buffer_size=64 --data_merged_each_round=8 \
# --compaction_buffer_length=231 --compaction_buffer_length=232 --compaction_buffer_length=13 \
# --range_portion=0 --range_size=100 --max_print_level=3  --print_version_info=1 --print_compaction_buffer=1 --print_dash=1 --hitratio_interval=1 \
# --hot_ratio=100 --read_portion=95 --num=10485760 --throughput=-1 --benchmarks=mix --preload_metadata=1 --open_files=1000 --hot_file_threshold=256 \
# \
#  --read_key_from=0 --read_key_upto=10485760 --write_key_from=0 --write_key_upto=10485760 --key_from=0 --key_upto=10485760 \
#  --block_cache_size=270 --key_cache_size=0 --warmup=0 --read_workload=zipfian --zipfian_constant=0.99 --write_workload=zipfian --readspeed=-1 --writespeed=-1 \
# --read_threads=0 --random_reads=-1 --writes=10485760 --countdown=25000 --noise_percent=0  --buffered_merge=1

# YCSB-C
# r:w 1:0
# zipfian 0.99

# lsbm/db_bench \
# --db=/home/shunzi/leveldb_test/lsbmdb/temp-test --use_existing_db=1 --monitor_log=/home/shunzi/leveldb_test/lsbmdb/C-log --compaction_min_score=1 --file_size=2 \
# --write_buffer_size=64 --data_merged_each_round=8 \
# --compaction_buffer_length=231 --compaction_buffer_length=232 --compaction_buffer_length=13 \
# --range_portion=0 --range_size=100 --max_print_level=3  --print_version_info=1 --print_compaction_buffer=1 --print_dash=1 --hitratio_interval=1 \
# --hot_ratio=100 --read_portion=100 --num=10485760 --throughput=-1 --benchmarks=mix --preload_metadata=1 --open_files=1000 --hot_file_threshold=256 \
# \
#  --read_key_from=0 --read_key_upto=10485760 --write_key_from=0 --write_key_upto=10485760 --key_from=0 --key_upto=10485760 \
#  --block_cache_size=270 --key_cache_size=0 --warmup=0 --read_workload=zipfian --zipfian_constant=0.99 --write_workload=zipfian --readspeed=-1 --writespeed=-1 \
# --read_threads=0 --random_reads=-1 --writes=10485760 --countdown=25000 --noise_percent=0  --buffered_merge=1


# YCSB-D
# latest
# r:w 0.95:0.05

# lsbm/db_bench \
# --db=/home/shunzi/leveldb_test/lsbmdb/temp-test --use_existing_db=1 --monitor_log=/home/shunzi/leveldb_test/lsbmdb/D-log --compaction_min_score=1 --file_size=2 \
# --write_buffer_size=64 --data_merged_each_round=8 \
# --compaction_buffer_length=231 --compaction_buffer_length=232 --compaction_buffer_length=13 \
# --range_portion=0 --range_size=100 --max_print_level=3  --print_version_info=1 --print_compaction_buffer=1 --print_dash=1 --hitratio_interval=1 \
# --hot_ratio=100 --read_portion=95 --num=10485760 --throughput=-1 --benchmarks=mix --preload_metadata=1 --open_files=1000 --hot_file_threshold=256 \
# \
#  --read_key_from=0 --read_key_upto=10485760 --write_key_from=0 --write_key_upto=10485760 --key_from=0 --key_upto=10485760 \
#  --block_cache_size=270 --key_cache_size=0 --warmup=0 --read_workload=latest --write_workload=latest --readspeed=-1 --writespeed=-1 \
# --read_threads=0 --random_reads=-1 --writes=10485760 --countdown=25000 --noise_percent=0  --buffered_merge=1


# YCSB-E
# scan:w 0.95:0.05
# zipfian 0.99
# range_portion 只在单纯的只读负载中才会生效，所以为了模拟 YCSB-E，我们定制化修改一下 100
# read_portion 用于指定读操作的比例，YCSB-E 中读操作（range）占比 95%

cp -rf /home/shunzi/leveldb_test/lsbmdb/temp /home/shunzi/leveldb_test/lsbmdb/temp-test

lsbm/db_bench \
--db=/home/shunzi/leveldb_test/lsbmdb/temp-test --use_existing_db=1 --monitor_log=/home/shunzi/leveldb_test/lsbmdb/E-log --compaction_min_score=1 --file_size=2 \
--write_buffer_size=64 --data_merged_each_round=8 \
--compaction_buffer_length=231 --compaction_buffer_length=232 --compaction_buffer_length=13 \
--range_portion=100 --range_size=100 --max_print_level=3  --print_version_info=1 --print_compaction_buffer=1 --print_dash=1 --hitratio_interval=1 \
--hot_ratio=100 --read_portion=95 --num=10485760 --throughput=-1 --benchmarks=mix --preload_metadata=1 --open_files=1000 --hot_file_threshold=256 \
\
 --read_key_from=0 --read_key_upto=10485760 --write_key_from=0 --write_key_upto=10485760 --key_from=0 --key_upto=10485760 \
 --block_cache_size=270 --key_cache_size=0 --warmup=0 --read_workload=latest --write_workload=latest --readspeed=-1 --writespeed=-1 \
--read_threads=0 --random_reads=-1 --writes=10485760 --countdown=25000 --noise_percent=0  --buffered_merge=1


# YCSB-F