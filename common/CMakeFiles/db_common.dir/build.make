# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.10

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /shunzi/lsbm

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /shunzi/lsbm

# Include any dependencies generated for this target.
include common/CMakeFiles/db_common.dir/depend.make

# Include the progress variables for this target.
include common/CMakeFiles/db_common.dir/progress.make

# Include the compile flags for this target's objects.
include common/CMakeFiles/db_common.dir/flags.make

common/CMakeFiles/db_common.dir/dbformat.cc.o: common/CMakeFiles/db_common.dir/flags.make
common/CMakeFiles/db_common.dir/dbformat.cc.o: common/dbformat.cc
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/shunzi/lsbm/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object common/CMakeFiles/db_common.dir/dbformat.cc.o"
	cd /shunzi/lsbm/common && g++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/db_common.dir/dbformat.cc.o -c /shunzi/lsbm/common/dbformat.cc

common/CMakeFiles/db_common.dir/dbformat.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/db_common.dir/dbformat.cc.i"
	cd /shunzi/lsbm/common && g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /shunzi/lsbm/common/dbformat.cc > CMakeFiles/db_common.dir/dbformat.cc.i

common/CMakeFiles/db_common.dir/dbformat.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/db_common.dir/dbformat.cc.s"
	cd /shunzi/lsbm/common && g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /shunzi/lsbm/common/dbformat.cc -o CMakeFiles/db_common.dir/dbformat.cc.s

common/CMakeFiles/db_common.dir/dbformat.cc.o.requires:

.PHONY : common/CMakeFiles/db_common.dir/dbformat.cc.o.requires

common/CMakeFiles/db_common.dir/dbformat.cc.o.provides: common/CMakeFiles/db_common.dir/dbformat.cc.o.requires
	$(MAKE) -f common/CMakeFiles/db_common.dir/build.make common/CMakeFiles/db_common.dir/dbformat.cc.o.provides.build
.PHONY : common/CMakeFiles/db_common.dir/dbformat.cc.o.provides

common/CMakeFiles/db_common.dir/dbformat.cc.o.provides.build: common/CMakeFiles/db_common.dir/dbformat.cc.o


common/CMakeFiles/db_common.dir/filename.cc.o: common/CMakeFiles/db_common.dir/flags.make
common/CMakeFiles/db_common.dir/filename.cc.o: common/filename.cc
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/shunzi/lsbm/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object common/CMakeFiles/db_common.dir/filename.cc.o"
	cd /shunzi/lsbm/common && g++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/db_common.dir/filename.cc.o -c /shunzi/lsbm/common/filename.cc

common/CMakeFiles/db_common.dir/filename.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/db_common.dir/filename.cc.i"
	cd /shunzi/lsbm/common && g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /shunzi/lsbm/common/filename.cc > CMakeFiles/db_common.dir/filename.cc.i

common/CMakeFiles/db_common.dir/filename.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/db_common.dir/filename.cc.s"
	cd /shunzi/lsbm/common && g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /shunzi/lsbm/common/filename.cc -o CMakeFiles/db_common.dir/filename.cc.s

common/CMakeFiles/db_common.dir/filename.cc.o.requires:

.PHONY : common/CMakeFiles/db_common.dir/filename.cc.o.requires

common/CMakeFiles/db_common.dir/filename.cc.o.provides: common/CMakeFiles/db_common.dir/filename.cc.o.requires
	$(MAKE) -f common/CMakeFiles/db_common.dir/build.make common/CMakeFiles/db_common.dir/filename.cc.o.provides.build
.PHONY : common/CMakeFiles/db_common.dir/filename.cc.o.provides

common/CMakeFiles/db_common.dir/filename.cc.o.provides.build: common/CMakeFiles/db_common.dir/filename.cc.o


common/CMakeFiles/db_common.dir/generator.cc.o: common/CMakeFiles/db_common.dir/flags.make
common/CMakeFiles/db_common.dir/generator.cc.o: common/generator.cc
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/shunzi/lsbm/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building CXX object common/CMakeFiles/db_common.dir/generator.cc.o"
	cd /shunzi/lsbm/common && g++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/db_common.dir/generator.cc.o -c /shunzi/lsbm/common/generator.cc

common/CMakeFiles/db_common.dir/generator.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/db_common.dir/generator.cc.i"
	cd /shunzi/lsbm/common && g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /shunzi/lsbm/common/generator.cc > CMakeFiles/db_common.dir/generator.cc.i

common/CMakeFiles/db_common.dir/generator.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/db_common.dir/generator.cc.s"
	cd /shunzi/lsbm/common && g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /shunzi/lsbm/common/generator.cc -o CMakeFiles/db_common.dir/generator.cc.s

common/CMakeFiles/db_common.dir/generator.cc.o.requires:

.PHONY : common/CMakeFiles/db_common.dir/generator.cc.o.requires

common/CMakeFiles/db_common.dir/generator.cc.o.provides: common/CMakeFiles/db_common.dir/generator.cc.o.requires
	$(MAKE) -f common/CMakeFiles/db_common.dir/build.make common/CMakeFiles/db_common.dir/generator.cc.o.provides.build
.PHONY : common/CMakeFiles/db_common.dir/generator.cc.o.provides

common/CMakeFiles/db_common.dir/generator.cc.o.provides.build: common/CMakeFiles/db_common.dir/generator.cc.o


common/CMakeFiles/db_common.dir/log_reader.cc.o: common/CMakeFiles/db_common.dir/flags.make
common/CMakeFiles/db_common.dir/log_reader.cc.o: common/log_reader.cc
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/shunzi/lsbm/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Building CXX object common/CMakeFiles/db_common.dir/log_reader.cc.o"
	cd /shunzi/lsbm/common && g++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/db_common.dir/log_reader.cc.o -c /shunzi/lsbm/common/log_reader.cc

common/CMakeFiles/db_common.dir/log_reader.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/db_common.dir/log_reader.cc.i"
	cd /shunzi/lsbm/common && g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /shunzi/lsbm/common/log_reader.cc > CMakeFiles/db_common.dir/log_reader.cc.i

common/CMakeFiles/db_common.dir/log_reader.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/db_common.dir/log_reader.cc.s"
	cd /shunzi/lsbm/common && g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /shunzi/lsbm/common/log_reader.cc -o CMakeFiles/db_common.dir/log_reader.cc.s

common/CMakeFiles/db_common.dir/log_reader.cc.o.requires:

.PHONY : common/CMakeFiles/db_common.dir/log_reader.cc.o.requires

common/CMakeFiles/db_common.dir/log_reader.cc.o.provides: common/CMakeFiles/db_common.dir/log_reader.cc.o.requires
	$(MAKE) -f common/CMakeFiles/db_common.dir/build.make common/CMakeFiles/db_common.dir/log_reader.cc.o.provides.build
.PHONY : common/CMakeFiles/db_common.dir/log_reader.cc.o.provides

common/CMakeFiles/db_common.dir/log_reader.cc.o.provides.build: common/CMakeFiles/db_common.dir/log_reader.cc.o


common/CMakeFiles/db_common.dir/log_writer.cc.o: common/CMakeFiles/db_common.dir/flags.make
common/CMakeFiles/db_common.dir/log_writer.cc.o: common/log_writer.cc
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/shunzi/lsbm/CMakeFiles --progress-num=$(CMAKE_PROGRESS_5) "Building CXX object common/CMakeFiles/db_common.dir/log_writer.cc.o"
	cd /shunzi/lsbm/common && g++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/db_common.dir/log_writer.cc.o -c /shunzi/lsbm/common/log_writer.cc

common/CMakeFiles/db_common.dir/log_writer.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/db_common.dir/log_writer.cc.i"
	cd /shunzi/lsbm/common && g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /shunzi/lsbm/common/log_writer.cc > CMakeFiles/db_common.dir/log_writer.cc.i

common/CMakeFiles/db_common.dir/log_writer.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/db_common.dir/log_writer.cc.s"
	cd /shunzi/lsbm/common && g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /shunzi/lsbm/common/log_writer.cc -o CMakeFiles/db_common.dir/log_writer.cc.s

common/CMakeFiles/db_common.dir/log_writer.cc.o.requires:

.PHONY : common/CMakeFiles/db_common.dir/log_writer.cc.o.requires

common/CMakeFiles/db_common.dir/log_writer.cc.o.provides: common/CMakeFiles/db_common.dir/log_writer.cc.o.requires
	$(MAKE) -f common/CMakeFiles/db_common.dir/build.make common/CMakeFiles/db_common.dir/log_writer.cc.o.provides.build
.PHONY : common/CMakeFiles/db_common.dir/log_writer.cc.o.provides

common/CMakeFiles/db_common.dir/log_writer.cc.o.provides.build: common/CMakeFiles/db_common.dir/log_writer.cc.o


common/CMakeFiles/db_common.dir/params.cc.o: common/CMakeFiles/db_common.dir/flags.make
common/CMakeFiles/db_common.dir/params.cc.o: common/params.cc
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/shunzi/lsbm/CMakeFiles --progress-num=$(CMAKE_PROGRESS_6) "Building CXX object common/CMakeFiles/db_common.dir/params.cc.o"
	cd /shunzi/lsbm/common && g++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/db_common.dir/params.cc.o -c /shunzi/lsbm/common/params.cc

common/CMakeFiles/db_common.dir/params.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/db_common.dir/params.cc.i"
	cd /shunzi/lsbm/common && g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /shunzi/lsbm/common/params.cc > CMakeFiles/db_common.dir/params.cc.i

common/CMakeFiles/db_common.dir/params.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/db_common.dir/params.cc.s"
	cd /shunzi/lsbm/common && g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /shunzi/lsbm/common/params.cc -o CMakeFiles/db_common.dir/params.cc.s

common/CMakeFiles/db_common.dir/params.cc.o.requires:

.PHONY : common/CMakeFiles/db_common.dir/params.cc.o.requires

common/CMakeFiles/db_common.dir/params.cc.o.provides: common/CMakeFiles/db_common.dir/params.cc.o.requires
	$(MAKE) -f common/CMakeFiles/db_common.dir/build.make common/CMakeFiles/db_common.dir/params.cc.o.provides.build
.PHONY : common/CMakeFiles/db_common.dir/params.cc.o.provides

common/CMakeFiles/db_common.dir/params.cc.o.provides.build: common/CMakeFiles/db_common.dir/params.cc.o


common/CMakeFiles/db_common.dir/memtable.cc.o: common/CMakeFiles/db_common.dir/flags.make
common/CMakeFiles/db_common.dir/memtable.cc.o: common/memtable.cc
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/shunzi/lsbm/CMakeFiles --progress-num=$(CMAKE_PROGRESS_7) "Building CXX object common/CMakeFiles/db_common.dir/memtable.cc.o"
	cd /shunzi/lsbm/common && g++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/db_common.dir/memtable.cc.o -c /shunzi/lsbm/common/memtable.cc

common/CMakeFiles/db_common.dir/memtable.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/db_common.dir/memtable.cc.i"
	cd /shunzi/lsbm/common && g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /shunzi/lsbm/common/memtable.cc > CMakeFiles/db_common.dir/memtable.cc.i

common/CMakeFiles/db_common.dir/memtable.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/db_common.dir/memtable.cc.s"
	cd /shunzi/lsbm/common && g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /shunzi/lsbm/common/memtable.cc -o CMakeFiles/db_common.dir/memtable.cc.s

common/CMakeFiles/db_common.dir/memtable.cc.o.requires:

.PHONY : common/CMakeFiles/db_common.dir/memtable.cc.o.requires

common/CMakeFiles/db_common.dir/memtable.cc.o.provides: common/CMakeFiles/db_common.dir/memtable.cc.o.requires
	$(MAKE) -f common/CMakeFiles/db_common.dir/build.make common/CMakeFiles/db_common.dir/memtable.cc.o.provides.build
.PHONY : common/CMakeFiles/db_common.dir/memtable.cc.o.provides

common/CMakeFiles/db_common.dir/memtable.cc.o.provides.build: common/CMakeFiles/db_common.dir/memtable.cc.o


common/CMakeFiles/db_common.dir/table_cache.cc.o: common/CMakeFiles/db_common.dir/flags.make
common/CMakeFiles/db_common.dir/table_cache.cc.o: common/table_cache.cc
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/shunzi/lsbm/CMakeFiles --progress-num=$(CMAKE_PROGRESS_8) "Building CXX object common/CMakeFiles/db_common.dir/table_cache.cc.o"
	cd /shunzi/lsbm/common && g++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/db_common.dir/table_cache.cc.o -c /shunzi/lsbm/common/table_cache.cc

common/CMakeFiles/db_common.dir/table_cache.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/db_common.dir/table_cache.cc.i"
	cd /shunzi/lsbm/common && g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /shunzi/lsbm/common/table_cache.cc > CMakeFiles/db_common.dir/table_cache.cc.i

common/CMakeFiles/db_common.dir/table_cache.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/db_common.dir/table_cache.cc.s"
	cd /shunzi/lsbm/common && g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /shunzi/lsbm/common/table_cache.cc -o CMakeFiles/db_common.dir/table_cache.cc.s

common/CMakeFiles/db_common.dir/table_cache.cc.o.requires:

.PHONY : common/CMakeFiles/db_common.dir/table_cache.cc.o.requires

common/CMakeFiles/db_common.dir/table_cache.cc.o.provides: common/CMakeFiles/db_common.dir/table_cache.cc.o.requires
	$(MAKE) -f common/CMakeFiles/db_common.dir/build.make common/CMakeFiles/db_common.dir/table_cache.cc.o.provides.build
.PHONY : common/CMakeFiles/db_common.dir/table_cache.cc.o.provides

common/CMakeFiles/db_common.dir/table_cache.cc.o.provides.build: common/CMakeFiles/db_common.dir/table_cache.cc.o


common/CMakeFiles/db_common.dir/write_batch.cc.o: common/CMakeFiles/db_common.dir/flags.make
common/CMakeFiles/db_common.dir/write_batch.cc.o: common/write_batch.cc
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/shunzi/lsbm/CMakeFiles --progress-num=$(CMAKE_PROGRESS_9) "Building CXX object common/CMakeFiles/db_common.dir/write_batch.cc.o"
	cd /shunzi/lsbm/common && g++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/db_common.dir/write_batch.cc.o -c /shunzi/lsbm/common/write_batch.cc

common/CMakeFiles/db_common.dir/write_batch.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/db_common.dir/write_batch.cc.i"
	cd /shunzi/lsbm/common && g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /shunzi/lsbm/common/write_batch.cc > CMakeFiles/db_common.dir/write_batch.cc.i

common/CMakeFiles/db_common.dir/write_batch.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/db_common.dir/write_batch.cc.s"
	cd /shunzi/lsbm/common && g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /shunzi/lsbm/common/write_batch.cc -o CMakeFiles/db_common.dir/write_batch.cc.s

common/CMakeFiles/db_common.dir/write_batch.cc.o.requires:

.PHONY : common/CMakeFiles/db_common.dir/write_batch.cc.o.requires

common/CMakeFiles/db_common.dir/write_batch.cc.o.provides: common/CMakeFiles/db_common.dir/write_batch.cc.o.requires
	$(MAKE) -f common/CMakeFiles/db_common.dir/build.make common/CMakeFiles/db_common.dir/write_batch.cc.o.provides.build
.PHONY : common/CMakeFiles/db_common.dir/write_batch.cc.o.provides

common/CMakeFiles/db_common.dir/write_batch.cc.o.provides.build: common/CMakeFiles/db_common.dir/write_batch.cc.o


# Object files for target db_common
db_common_OBJECTS = \
"CMakeFiles/db_common.dir/dbformat.cc.o" \
"CMakeFiles/db_common.dir/filename.cc.o" \
"CMakeFiles/db_common.dir/generator.cc.o" \
"CMakeFiles/db_common.dir/log_reader.cc.o" \
"CMakeFiles/db_common.dir/log_writer.cc.o" \
"CMakeFiles/db_common.dir/params.cc.o" \
"CMakeFiles/db_common.dir/memtable.cc.o" \
"CMakeFiles/db_common.dir/table_cache.cc.o" \
"CMakeFiles/db_common.dir/write_batch.cc.o"

# External object files for target db_common
db_common_EXTERNAL_OBJECTS =

common/libdb_common.a: common/CMakeFiles/db_common.dir/dbformat.cc.o
common/libdb_common.a: common/CMakeFiles/db_common.dir/filename.cc.o
common/libdb_common.a: common/CMakeFiles/db_common.dir/generator.cc.o
common/libdb_common.a: common/CMakeFiles/db_common.dir/log_reader.cc.o
common/libdb_common.a: common/CMakeFiles/db_common.dir/log_writer.cc.o
common/libdb_common.a: common/CMakeFiles/db_common.dir/params.cc.o
common/libdb_common.a: common/CMakeFiles/db_common.dir/memtable.cc.o
common/libdb_common.a: common/CMakeFiles/db_common.dir/table_cache.cc.o
common/libdb_common.a: common/CMakeFiles/db_common.dir/write_batch.cc.o
common/libdb_common.a: common/CMakeFiles/db_common.dir/build.make
common/libdb_common.a: common/CMakeFiles/db_common.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/shunzi/lsbm/CMakeFiles --progress-num=$(CMAKE_PROGRESS_10) "Linking CXX static library libdb_common.a"
	cd /shunzi/lsbm/common && $(CMAKE_COMMAND) -P CMakeFiles/db_common.dir/cmake_clean_target.cmake
	cd /shunzi/lsbm/common && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/db_common.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
common/CMakeFiles/db_common.dir/build: common/libdb_common.a

.PHONY : common/CMakeFiles/db_common.dir/build

common/CMakeFiles/db_common.dir/requires: common/CMakeFiles/db_common.dir/dbformat.cc.o.requires
common/CMakeFiles/db_common.dir/requires: common/CMakeFiles/db_common.dir/filename.cc.o.requires
common/CMakeFiles/db_common.dir/requires: common/CMakeFiles/db_common.dir/generator.cc.o.requires
common/CMakeFiles/db_common.dir/requires: common/CMakeFiles/db_common.dir/log_reader.cc.o.requires
common/CMakeFiles/db_common.dir/requires: common/CMakeFiles/db_common.dir/log_writer.cc.o.requires
common/CMakeFiles/db_common.dir/requires: common/CMakeFiles/db_common.dir/params.cc.o.requires
common/CMakeFiles/db_common.dir/requires: common/CMakeFiles/db_common.dir/memtable.cc.o.requires
common/CMakeFiles/db_common.dir/requires: common/CMakeFiles/db_common.dir/table_cache.cc.o.requires
common/CMakeFiles/db_common.dir/requires: common/CMakeFiles/db_common.dir/write_batch.cc.o.requires

.PHONY : common/CMakeFiles/db_common.dir/requires

common/CMakeFiles/db_common.dir/clean:
	cd /shunzi/lsbm/common && $(CMAKE_COMMAND) -P CMakeFiles/db_common.dir/cmake_clean.cmake
.PHONY : common/CMakeFiles/db_common.dir/clean

common/CMakeFiles/db_common.dir/depend:
	cd /shunzi/lsbm && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /shunzi/lsbm /shunzi/lsbm/common /shunzi/lsbm /shunzi/lsbm/common /shunzi/lsbm/common/CMakeFiles/db_common.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : common/CMakeFiles/db_common.dir/depend
