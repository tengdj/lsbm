
add_library(db_common 
		dbformat.cc
		filename.cc
		generator.cc
		log_reader.cc
		log_writer.cc
		params.cc
		memtable.cc
		table_cache.cc
		write_batch.cc
		)
target_link_libraries(db_common port table util snappy pthread)
