PROJECT(LSMCB)
cmake_minimum_required(VERSION 2.8)
set (CMAKE_CXX_COMPILER g++)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-builtin-memcmp -pthread -DOS_LINUX -DLEVELDB_PLATFORM_POSIX -DSNAPPY -O3 -g2 -DNDEBUG -std=c++11")#

#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

include_directories(include)
include_directories(.)

ADD_SUBDIRECTORY(common)
ADD_SUBDIRECTORY(port)
ADD_SUBDIRECTORY(table)
ADD_SUBDIRECTORY(util)
ADD_SUBDIRECTORY(lsbm)


configure_file(tests/writebase.sh writebase.sh COPYONLY)
configure_file(tests/writelsmcb.sh writelsmcb.sh COPYONLY)
configure_file(tests/blsm.sh blsm.sh COPYONLY)
configure_file(tests/blsm_range.sh blsm_range.sh COPYONLY)
configure_file(tests/blsm_zipfian.sh blsm_zipfian.sh COPYONLY)
configure_file(tests/blsm_oscache.sh blsm_oscache.sh COPYONLY)

configure_file(tests/lsmcb_zipfian.sh lsmcb_zipfian.sh COPYONLY)

configure_file(tests/blsm_kv.sh blsm_kv.sh COPYONLY)
configure_file(tests/incre.sh incre.sh COPYONLY)
configure_file(tests/blsm_sm.sh blsm_sm.sh COPYONLY)
configure_file(tests/lsbm.sh lsbm.sh COPYONLY)
configure_file(tests/lsmcb_range.sh lsmcb_range.sh COPYONLY)
configure_file(tests/blsmwithicre.sh blsmwithicre.sh COPYONLY)




