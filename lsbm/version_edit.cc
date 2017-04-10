// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "version_edit.h"

#include "version_set.h"
#include "util/coding.h"

namespace leveldb {

// Tag numbers for serialized VersionEdit.  These numbers are written to
// disk and should not be changed.
enum Tag {
  kComparator           = 1,
  kLogNumber            = 2,
  kNextFileNumber       = 3,
  kLastSequence         = 4,
  kCompactPointer       = 5,
  kDeletedFile          = 6,
  kNewFile              = 7,
  // 8 was used for large value refs
  kPrevLogNumber        = 9,
  kWriteCursor			=10,
  kReadCursor			=11
};

void VersionEdit::Clear() {
  comparator_.clear();
  log_number_ = 0;
  prev_log_number_ = 0;
  last_sequence_ = 0;
  next_file_number_ = 0;
  has_comparator_ = false;
  has_log_number_ = false;
  has_prev_log_number_ = false;
  has_next_file_number_ = false;
  has_last_sequence_ = false;
  deleted_files_[0].clear();
  deleted_files_[1].clear();
  deleted_files_[2].clear();

  new_files_[0].clear();
  new_files_[1].clear();
  new_files_[2].clear();

  isLevelMove = false;

}

void VersionEdit::EncodeTo(std::string* dst) const {
  if (has_comparator_) {
    PutVarint32(dst, kComparator);
    PutLengthPrefixedSlice(dst, comparator_);
  }
  if (has_log_number_) {
    PutVarint32(dst, kLogNumber);
    PutVarint64(dst, log_number_);
  }
  if (has_prev_log_number_) {
    PutVarint32(dst, kPrevLogNumber);
    PutVarint64(dst, prev_log_number_);
  }
  if (has_next_file_number_) {
    PutVarint32(dst, kNextFileNumber);
    PutVarint64(dst, next_file_number_);
  }
  if (has_last_sequence_) {
    PutVarint32(dst, kLastSequence);
    PutVarint64(dst, last_sequence_);
  }


  //for deletion insertion and compaction buffer
  for(int t=0;t<4;t++){

	  for (DeletedFileSet::const_iterator iter = deleted_files_[t].begin();
	         iter != deleted_files_[t].end();
	         ++iter) {
	      PutVarint32(dst, kDeletedFile);
	      PutVarint32(dst, t);
	      PutVarint32(dst, iter->first);   // level
	      PutVarint64(dst, iter->second);  // file number
	  }
	  //printf("encode type: %d\n",t);

	  for (int i = 0; i <new_files_[t].size() ; i++) {

	      const FileMetaData& f = new_files_[t][i].second;
	      PutVarint32(dst, kNewFile);
	      PutVarint32(dst, t);
	      PutVarint32(dst, new_files_[t][i].first);  // level
	      PutVarint64(dst, f.number);
	      PutVarint64(dst, f.file_size);
	      PutLengthPrefixedSlice(dst, f.smallest.Encode());
	      PutLengthPrefixedSlice(dst, f.largest.Encode());
	      //printf("%d ",(int)f.number);
	    }
	  //printf("\n");

  }

  PutVarint32(dst,kWriteCursor);
  PutVarint64(dst,runtime::write_cursor);

  for(int i=0;i<config::kNumLevels;i++){
	  PutVarint32(dst,kReadCursor);
	  PutVarint64(dst,i);
	  PutVarint64(dst,runtime::read_cursor_[i]);
  }



}

static bool GetInternalKey(Slice* input, InternalKey* dst) {
  Slice str;
  if (GetLengthPrefixedSlice(input, &str)) {
    dst->DecodeFrom(str);
    return true;
  } else {
    return false;
  }
}

static bool GetLevel(Slice* input, int* level) {
  uint32_t v;
  if (GetVarint32(input, &v) &&
      v < config::kNumLevels) {
    *level = v;
    return true;
  } else {
    return false;
  }
}
//TODO teng, for compaction buffer, when overlap find, create a new sorted table
Status VersionEdit::DecodeFrom(const Slice& src) {
  Clear();
  Slice input = src;
  const char* msg = NULL;
  uint32_t tag;

  // Temporary storage for parsing
  uint type;
  int level;
  uint32_t number32;
  uint64_t number;
  FileMetaData f;
  f.visible = true;
  Slice str;
  InternalKey key;

  while (msg == NULL && GetVarint32(&input, &tag)) {
    switch (tag) {
      case kComparator:
        if (GetLengthPrefixedSlice(&input, &str)) {
          comparator_ = str.ToString();
          has_comparator_ = true;
        } else {
          msg = "comparator name";
        }
        break;

      case kLogNumber:
        if (GetVarint64(&input, &log_number_)) {
          has_log_number_ = true;
        } else {
          msg = "log number";
        }
        break;

      case kPrevLogNumber:
        if (GetVarint64(&input, &prev_log_number_)) {
          has_prev_log_number_ = true;
        } else {
          msg = "previous log number";
        }
        break;

      case kNextFileNumber:
        if (GetVarint64(&input, &next_file_number_)) {
          has_next_file_number_ = true;
        } else {
          msg = "next file number";
        }
        break;

      case kLastSequence:
        if (GetVarint64(&input, &last_sequence_)) {
          has_last_sequence_ = true;
        } else {
          msg = "last sequence number";
        }
        break;
      case kDeletedFile:
        if (
        	GetVarint32(&input,&type)&&
        	GetLevel(&input, &level) &&
            GetVarint64(&input, &number)) {
          deleted_files_[type].insert(std::make_pair(level, number));
        } else {
          msg = "deleted file";
        }
        break;

      case kNewFile:
        if (
        	GetVarint32(&input,&type)&&
        	GetLevel(&input, &level) &&
            GetVarint64(&input, &f.number) &&
            GetVarint64(&input, &f.file_size) &&
            GetInternalKey(&input, &f.smallest) &&
            GetInternalKey(&input, &f.largest)) {
          new_files_[type].push_back(std::make_pair(level, f));
          //printf("decode|level: %d type: %d file: %d\n",level,type,(int)f.number);
        } else {
          msg = "new-file entry";
        }
        break;
      case kWriteCursor:
          	  if (GetVarint64(&input, &runtime::write_cursor)) {
      		  } else {
      			msg = "write cursor";
      		  }
      		  break;
      case kReadCursor:
		  if (GetVarint64(&input, &number)&&
			  GetVarint64(&input, &runtime::read_cursor_[number])
		  ) {
		  } else {
			msg = "read cursor";
		  }
		  break;
      default:
        msg = "unknown tag";
        break;
    }
  }

  if (msg == NULL && !input.empty()) {
    msg = "invalid tag";
  }

  Status result;
  if (msg != NULL) {
    result = Status::Corruption("VersionEdit", msg);
  }

  runtime::setReadCursor();
  return result;
}


SortedTable *VersionEdit::CloneSortedTable(SortedTable *original){

	if(original==NULL){
		return NULL;
	}

	SortedTable *cloned = new SortedTable();
	for(int i=0;i<original->files_.size();i++){
		cloned->AddFile(original->files_[i]);
	}

	return cloned;
}

}  // namespace leveldb
