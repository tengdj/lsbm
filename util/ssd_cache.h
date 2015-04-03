// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "leveldb/cache.h"
#include "table/format.h"
#include "leveldb/status.h"

#include <iostream>
#include <vector>

#define BLOCKSIZE 5120

namespace leveldb {

class SSDCache {
 public:
  static SSDCache* current_;

  static void Deleter(const Slice& key, void* v) {
    int *cache_slot = (int *)v;
    current_->freelist_.push_back(*cache_slot);
    delete cache_slot;
  }

  SSDCache(std::string dev, size_t blocksize, size_t capacity);
  ~SSDCache();


  //read a SSTable block from SSD cache
  Status ReadBlock(const int cache_slot, int block_size, BlockContents* result);

  Cache* GetCache() { return cache_; }
  Cache::Handle* Lookup(const Slice& key) { return cache_->Lookup(key); }
  Cache::Handle* Insert(const Slice& key, const BlockContents* content);
  int* Value(const Cache::Handle* ssd_cache_handle);
  void Erase(const Slice& key);
  int FreeListSize(){
	  return freelist_.size();
  }
  void Recover();

 private:
  std::string devname_;
  int fd_ssd_cache_;
  size_t blocksize_;
  size_t capacity_;
  static const size_t delta_capacity_ = 10;
  Cache* cache_;
  port::Mutex mu_;
  std::vector<int> freelist_;  //list of unused cache blocks
  //write a SSTable block to SSD cache
   Status WriteBlock(const int cache_slot, const Slice &key, const BlockContents* result);
};

}  // namespace leveldb
