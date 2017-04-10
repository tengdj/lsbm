
include_directories(../common)

add_library(db_lsmcb 
		builder.cc
		db_impl.cc
		db_iter.cc
		version_edit.cc
		version_set.cc
		)
add_executable(db_bench
		db_bench.cc
)
target_link_libraries(db_lsmcb db_common)

target_link_libraries(db_bench db_lsmcb)
