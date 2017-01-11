// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_DB_VERSION_EDIT_H_
#define STORAGE_LEVELDB_DB_VERSION_EDIT_H_

#include <set>
#include <utility>
#include <vector>
#include "db/dbformat.h"

namespace leveldb {

class VersionSet;
class SortedTable;
enum SortedTableType{

	DELETION_PART = 0,
	INSERTION_PART = 1,
	COMPACTION_BUFFER = 2

};
struct FileMetaData {
  int refs;
  bool visible;//is it visible in compaction buffer
  uint64_t number;
  uint64_t file_size;         // File size in bytes
  InternalKey smallest;       // Smallest internal key served by table
  InternalKey largest;        // Largest internal key served by table

  FileMetaData() : refs(0), file_size(0), number(0), visible(true){ }
};

class VersionEdit {
 public:
  VersionEdit() { Clear(); }
  ~VersionEdit() { }

  void Clear();

  void CloneMeta(VersionEdit *edit){
	  edit->SetLastSequence(last_sequence_);
	  edit->SetLogNumber(log_number_);
	  edit->SetNextFile(next_file_number_);
	  edit->SetPrevLogNumber(prev_log_number_);
  }

  bool GetRefineCB(){
	  return refineCB;
  }
  int GetRefineLevel(){
	  return refineLevel;
  }
  void EnableRefineCB(bool enable,int level){
	 refineCB = enable;
	 refineLevel = level;
  }
  void SetComparatorName(const Slice& name) {
    has_comparator_ = true;
    comparator_ = name.ToString();
  }
  void SetLogNumber(uint64_t num) {
    has_log_number_ = true;
    log_number_ = num;
  }
  void SetPrevLogNumber(uint64_t num) {
    has_prev_log_number_ = true;
    prev_log_number_ = num;
  }
  void SetNextFile(uint64_t num) {
    has_next_file_number_ = true;
    next_file_number_ = num;
  }
  void SetLastSequence(SequenceNumber seq) {
    has_last_sequence_ = true;
    last_sequence_ = seq;
  }

  std::vector<std::pair<int, FileMetaData>>* GetNewFiles(){
	  return new_files_;
  }

  // Add the specified file at the specified number.
  // REQUIRES: This version has not been saved (see VersionSet::SaveTo)
  // REQUIRES: "smallest" and "largest" are smallest and largest keys in file
  void AddFile(SortedTableType type, int level, uint64_t file,
               uint64_t file_size,
               const InternalKey& smallest,
               const InternalKey& largest) {
    FileMetaData f;
    f.number = file;
    f.file_size = file_size;
    f.smallest = smallest;
    f.largest = largest;
    f.visible = true;
    new_files_[type].push_back(std::make_pair(level, f));
  }


  // Delete the specified "file" from the specified "level".
  void DeleteFile(SortedTableType type, int level, uint64_t file) {
	assert(type!=COMPACTION_BUFFER);//can only be deleted in batch in the level move part
    deleted_files_[type].insert(std::make_pair(level, file));
  }

  void EncodeTo(std::string* dst) const;
  Status DecodeFrom(const Slice& src);

  std::string DebugString() const;
  SortedTable* CloneSortedTable(SortedTable *);

 private:
  friend class VersionSet;
  friend class LazyVersionSet;
  friend class BasicVersionSet;

  typedef std::set< std::pair<int, uint64_t> > DeletedFileSet;

  std::string comparator_;
  uint64_t log_number_;
  uint64_t prev_log_number_;
  uint64_t next_file_number_;
  SequenceNumber last_sequence_;
  bool has_comparator_;
  bool has_log_number_;
  bool has_prev_log_number_;
  bool has_next_file_number_;
  bool has_last_sequence_;

  bool refineCB;
  int refineLevel;
  DeletedFileSet deleted_files_[3];
  std::vector< std::pair<int, FileMetaData> > new_files_[3];
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_VERSION_EDIT_H_
