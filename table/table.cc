// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <sys/time.h>
#include "leveldb/table.h"

#include "leveldb/cache.h"
#include "leveldb/comparator.h"
#include "leveldb/env.h"
#include "leveldb/filter_policy.h"
#include "leveldb/options.h"
#include "table/block.h"
#include "table/filter_block.h"
#include "table/format.h"
#include "table/two_level_iterator.h"
#include "util/coding.h"
#include "db/dlsm_param.h"
#include "util/cache_stat.h"

namespace leveldb {

struct Table::Rep {
  ~Rep() {
    delete filter;
    delete [] filter_data;
    //printf("rep: %ld\n",this->filenumber);
    delete index_block;
  }

  Options options;
  Status status;
  RandomAccessFile* file;
  uint64_t filenumber;
  uint64_t cache_id;
  FilterBlockReader* filter;
  const char* filter_data;


  BlockHandle metaindex_handle;  // Handle to metaindex_block: saved from footer
  Block* index_block;
};
Status Table::Open(const Options& options,
		           uint64_t filenumber,
                   RandomAccessFile* file,
                   uint64_t size,
                   Table** table) {
  *table = NULL;
  if (size < Footer::kEncodedLength) {
    return Status::InvalidArgument("file is too short to be an sstable");
  }

  char footer_space[Footer::kEncodedLength];
  Slice footer_input;
  Status s = file->Read(size - Footer::kEncodedLength, Footer::kEncodedLength,
                        &footer_input, footer_space);

  if (!s.ok()) return s;

  Footer footer;
  s = footer.DecodeFrom(&footer_input);
  if (!s.ok()) return s;

  // Read the index block
  BlockContents contents;
  Block* index_block = NULL;
  if (s.ok()) {
    s = ReadBlock(file, ReadOptions(), footer.index_handle(), &contents);
    if (s.ok()) {
      index_block = new Block(contents);
    }
  }

  if (s.ok()) {
    // We've successfully read the footer and the index block: we're
    // ready to serve requests.
    Rep* rep = new Table::Rep;
    rep->options = options;
    rep->file = file;
    rep->metaindex_handle = footer.metaindex_handle();
    rep->index_block = index_block;
    rep->cache_id = (options.block_cache ? options.block_cache->NewId() : 0);
    rep->filter_data = NULL;
    rep->filter = NULL;
    rep->filenumber = filenumber;
    *table = new Table(rep);
    (*table)->ReadMeta(footer);
  } else {
    if (index_block) delete index_block;
  }

  return s;
}

void Table::ReadMeta(const Footer& footer) {
  if (rep_->options.filter_policy == NULL) {
    return;  // Do not need any metadata
  }

  // TODO(sanjay): Skip this if footer.metaindex_handle() size indicates
  // it is an empty block.
  ReadOptions opt;
  BlockContents contents;
  if (!ReadBlock(rep_->file, opt, footer.metaindex_handle(), &contents).ok()) {
    // Do not propagate errors since meta info is not needed for operation
    return;
  }

  Block* meta = new Block(contents);

  Iterator* iter = meta->NewIterator(BytewiseComparator());
  std::string key = "filter.";
  key.append(rep_->options.filter_policy->Name());
  iter->Seek(key);
  if (iter->Valid() && iter->key() == Slice(key)) {
    ReadFilter(iter->value());
  }
  delete iter;
  delete meta;
}

void Table::ReadFilter(const Slice& filter_handle_value) {
  Slice v = filter_handle_value;
  BlockHandle filter_handle;
  if (!filter_handle.DecodeFrom(&v).ok()) {
    return;
  }

  // We might want to unify with ReadBlock() if we start
  // requiring checksum verification in Table::Open.
  ReadOptions opt;
  BlockContents block;
  if (!ReadBlock(rep_->file, opt, filter_handle, &block).ok()) {
    return;
  }
  if (block.heap_allocated) {
    rep_->filter_data = block.data.data();     // Will need to delete later
  }
  rep_->filter = new FilterBlockReader(rep_->options.filter_policy, block.data);
}

Table::~Table() {
//  if(this->num_blocks_cached!=0){
//	  exit(0);
//  }
  //printf("%ld\n",this->num_blocks_cached);

  Status s = this->EvictBlockCache();
  assert(s.ok());
  delete rep_;
}


static void DeleteBlock(void* arg, void* ignored) {
  delete reinterpret_cast<Block*>(arg);
}

static void DeleteCachedBlock(const Slice& key, void* value) {
  Block* block = reinterpret_cast<Block*>(value);
  delete block;
}

static void ReleaseBlock(void* arg, void* h) {
  Cache* cache = reinterpret_cast<Cache*>(arg);
  Cache::Handle* handle = reinterpret_cast<Cache::Handle*>(h);
  cache->Release(handle);
}


// Convert an index iterator value (i.e., an encoded BlockHandle)
// into an iterator over the contents of the corresponding block.
Iterator* Table::BlockReader(void* arg,
                             const ReadOptions& options,
                             const Slice& index_value) {


  int block_cache_served = 0;
  int hdd_served = 0;

  Table* table = reinterpret_cast<Table*>(arg);
  Cache* block_cache = table->rep_->options.block_cache;
  Block* block = NULL;
  Cache::Handle* cache_handle = NULL;

  BlockHandle handle;
  Slice input = index_value;
  Status s = handle.DecodeFrom(&input);
  // We intentionally allow extra stuff in index_value so that we
  // can add more features in the future.

  if (s.ok()) {

    BlockContents contents;
    //build key for cache lookup
    char cache_key_buffer[16];
    //EncodeFixed64(cache_key_buffer, table->rep_->cache_id);
    EncodeFixed64(cache_key_buffer, table->rep_->filenumber);
    EncodeFixed64(cache_key_buffer+8, handle.offset());
    Slice key(cache_key_buffer, sizeof(cache_key_buffer));
    //check block_cache (in memory)
    if (block_cache != NULL) {
      if(options.fill_cache){
    	  cache_handle = block_cache->Lookup(key);
      }else{
    	  cache_handle = block_cache->LiteLookup(key);
      }
      if (cache_handle != NULL) {
        block = reinterpret_cast<Block*>(block_cache->Value(cache_handle));
        if(!runtime::isWarmingUp())
        {
        	block_cache_served++;
        }
      }
    }


    if(block != NULL&&block->size()==0){

       delete block;
       block = NULL;

       if(cache_handle!=NULL){//error block got from memory

          block_cache->Release(cache_handle);
          block_cache->Erase(key);
          cache_handle = NULL;
          if(!runtime::isWarmingUp())
          {
        	  block_cache_served--;
          }
      }
    }
    //read from disk
    if ( block == NULL) {
      s = ReadBlock(table->rep_->file, options, handle, &contents);
      if (s.ok()) {
        block = new Block(contents);
        if(!runtime::isWarmingUp())
        {
        	hdd_served++;
        }
      }
    }

    //insert block into cache if necessary
    if (block != NULL && options.fill_cache) {//contents.cachable &&

      if (block_cache != NULL) {
      //always insert into block_cache if missed
        if (cache_handle == NULL) {
          //when one block is inserted into the block cache, increase the counter of this file
          cache_handle = block_cache->Insert(key, block, block->size(), &DeleteCachedBlock);
   	    }
      }
    }
  }

  Iterator* iter;
  if (block != NULL) {

    iter = block->NewIterator(table->rep_->options.comparator);

    if (cache_handle == NULL) {
      iter->RegisterCleanup(&DeleteBlock, block, NULL);
    } else {
      iter->RegisterCleanup(&ReleaseBlock, block_cache, cache_handle);
    }

  } else {
    iter = NewErrorIterator(s);
  }

  double blockcache_used = 0;
  if(block_cache){
	  blockcache_used = (double)block_cache->Used()/block_cache->getCapacity();
  }

  if(!runtime::isWarmingUp()&&options.fill_cache){
	  leveldb::updateCache_stat(0,block_cache_served,hdd_served,0,blockcache_used);
  }

  if(options.fill_cache){
	  table->IncVisitedNum();
	  runtime::num_reads_++;
  }

  return iter;
}

Iterator* Table::NewIterator(const ReadOptions& options) const {
  return NewTwoLevelIterator(
      rep_->index_block->NewIterator(rep_->options.comparator),
      &Table::BlockReader, const_cast<Table*>(this), options);
}

Status Table::InternalGet(const ReadOptions& options, const Slice& k,
                          void* arg,
                          void (*saver)(void*, const Slice&, const Slice&)) {
  Status s;
  Iterator* iiter = rep_->index_block->NewIterator(rep_->options.comparator);
  iiter->Seek(k);
  if (iiter->Valid()) {
    Slice handle_value = iiter->value();
    FilterBlockReader* filter = rep_->filter;
    BlockHandle handle;
    if (filter != NULL &&
        handle.DecodeFrom(&handle_value).ok() &&
        !filter->KeyMayMatch(handle.offset(), k)) {
      // Not found
    } else
    {
      Iterator* block_iter = BlockReader(this, options, iiter->value());
      block_iter->Seek(k);
      if (block_iter->Valid()) {
        (*saver)(arg, block_iter->key(), block_iter->value());
      }
      s = block_iter->status();
      delete block_iter;
    }
  }
  if (s.ok()) {
    s = iiter->status();
  }

  delete iiter;
  return s;
}
//evict all blocks from the mem cache of this table related file
Status Table::EvictBlockCache(){
	Status s;

	Iterator* iiter = rep_->index_block->NewIterator(rep_->options.comparator);
	Cache *block_cache = rep_->options.block_cache;
	if(!block_cache){
		return Status::OK();
	}
	uint64_t before, after;
	before = block_cache->Used();

	iiter->SeekToFirst();

	while (iiter->Valid()) {
	    Slice handle_value = iiter->value();
	    BlockHandle handle;
	    Status s = handle.DecodeFrom(&handle_value);
	    if (s.ok()) {
	        char cache_key_buffer[16];
	        EncodeFixed64(cache_key_buffer, rep_->filenumber);
	        EncodeFixed64(cache_key_buffer+8, handle.offset());
	        Slice key(cache_key_buffer, sizeof(cache_key_buffer));
	        block_cache->Erase(key);
	    }
	    iiter->Next();
	}
	if (s.ok()) {
	    s = iiter->status();
	}
    delete iiter;
    after = block_cache->Used();

    if(false&&before>after){
      printf("evicted file: %ld, before %ld and after: %ld\n",rep_->filenumber,before/rep_->options.block_size,after/rep_->options.block_size);
    }
	return s;
}

//TODO
/*
 * go through the whole block cache to check if all the blocks in this file in the cache or not,
 * for the blocks in the cache, remember its min and max key
 * */
Status Table::GetKeyRangeCached(std::vector<Slice *> *result){

	Status s;
	Iterator* iiter = rep_->index_block->NewIterator(rep_->options.comparator);
	Cache *block_cache = rep_->options.block_cache;
	if(!block_cache){
		return Status::OK();
	}

	iiter->SeekToFirst();

	while (iiter->Valid()) {
	    Slice handle_value = iiter->value();
	    BlockHandle handle;
	    Status s = handle.DecodeFrom(&handle_value);
	    if (s.ok()) {
	        char cache_key_buffer[16];
	        EncodeFixed64(cache_key_buffer, rep_->filenumber);
	        EncodeFixed64(cache_key_buffer+8, handle.offset());
	        Slice key(cache_key_buffer, sizeof(cache_key_buffer));
	        Cache::Handle *handle = block_cache->LiteLookup(key);
	        if(handle){
	        	Block *block = reinterpret_cast<Block*>(block_cache->Value(handle));
        		Slice *minAndMax = new Slice[2];
	        	Iterator *ittr = block->NewIterator(rep_->options.comparator);
        		ittr->SeekToFirst();
        		char *ch1 = new char[ittr->key().size()];
        		memcpy(ch1,ittr->key().data(),ittr->key().size());
        		Slice s1(ch1,ittr->key().size());
        		ittr->SeekToLast();
        		minAndMax[0] = s1;
        		char *ch2 = new char[ittr->key().size()];
        		memcpy(ch2,ittr->key().data(),ittr->key().size());
        		Slice s2(ch2,ittr->key().size());
        		minAndMax[1] = s2;
		        //printf("%s %s\n",minAndMax[0].ToString().c_str(),minAndMax[1].ToString().c_str());
		        result->push_back(minAndMax);
	        	delete ittr;
		        block_cache->Release(handle);
	        }
	    }
	    iiter->Next();
	}
	if (s.ok()) {
	    s = iiter->status();
	}
    delete iiter;

	return s;
}

uint64_t Table::ApproximateOffsetOf(const Slice& key) const {
  Iterator* index_iter =
      rep_->index_block->NewIterator(rep_->options.comparator);
  index_iter->Seek(key);
  uint64_t result;
  if (index_iter->Valid()) {
    BlockHandle handle;
    Slice input = index_iter->value();
    Status s = handle.DecodeFrom(&input);
    if (s.ok()) {
      result = handle.offset();
    } else {
      // Strange: we can't decode the block handle in the index block.
      // We'll just return the offset of the metaindex block, which is
      // close to the whole file size for this case.
      result = rep_->metaindex_handle.offset();
    }
  } else {
    // key is past the last key in the file.  Approximate the offset
    // by returning the offset of the metaindex block (which is
    // right near the end of the file).
    result = rep_->metaindex_handle.offset();
  }
  delete index_iter;
  return result;
}

}  // namespace leveldb
