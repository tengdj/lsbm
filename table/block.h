// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_TABLE_BLOCK_H_
#define STORAGE_LEVELDB_TABLE_BLOCK_H_

#include <stddef.h>
#include <stdint.h>
#include "leveldb/iterator.h"
#include "leveldb/table.h"

namespace leveldb {

struct BlockContents;
class Comparator;

class Block {
 public:
  // Initialize the block with the specified contents.
  //explicit
  Block(const BlockContents& contents);
  Block(const BlockContents& contents, Table *table);
  Block(const BlockContents& contents, uint64_t file_number);
  Block(const char *data, uint64_t size,bool owned);

  ~Block();

  size_t size() const { return size_; }
  Iterator* NewIterator(const Comparator* comparator);

  void cached(){
	  table_->IncCachedNum();
	 //runtime::cached_num[file_number]++;
  }
//
  void evicted(){
	  table_->DecCachedNum();
	 //runtime::cached_num[file_number]--;
  }
 private:
  friend class Table;
  uint32_t NumRestarts() const;

  const char* data_;
  size_t size_;
  uint32_t restart_offset_;     // Offset in data_ of restart array
  bool owned_;                  // Block owns data_[]
  Table *table_;				// the table which owns this block
  uint64_t file_number = 0;
  // No copying allowed
  Block(const Block&);
  void operator=(const Block&);

  class Iter;
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_TABLE_BLOCK_H_
