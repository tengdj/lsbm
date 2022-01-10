// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "table_cache.h"

#include "filename.h"
#include "lsbm/env.h"
#include "lsbm/table.h"
#include "util/coding.h"
#include "lsbm/params.h"
#include <iostream>
namespace leveldb {

struct TableAndFile {
  RandomAccessFile* file;
  Table* table;
};

static void DeleteEntry(const Slice& key, void* value) {
  TableAndFile* tf = reinterpret_cast<TableAndFile*>(value);
  delete tf->table;
  delete tf->file;
  delete tf;
}

static void UnrefEntry(void* arg1, void* arg2) {
  Cache* cache = reinterpret_cast<Cache*>(arg1);
  Cache::Handle* h = reinterpret_cast<Cache::Handle*>(arg2);
  cache->Release(h);
}

TableCache::TableCache(const std::string& dbname,
                       const Options* options,
                       int entries)
    : env_(options->env),
      dbname_(dbname),
      options_(options),
      cache_(NewLRUCache(entries)) {
}

TableCache::~TableCache() {
  delete cache_;
}



Iterator* TableCache::NewIterator(const ReadOptions& options,
                                  uint64_t file_number,
                                  uint64_t file_size,
                                  Table** tableptr) {
  if (tableptr != NULL) {
    *tableptr = NULL;
  }

  Cache::Handle* handle = NULL;
  Status s = this->FindTable(file_number, file_size, &handle);
  if (!s.ok()) {
    return NewErrorIterator(s);
  }

  Table* table = reinterpret_cast<TableAndFile*>(cache_->Value(handle))->table;
  Iterator* result = table->NewIterator(options);
  result->RegisterCleanup(&UnrefEntry, cache_, handle);
  if (tableptr != NULL) {
    *tableptr = table;
  }
  return result;
}

Status TableCache::Get(const ReadOptions& options,
                       uint64_t file_number,
                       uint64_t file_size,
                       const Slice& k,
                       void* arg,
                       void (*saver)(void*, const Slice&, const Slice&)) {
  Cache::Handle* handle = NULL;
  Status s = this->FindTable(file_number, file_size, &handle);
  if (s.ok()) {
    Table* t = reinterpret_cast<TableAndFile*>(cache_->Value(handle))->table;
    s = t->InternalGet(options, k, arg, saver);
    cache_->Release(handle);
  }
  return s;
}

int TableCache::GetRange(const ReadOptions& options,
		const Comparator* ucmp,
        uint64_t file_number,
        uint64_t file_size,
        const Slice& start,
        const Slice& end){
	  Cache::Handle* handle = NULL;
	  Status s = this->FindTable(file_number, file_size, &handle);
	  int found = 0;
	  if (s.ok()) {
	    Table* t = reinterpret_cast<TableAndFile*>(cache_->Value(handle))->table;

	    found = t->InternalGetRange(options, ucmp,start,end);
	    cache_->Release(handle);
	  }
	  return found;

}

int TableCache::getCacheNum(uint64_t file_number, uint64_t file_size){
	int cached = 0;
	Cache::Handle *handle = NULL;
	Status s = this->FindTable(file_number,file_size,&handle);
	if (s.ok()) {
		Table* t = reinterpret_cast<TableAndFile*>(cache_->Value(handle))->table;
		cached = t->getCachedNum();
		cache_->Release(handle);
	}

	return cached;
}


bool TableCache::isTableHot(uint64_t file_number, uint64_t file_size){
	bool ishot = false;
	Cache::Handle *handle = NULL;
	Status s = this->FindTable(file_number,file_size,&handle);
	if (s.ok()) {
		Table* t = reinterpret_cast<TableAndFile*>(cache_->Value(handle))->table;
		ishot = t->isHot();
		cache_->Release(handle);
	}
	return ishot;
}

Status TableCache::GetKeyRangeCached(uint64_t file_number, uint64_t file_size,std::vector<Slice *> *result) {
  Cache::Handle* handle = NULL;
  Status s = this->FindTable(file_number, file_size, &handle);
  if (s.ok()) {
    Table* t = reinterpret_cast<TableAndFile*>(cache_->Value(handle))->table;
    s = t->GetKeyRangeCached(result);
    cache_->Release(handle);
  }
  return s;
}

Status TableCache::LoadTable(uint64_t file_number, uint64_t file_size){
	Cache::Handle *handle = NULL;
	Status s = this->FindTable(file_number,file_size,&handle);
	if (s.ok()) {
		cache_->Release(handle);
	}
	return s;
}


bool TableCache::ContainKey(const ReadOptions& options,
		  	uint64_t file_number,
	  	  	uint64_t file_size,
	  	  	const Slice& k){
	Cache::Handle* handle = NULL;
	Status s = this->FindTable(file_number, file_size, &handle);
	bool contain = false;
	if (s.ok()) {
		Table* t = reinterpret_cast<TableAndFile*>(cache_->Value(handle))->table;
		contain = t->ContainKey(options,k);
		cache_->Release(handle);
	}
	return contain;
}

Status TableCache::SkipFilterGet(const ReadOptions& options,
		  	  	uint64_t file_number,
		  	  	uint64_t file_size,
		  	  	const Slice& k,
				void* arg,
				void (*saver)(void*, const Slice&, const Slice&)){
  Cache::Handle* handle = NULL;
  Status s = this->FindTable(file_number, file_size, &handle);
  if (s.ok()) {
	Table* t = reinterpret_cast<TableAndFile*>(cache_->Value(handle))->table;
	s = t->SkipFilterGet(options, k, arg, saver);
	cache_->Release(handle);
  }
  return s;
}

void TableCache::Evict(uint64_t file_number, uint64_t file_size, bool evict_block_cache) {
  char buf[sizeof(file_number)];
  EncodeFixed64(buf, file_number);

  Cache::Handle* handle = NULL;
  Status s = this->FindTable(file_number, file_size, &handle);

  if(s.ok() && handle!=NULL){
	if(options_->block_cache!=NULL&&evict_block_cache){
		Table* t = reinterpret_cast<TableAndFile*>(cache_->Value(handle))->table;
		//double before = options_->block_cache->Percent();
		s = t->EvictBlockCache();
		//double after = options_->block_cache->Percent();
		//std::cout<<before<<"	"<<after<<" "<<(before-after)<<std::endl;
	}
	//runtime::cached_num.erase(file_number);

	cache_->Release(handle);
	cache_->Erase(Slice(buf, sizeof(buf)));
  }


}


Status TableCache::FindTable(uint64_t file_number, uint64_t file_size,
                             Cache::Handle** handle) {
  Status s;
  char buf[sizeof(file_number)];
  EncodeFixed64(buf, file_number);
  Slice key(buf, sizeof(buf));
  *handle = cache_->Lookup(key);
  if (*handle == NULL) {
    std::string fname = TableFileName(dbname_, file_number);
    RandomAccessFile* file = NULL;
    Table* table = NULL;
    s = env_->NewRandomAccessFile(fname, &file);
    if (!s.ok()) {
      std::string old_fname = SSTTableFileName(dbname_, file_number);
      if (env_->NewRandomAccessFile(old_fname, &file).ok()) {
        s = Status::OK();
      }
    }

    if (s.ok()) {
      s = Table::Open(*options_,file_number, file, file_size, &table);
    }

    if (!s.ok()) {
      assert(table == NULL);
      delete file;
      // We do not cache error results so that if the error is transient,
      // or somebody repairs the file, we recover automatically.
    } else {
      TableAndFile* tf = new TableAndFile;
      tf->file = file;
      tf->file = file;
      tf->table = table;
      *handle = cache_->Insert(key, tf, 1, &DeleteEntry);

//      if(runtime::cached_num.find(file_number)==runtime::cached_num.end()){
//    	  runtime::cached_num[file_number] = 0;
//      }
    }
  }
  return s;
}



}  // namespace leveldb
