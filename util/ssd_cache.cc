// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>

#include <iostream>
#include <vector>
#include "util/coding.h"
#include "leveldb/cache.h"
#include "table/format.h"
#include "util/ssd_cache.h"
#include "util/mutexlock.h"

namespace leveldb {

class ShardedLRUCache;

SSDCache* SSDCache::current_;

SSDCache::SSDCache(std::string dev, size_t blocksize, size_t capacity) :
                  devname_(dev), blocksize_(blocksize), capacity_(capacity),
		  cache_(NewLRUCache(capacity)), freelist_(std::vector<int>(capacity+delta_capacity_))
{
    current_ = this;

    //open SSD as a raw device
    fd_ssd_cache_ = open(devname_.c_str(), O_RDWR|O_DIRECT);
    if (fd_ssd_cache_ < 0) {
    	std::string errstr = devname_ + ": open() error";
        perror(errstr.c_str());
        exit(-1);
    }
    std::cerr << "device " << devname_ << " opened successfully." << std::endl;

    //check device size
    size_t dev_size_in_byte;
    if ((dev_size_in_byte = lseek(fd_ssd_cache_, 0, SEEK_END)) < 0) {
    	std::string errstr = devname_ + ": lseek() error";
        perror(errstr.c_str());
        exit(-1);
    }
    if (dev_size_in_byte < blocksize_ * capacity_) {
    	std::cerr << "device has no enough space: require " << blocksize_ * capacity_ << " bytes, has " << dev_size_in_byte << std::endl;
    	exit(-1);
    }

    //set freelist_
    //TODO: load cache status
    for (int i=0; i<freelist_.size(); i++) {
    	freelist_[i] = i;
    }
}

SSDCache::~SSDCache() {
    delete cache_;

    //TODO: write cache status to log

    //close SSD device
    int ret = close(fd_ssd_cache_);
    if (ret < 0) {
    	std::string errstr = devname_ + ": close() error";
        perror(errstr.c_str());
        exit(-1);
    }
    std::cerr << "device " << devname_ << " closed successfully.\n" << std::endl;
}

//write a SSTable block to SSD cache
//TODO: we have not implemented crc verification and compression/decompression.
Status SSDCache::WriteBlock(const int cache_slot, const Slice &key,const BlockContents* result) {
    //get position to seek

    off_t pos = cache_slot*BLOCKSIZE;
    off_t offset = lseek(fd_ssd_cache_, pos, SEEK_SET);
    assert(offset >= 0);
    int n = result->data.size();
    //write block content into cache
    char *buf = new char[n];
    assert(n < BLOCKSIZE);
    memcpy(buf, result->data.data(), result->data.size());

    ssize_t bytes_written = write(fd_ssd_cache_, buf, BLOCKSIZE);
    assert(bytes_written >= 0);
    delete buf;
    return Status::OK();
}

//read a SSTable block from SSD cache
//TODO: we have not implemented crc verification and compression/decompression.
Status SSDCache::ReadBlock(const int cache_slot, int block_size, BlockContents* result) {
    //initialize result
    result->data = Slice();
    result->cachable = false;
    result->heap_allocated = false;

	off_t pos = (cache_slot)*BLOCKSIZE;
    off_t offset = lseek(fd_ssd_cache_, pos, SEEK_SET);
    assert(offset >= 0);

    //read data from SSD cache
    size_t n = static_cast<size_t>(block_size);
	char *buf = new char[n];
	ssize_t bytes_read = read(fd_ssd_cache_, buf, n);
	assert(bytes_read >= 0);
	assert(n <= BLOCKSIZE);

	result->data = Slice((const char *)buf, n);
    result->cachable = true;
    result->heap_allocated = true;
    return Status::OK();
}

int* SSDCache::Value(const Cache::Handle* ssd_cache_handle){
	return reinterpret_cast<int*>(cache_->Value(const_cast<Cache::Handle*>(ssd_cache_handle)));
}

Cache::Handle* SSDCache::Insert(const Slice& key, const BlockContents* content)
{
  MutexLock l(&mu_);
  assert(freelist_.size() >= 1);
  int* cache_slot = new int(freelist_.back());
  freelist_.pop_back();
  Cache::Handle* ssd_cache_handle = cache_->Insert(key, cache_slot, 1, &SSDCache::Deleter);
  Status s = WriteBlock(*cache_slot, key, content);
  assert(s.ok());
  return ssd_cache_handle;
}

void SSDCache::Erase(const Slice& key) {
    cache_->Erase(key);
}

void SSDCache::Recover(){

}

}  // namespace leveldb
