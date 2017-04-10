
add_library(util 
		arena.cc
		bloom.cc
		cache_stat.cc
		cache.cc
		coding.cc
		comparator.cc
		crc32c.cc
		env_posix.cc
		env.cc
		filter_policy.cc
		hash.cc
		histogram.cc
		logging.cc
		options.cc
		status.cc
		testutil.cc
		)
#target_link_libraries(db pthread)
