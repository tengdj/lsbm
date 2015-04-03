// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

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
#include "util/ssd_cache.h"

namespace leveldb {

struct Table::Rep {
  ~Rep() {
    delete filter;
    delete [] filter_data;
    delete index_block;
  }

  Options options;
  Status status;
  RandomAccessFile* file;
  int filenumber;
  uint64_t cache_id;
  FilterBlockReader* filter;
  const char* filter_data;

  BlockHandle metaindex_handle;  // Handle to metaindex_block: saved from footer
  Block* index_block;
};

Status Table::Open(const Options& options,
		           int filenumber,
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
static int tcounter = 0;

Iterator* Table::BlockReader(void* arg,
                             const ReadOptions& options,
                             const Slice& index_value) {
  //teng: for test
  bool ssd_cheat = true;
  bool ssd_cache_on = true;
  bool cache_on = true;
  Table* table = reinterpret_cast<Table*>(arg);
  Cache* block_cache = table->rep_->options.block_cache;
  Block* block = NULL;
  Cache::Handle* cache_handle = NULL;
  SSDCache* ssd_block_cache = table->rep_->options.ssd_block_cache;
  Cache::Handle* ssd_cache_handle = NULL;

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
    if (cache_on && block_cache != NULL) {
      cache_handle = block_cache->Lookup(key);
      if (cache_handle != NULL) {
        block = reinterpret_cast<Block*>(block_cache->Value(cache_handle));
      }
    }
    //check ssd_block_cache (in ssd cache)
    if (ssd_cache_on && options.fill_cache && block == NULL && ssd_block_cache != NULL) {
      ssd_cache_handle = ssd_block_cache->Lookup(key);
      if (ssd_cache_handle != NULL) {
    	int *cache_slot = ssd_block_cache->Value(ssd_cache_handle);
        s = ssd_block_cache->ReadBlock(*cache_slot, handle.size(),  &contents);
        if (s.ok())
        {
        	block = new Block(contents);
        }
      }
    }

    if(block != NULL&&block->size()==0){
       delete block;
       block = NULL;
    }
    //read from disk
    if ( block == NULL) {

       if(ssd_cache_handle!=NULL){
    	  ssd_block_cache->GetCache()->Release(ssd_cache_handle);
    	  ssd_block_cache->GetCache()->Erase(key);
       }
       if(cache_handle!=NULL){
    	  block_cache->Release(cache_handle);
    	  block_cache->Erase(key);
       }

  	  //printf("get here 2\n");
      s = ReadBlock(table->rep_->file, options, handle, &contents);
      //printf("leave here 2\n");

      if (s.ok()) {
        block = new Block(contents);

      }
    }

    //insert block into cache if necessary
    if (block != NULL && options.fill_cache) {//contents.cachable &&
      //printf("get here\n");
      if (cache_on && block_cache != NULL) {
      //always insert into block_cache if missed
        if (cache_handle == NULL) {
          cache_handle = block_cache->Insert(key, block, block->size(), &DeleteCachedBlock);
   	    }
      }
      if (ssd_cache_on && ssd_block_cache != NULL) {
      //insert into ssd_block_cache if missed in block_cache and ssd_block_cache
        if (ssd_cache_handle == NULL) {
          ssd_cache_handle = ssd_block_cache->Insert(key, &contents);
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
    if (ssd_cache_handle != NULL){
      iter->RegisterCleanup(&ReleaseBlock, ssd_block_cache->GetCache(), ssd_cache_handle);
    }

  } else {
    iter = NewErrorIterator(s);
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
    } else {
      Iterator* block_iter = BlockReader(this, options, iiter->value());
      //printf("get here 5\n");
      block_iter->Seek(k);
      //printf("leave here 5\n");
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
//evict all blocks from the ssd cache of this table related file
Status Table::EvictSSDCache(){
	Status s;
	int i=0;

	Iterator* iiter = rep_->index_block->NewIterator(rep_->options.comparator);
	SSDCache *ssdcache = rep_->options.ssd_block_cache;
	printf("evict file %d, before: %d  ",rep_->filenumber,ssdcache->FreeListSize());
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
	        ssdcache->Erase(key);
	    }
	    iiter->Next();
	}
	if (s.ok()) {
	    s = iiter->status();
	}
    delete iiter;
    printf("after: %d  \n",ssdcache->FreeListSize());
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
