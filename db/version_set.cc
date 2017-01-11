// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "db/version_set.h"
#include <iostream>
#include <algorithm>
#include <stdio.h>
#include "db/filename.h"
#include "db/log_reader.h"
#include "db/log_writer.h"
#include "db/memtable.h"
#include "db/table_cache.h"
#include "leveldb/env.h"
#include "leveldb/table_builder.h"
#include "table/merger.h"
#include "table/two_level_iterator.h"
#include "util/coding.h"
#include "util/logging.h"
#include "version_set.h"
#include "dlsm_param.h"

#include <cstdint>


namespace leveldb {


// Maximum bytes of overlaps in grandparent (i.e., level+2) before we
// stop building a single file in a level->level+1 compaction.
const int64_t kMaxGrandParentOverlapBytes = 10 * config::kTargetFileSize;

// Maximum number of bytes in all compacted files.  We avoid expanding
// the lower level file set of a compaction if it would make the
// total compaction cover more than this many bytes.
const int64_t kExpandedCompactionByteSizeLimit = 25 * config::kTargetFileSize;
//TODO precision problem
uint64_t MaxBytesForLevel(int level) {
  assert(level>=0);
  // Note: the result for level zero is not really used since we set
  // the level-0 compaction threshold based on number of files.
  uint64_t result = config::kL0_size * 1048576.0;  // Result for both level-0

  while (level > 0) {
    result *= 10;//times size ratio
    level--;
  }

  return result;
}

uint64_t TotalFileSize(const std::vector<FileMetaData*>& files) {
  uint64_t sum = 0;
  for (size_t i = 0; i < files.size(); i++) {
    sum += files[i]->file_size;
  }
  return sum;
}
uint64_t MaxKBytesForLevel(int level){
	return MaxBytesForLevel(level)/digit_base;
}
uint64_t MaxMBytesForLevel(int level){
	return MaxBytesForLevel(level)/(digit_base*digit_base);
}

uint64_t TotalFileSizeKB(const std::vector<FileMetaData*>& files) {
  return TotalFileSize(files)/digit_base;
}

uint64_t TotalFileSizeMB(const std::vector<FileMetaData*>& files) {
  return TotalFileSize(files)/(digit_base*digit_base);
}


uint64_t MaxFileSizeForLevel(int level) {
  return config::kTargetFileSize;  // We could vary per level to reduce number of files?
}





namespace {
std::string IntSetToString(const std::set<uint64_t>& s) {
  std::string result = "{";
  for (std::set<uint64_t>::const_iterator it = s.begin();
       it != s.end();
       ++it) {
    result += (result.size() > 1) ? "," : "";
    result += NumberToString(*it);
  }
  result += "}";
  return result;
}
}  // namespace

Version::~Version() {

  assert(refs_ == 0);

  // Remove from linked list
  prev_->next_ = next_;
  next_->prev_ = prev_;

  for(int l = 0;l<config::kNumLevels;l++){

	  // Drop references to files of the all three part of each level
	  for(int type=0;type<3;type++){
		  SortedTable *cur = levels_[l][type];
		  SortedTable *next = NULL;
		  cur->prev->next = NULL;
		  while(cur!=NULL){
			  next = cur->next;
			  cur->prev = NULL;
			  cur->next = NULL;
			  delete cur;
			  cur = next;

		  };
	  }

  }

}

int FindFile(const InternalKeyComparator& icmp,
             const std::vector<FileMetaData*>& files,
             const Slice& key) {
  uint32_t left = 0;
  uint32_t right = files.size();
  while (left < right) {
    uint32_t mid = (left + right) / 2;
    const FileMetaData* f = files[mid];

    if (icmp.InternalKeyComparator::Compare(f->largest.Encode(), key) < 0) {
      // Key at "mid.largest" is < "target".  Therefore all
      // files at or before "mid" are uninteresting.
      left = mid + 1;
    } else {
      // Key at "mid.largest" is >= "target".  Therefore all files
      // after "mid" are uninteresting.
      right = mid;
    }
  }
  return right;
}

bool AfterFile(const Comparator* ucmp,
                      const Slice* user_key, const FileMetaData* f) {
  // NULL user_key occurs before all keys and is therefore never after *f
  return (user_key != NULL &&
          ucmp->Compare(*user_key, f->largest.user_key()) > 0);
}

bool BeforeFile(const Comparator* ucmp,
                       const Slice* user_key, const FileMetaData* f) {
  // NULL user_key occurs after all keys and is therefore never before *f
  return (user_key != NULL &&
          ucmp->Compare(*user_key, f->smallest.user_key()) < 0);
}


static Iterator* GetFileIterator(void* arg,
                                 const ReadOptions& options,
                                 const Slice& file_value) {
  TableCache* cache = reinterpret_cast<TableCache*>(arg);
  if (file_value.size() != 16) {
    return NewErrorIterator(
        Status::Corruption("FileReader invoked with unexpected value"));
  } else {
    return cache->NewIterator(options,
                              DecodeFixed64(file_value.data()),
                              DecodeFixed64(file_value.data() + 8));
  }
}

Iterator* Version::NewConcatenatingIterator(const ReadOptions& options,int type,
                                            int level) const {
	assert(type!=COMPACTION_BUFFER);
  return NewTwoLevelIterator(
      new LevelFileNumIterator(vset_->icmp_, &levels_[level][type]->files_),
      &GetFileIterator, vset_->table_cache_, options);
}


void Version::AddIterators(const ReadOptions& options,
                           std::vector<Iterator*>* iters) {
  // Merge all level zero files together since they may overlap, all files in level 0 is in the deletion part
  for (size_t i = 0; i < levels_[0][DELETION_PART]->files_.size(); i++) {
    iters->push_back(
        vset_->table_cache_->NewIterator(
            options, levels_[0][DELETION_PART]->files_[i]->number, levels_[0][DELETION_PART]->files_[i]->file_size));
  }

  // For levels > 0, we can use a concatenating iterator that sequentially
  // walks through the non-overlapping files in the level, opening them
  // lazily.
  for (int level = 1; level < config::kNumLevels; level++) {
    if (!levels_[level][DELETION_PART]->files_.empty()) {
      iters->push_back(NewConcatenatingIterator(options, DELETION_PART,level));
    }
    if (!levels_[level][INSERTION_PART]->files_.empty()) {
      iters->push_back(NewConcatenatingIterator(options, INSERTION_PART,level));
    }
  }
}

// Callback from TableCache::Get()
namespace {
enum SaverState {
  kNotFound,
  kFound,
  kDeleted,
  kCorrupt,
};
struct Saver {
  SaverState state;
  const Comparator* ucmp;
  Slice user_key;
  std::string* value;
};
}
static void SaveValue(void* arg, const Slice& ikey, const Slice& v) {
  Saver* s = reinterpret_cast<Saver*>(arg);
  ParsedInternalKey parsed_key;
  if (!ParseInternalKey(ikey, &parsed_key)) {
    s->state = kCorrupt;
  } else {
    if (s->ucmp->Compare(parsed_key.user_key, s->user_key) == 0) {
      s->state = (parsed_key.type == kTypeValue) ? kFound : kDeleted;
      if (s->state == kFound) {
        s->value->assign(v.data(), v.size());
      }
    }
  }
}

static bool NewestFirst(FileMetaData* a, FileMetaData* b) {
  return a->number > b->number;
}



/*

void Version::ForEachOverlapping(Slice user_key, Slice internal_key,
                                 void* arg,
                                 bool (*func)(void*, int, FileMetaData*)) {
  // (sanjay): Change Version::Get() to use this function.
  const Comparator* ucmp = vset_->icmp_.user_comparator();

  // Search level-0 in order from newest to oldest.
  std::vector<FileMetaData*> tmp;
  tmp.reserve(files_[0].size());
  for (uint32_t i = 0; i < files_[0].size(); i++) {
    FileMetaData* f = files_[0][i];
    if (ucmp->Compare(user_key, f->smallest.user_key()) >= 0 &&
        ucmp->Compare(user_key, f->largest.user_key()) <= 0) {
      tmp.push_back(f);
    }
  }
  if (!tmp.empty()) {
    std::sort(tmp.begin(), tmp.end(), NewestFirst);
    for (uint32_t i = 0; i < tmp.size(); i++) {
      if (!(*func)(arg, 0, tmp[i])) {
        return;
      }
    }
  }

  // Search other levels.
  for (int level = 1; level < config::kNumLevels; level++) {
    size_t num_files = files_[level].size();
    if (num_files == 0) continue;

    // Binary search to find earliest index whose largest key >= internal_key.
    uint32_t index = FindFile(vset_->icmp_, files_[level], internal_key);
    if (index < num_files) {
      FileMetaData* f = files_[level][index];
      if (ucmp->Compare(user_key, f->smallest.user_key()) < 0) {
        // All of "f" is past any data for user_key
      } else {
        if (!(*func)(arg, level, f)) {
          return;
        }
      }
    }
  }
}
*/
Version::Version(VersionSet* vset, int level)
: vset_(vset), next_(this), prev_(this), refs_(0),
  level_num_(level),
  compaction_score_(-1),
  compaction_level_(-1),
	compaction_type_(INTERNAL_ROLLING_MERGE){



	for(int type=0;type<3;type++){
		for(int l=0;l<level;l++){
			levels_[l][type]=NULL;
			NewSortedTable(l,(SortedTableType)type);
		}
	}
}

void Version::NewSortedTable(int level, SortedTableType type){
	  SortedTable *head = new SortedTable();
	  if(levels_[level][type]==NULL){
		  head->next = head;
		  head->prev = head;
		  levels_[level][type] = head;
	  }else{
		  head->prev = levels_[level][type]->prev;
		  head->next = levels_[level][type];
		  head->prev->next = head;
		  head->next->prev = head;
		  levels_[level][type] = head;
	  }
	  //the length of the compaction buffer list is limited to a threshold, if this number is exceeded, evict the tail
	  if(type==COMPACTION_BUFFER){
		  int listlength = 0;
		  head = levels_[level][type];
		  SortedTable *cur = head;
		  do{
			 listlength++;
			 cur = cur->next;
		  }while(cur!=head&&listlength<=runtime::compaction_buffer_length[level]);

		  //detach all after
		  SortedTable *next;
		  while(cur!=head){
			  next = cur->next;
			  cur->prev->next = cur->next;
			  cur->next->prev = cur->prev;
			  delete cur;
			  cur = next;
		  }

	  }
}



Status Version::Get(const ReadOptions& options,
                    const LookupKey& k,
                    std::string* value) {
  Slice ikey = k.internal_key();
  Slice user_key = k.user_key();
  const Comparator* ucmp = vset_->icmp_.user_comparator();
  Status s;
  int order[]{COMPACTION_BUFFER,DELETION_PART,INSERTION_PART};
  //in order of descendant
  for (int level = 0; level < config::kNumLevels; level++) {

	// in order of compaction buffer, deletion part, insertion part one by one
	for(int o = 0;o<3;o++){

		int type = order[o];
		//deletion part is slightly different from other two parts in two ways:
		//1. level 0 is not fully sorted
		//2. it should be skipped is the compaction buffer is not empty
		if(type==DELETION_PART && level==0){
			  std::vector<FileMetaData *> files;

		      // Level-0 files may overlap each other.  Find all files that
		      // overlap user_key and process them in order from newest to oldest.
		      for (uint32_t i = 0; i < levels_[level][DELETION_PART]->files_.size(); i++) {

		        FileMetaData* f = levels_[level][DELETION_PART]->files_[i];
		        if (f != NULL && ucmp->Compare(user_key, f->smallest.user_key()) >= 0 &&
		            ucmp->Compare(user_key, f->largest.user_key()) <= 0) {
		          files.push_back(f);
		        }
		      }

		      if (!files.empty()){
		    	  std::sort(files.begin(), files.end(), NewestFirst);
		      }
		      for (uint32_t i = 0; i < files.size(); ++i) {


		        FileMetaData* f = files[i];

		        Saver saver;
		        saver.state = kNotFound;
		        saver.ucmp = ucmp;
		        saver.user_key = user_key;
		        saver.value = value;
		        s = vset_->table_cache_->Get(options, f->number, f->file_size,
		                                     ikey, &saver, SaveValue);
		        if (!s.ok()) {
		          return s;
		        }
		        switch (saver.state) {
		          case kNotFound:
		            break;      // Keep searching in other files
		          case kFound:
			        //printf("find in %d part of level %d\n",type,level);
		            return s;
		          case kDeleted:
		            s = Status::NotFound(Slice());  // Use empty error message for speed
		            return s;
		          case kCorrupt:
		            s = Status::Corruption("corrupted key for ", user_key);
		            return s;
		        }
		      }

		}else if(type==DELETION_PART && level>0
				&& !levels_[level][COMPACTION_BUFFER]->next->obsolete&&levels_[level][COMPACTION_BUFFER]->next->files_.size()>0){
			//if the compaction buffer list is set to empty, deletion part should be checked
			//skip otherwise
			continue;
		}else{

			SortedTable *head = levels_[level][type];
			SortedTable *cur = head;

			int tablechecked = 0;
			do{
				if(!cur->obsolete)
				{
					//limit the number of sorted tables checked in compaction buffer(for test only)
	//				if(type==COMPACTION_BUFFER && tablechecked++>=runtime::compaction_buffer_length[level]){
	//					break;
	//				}
					// Binary search to find earliest index whose largest key >= ikey.
					uint32_t index = FindFile(vset_->icmp_, cur->files_, ikey);

					if (index < cur->files_.size()) {

						//find!
						FileMetaData* f = cur->files_[index];
						if(type==COMPACTION_BUFFER&&!f->visible){//this file in the compaction buffer is not visible
							cur = cur->next;
							continue;
						}
						if (ucmp->Compare(user_key, f->smallest.user_key())>=0) {
							Saver saver;
							saver.state = kNotFound;
							saver.ucmp = ucmp;
							saver.user_key = user_key;
							saver.value = value;
							s = vset_->table_cache_->Get(options, f->number, f->file_size,
														 ikey, &saver, SaveValue);
							if (!s.ok()) {
								return s;
							}
							switch (saver.state) {
							case kNotFound:
								 break;      // Keep searching in other files
							case kFound:
								 // printf("end3\n");

								 return s;
							case kDeleted:
								 s = Status::NotFound(Slice());  // Use empty error message for speed
								 return s;
							case kCorrupt:
								 s = Status::Corruption("corrupted key for ", user_key);
								 return s;
							}
						}
					}
				}

				cur = cur->next;
			}while(cur!=head);
		}//cases if
	}//part for
  }//level for

  return Status::NotFound(Slice());  // Use an empty error message for speed
}


//do range query bypass the table cache
/*static int readFileinRange(FileMetaData* f, Slice start,Slice end) {

	char name[100];
	sprintf(name,"%s/%06ld.ldb",leveldb::config::db_path,f->number);
	std::string fname(name);


	 * keys found in this file
	 *
	int count = 0;
	leveldb::Env* env = leveldb::Env::Default();
	RandomAccessFile* file = NULL;
	Table* table = NULL;
	Status s = env->NewRandomAccessFile(fname, &file);

	if (s.ok()) {
	   s = Table::Open(Options(), f->number,file, f->file_size, &table);
	}
	if (!s.ok()) {
	   //fprintf(stderr, "wrong %s\n", s.ToString().c_str());
	   delete table;
	   delete file;
	   return -1;
	}
	int64_t bytes = 0;
	ReadOptions ro;
	leveldb::Options options;
	ro.fill_cache = false;
	Iterator* iter = table->NewIterator(ro);
    for (iter->Seek(start); iter->Valid(); iter->Next()) {
        if(options.comparator->Compare(iter->key(), end) > 0) {
    	 break;
    	}
    	count++;
    	bytes += iter->key().size() + iter->value().size();
    }

    s = iter->status();
    if (!s.ok()) {
      printf("iterator error: %s\n", s.ToString().c_str());
    }
    delete iter;
    delete table;
    delete file;
    return count;
}*/

int Version::RangeQuery(const ReadOptions& options,
                    const LookupKey &lkstart,
                    const LookupKey &lkend) {
  Slice start = lkstart.user_key();
  Slice end = lkend.user_key();
  const Comparator* ucmp = vset_->icmp_.user_comparator();

  ReadOptions cboption = options;
  cboption.fill_cache = true;
  cboption.range_query_ = true;
  std::vector<FileMetaData*> tmp;
  int found = 0;
  int filecount = 0;
  leveldb::Options tmpopt;
  //ignore level 0
  for (int level = 1; level < level_num_; level++) {

	  SortedTable *head = levels_[level][COMPACTION_BUFFER];
	  SortedTable *cur = head;
	  bool findinCB = false;

	  do{
		if(!cur->obsolete&&!cur->evictable)
		{
			size_t num_files = cur->files_.size();
			if (num_files != 0){
				// Get the list of files to search in this part
				FileMetaData* const* files = &cur->files_[0];
				for (uint32_t i = 0; i < num_files; i++) {
					FileMetaData* f = files[i];
					if (f->visible&&ucmp->Compare(end, f->smallest.user_key()) >= 0&&ucmp->Compare(start, f->largest.user_key()) <= 0){
						tmp.push_back(f);
						findinCB = true;
					}
				}
			}
		}
		cur = cur->next;
	 }while(cur!=head);

	 if(tmp.size()==0||!findinCB){
		 tmp.clear();
		 if(levels_[level][DELETION_PART]!=NULL){
		 		size_t num_files = levels_[level][DELETION_PART]->files_.size();
		 		if (num_files != 0){
		 			// Get the list of files to search in this part
		 			FileMetaData* const* files = &levels_[level][DELETION_PART]->files_[0];
		 			for (uint32_t i = 0; i < num_files; i++) {
		 				FileMetaData* f = files[i];
		 				if (ucmp->Compare(end, f->smallest.user_key()) >= 0&&ucmp->Compare(start, f->largest.user_key()) <= 0){
		 					tmp.push_back(f);
		 				}
		 			}
		 		}
		 	}
		 	if(levels_[level][INSERTION_PART]!=NULL){
		 			size_t num_files = levels_[level][INSERTION_PART]->files_.size();
		 			if (num_files != 0){
		 				// Get the list of files to search in this part
		 				FileMetaData* const* files = &levels_[level][INSERTION_PART]->files_[0];
		 				for (uint32_t i = 0; i < num_files; i++) {
		 					FileMetaData* f = files[i];
		 					if (ucmp->Compare(end, f->smallest.user_key()) >= 0&&ucmp->Compare(start, f->largest.user_key()) <= 0){
		 						tmp.push_back(f);
		 					}
		 				}
		 			}
		 	}
	 }

	   for(int i=0;i<tmp.size();i++)
	   {
	 	  FileMetaData* f = tmp[i];
	 	  Table *table;
	 	  vset_->table_cache_->GetTable(f->number,f->file_size,&table);

	 	  Iterator* iter;
	 	  if(findinCB){
	 		  iter = table->NewIterator(cboption);
	 	  }else{
	 		  iter = table->NewIterator(options);

	 	  }
 		  //iter = table->NewIterator(cboption);

	 	  //iter->SeekToFirst();
	 	  iter->Seek(lkstart.internal_key());
	 	  bool hasone = false;
	 	  if(iter->Valid()){
	 		  for (; iter->Valid(); iter->Next()) {
	 			  found++;

	 			  if(ucmp->Compare(iter->key(), start) < 0){
	 				 continue;
	 			  }
	 			  if(tmpopt.comparator->Compare(iter->key(), end) > 0){
	 				  break;
	 			  }
	 			  hasone = true;
	 		   }
 			  filecount++;


	 		   Status s = iter->status();
	 		   if (!s.ok()) {
	 				printf("iterator error: %s\n", s.ToString().c_str());
	 		   }
	 	  }

	 	  delete iter;
	   }

	  //printf("%d files are checked, %d records are found\n",filecount,found);
	   tmp.clear();


  }




  return found;
}


void Version::Ref() {
  ++refs_;
}

void Version::Unref() {
  assert(this != &vset_->dummy_versions_);
  assert(refs_ >= 1);
  --refs_;
  if (refs_ == 0) {
    delete this;
  }
}

// Store in "*inputs" all files in "level" that overlap [begin,end]
void Version::GetOverlappingInputs(
	int type,
    int level,
    const InternalKey* begin,
    const InternalKey* end,
    std::vector<FileMetaData*>* inputs) {
  assert(level >= 0);
  assert(level < config::kNumLevels);
  inputs->clear();
  Slice user_begin, user_end;
  if (begin != NULL) {
    user_begin = begin->user_key();
  }
  if (end != NULL) {
    user_end = end->user_key();
  }
  const Comparator* user_cmp = vset_->icmp_.user_comparator();
  for (size_t i = 0; i < levels_[level][type]->files_.size(); ) {
    FileMetaData* f = levels_[level][type]->files_[i++];
    const Slice file_start = f->smallest.user_key();
    const Slice file_limit = f->largest.user_key();
    if (begin != NULL && user_cmp->Compare(file_limit, user_begin) < 0) {
      // "f" is completely before specified range; skip it
    } else if (end != NULL && user_cmp->Compare(file_start, user_end) > 0) {
      // "f" is completely after specified range; skip it
    } else {
      inputs->push_back(f);
      if (level == 0) {
        // Level-0 files may overlap each other.  So check if the newly
        // added file has expanded the range.  If so, restart search.
        if (begin != NULL && user_cmp->Compare(file_start, user_begin) < 0) {
          user_begin = file_start;
          inputs->clear();
          i = 0;
        } else if (end != NULL && user_cmp->Compare(file_limit, user_end) > 0) {
          user_end = file_limit;
          inputs->clear();
          i = 0;
        }
      }
    }
  }
}

void Version::RefineCompactionBuffer(const int level){
	if(!config::manage_compaction_buffer)
	return;

	uint64_t cursor[40];
	SortedTable *head = levels_[level][COMPACTION_BUFFER];
	SortedTable *tail = head->prev;//start from the tail

	SortedTable *cur = tail;//start from the tail
	//leave the first few sorted tables serving read for a while
	int counter = 0;
	while(cur!=head){//keep all files of the head in compaction buffer for a while
//		if(level>2){
//			if(cur==head){
//				break;
//			}
//		}else{
//			if(cur==head->next||cur==head){
//				break;
//			}
//
//		}
		counter++;
//		if(level==2){
//			 printf("%d %ld\n",counter,cur->files_.size());
//	    }
		if(cur->secondchance){
			cur->secondchance = false;
		}else
		for(int i=0;i<cur->files_.size();i++){

			FileMetaData *f = cur->files_[i];
			if(!f->visible){
				continue;
			}

			 for(int j=0;j<40;j++){
				cursor[j] = 0;
			 }
			 Table * table = NULL;
			 int num = f->number;
			 vset_->table_cache_->GetTable(f->number,f->file_size,&table);
			 bool visible = false;
			 if(table == NULL){//never be visited

			 }else if(table->isHot()){//a hot file
				 //printf("%10ld %10ld\n", f->number,table->GetVistedNum());
				 visible = true;
				 //printf("added because hot\n");
			 }
			 else if(cur==head->next){//overlap with older file in the compaction buffer
				 SortedTable *cur_temp = head->prev;//start from the tail
				 int cblevel = 0;
				 while(cur_temp!=cur&&!visible){
					 leveldb::FileMetaData* const* cbfiles = &cur_temp->files_[0];
					 const Comparator* user_cmp = vset_->icmp_.user_comparator();
					 for (; cursor[cblevel] < cur_temp->files_.size(); ) {

					   FileMetaData* cbf = cbfiles[cursor[cblevel]];
					   if(!cbf->visible){//skip invisible files
							cursor[cblevel]++;
							continue;
					   }
					   const Slice cbfstart = cbf->smallest.user_key();
					   const Slice cbflimit = cbf->largest.user_key();
					   const Slice fstart = f->smallest.user_key();
					   const Slice flimit = f->largest.user_key();

					   //printf(" %d %s %s %s %s ",cblevel,cbfstart.ToString().c_str(),cbflimit.ToString().c_str(),fstart.ToString().c_str(),flimit.ToString().c_str());
					   if(user_cmp->Compare(fstart,cbflimit)>0){
						   //printf(" hehe1\n");
						   cursor[cblevel]++;
					   }else if(user_cmp->Compare(flimit,cbfstart)<0){
						   //printf(" hehe2\n");
						   break;
					   }else{//overlap
						   //printf(" hehe3\n");
						   visible = true;
						   //printf("added because overlap\n");
						   break;
					   }

					 }
					 cblevel++;
					 cur_temp = cur_temp->prev;
					 //printf("cblevel: %d\n",cblevel);
				 }
			 }
			 f->visible = visible;
			 if(!f->visible){
				 //vset_->table_cache_->Evict(f->number,f->file_size);
			 }
			 table->ClearVisitedNum();

			 //printf("%d\n",visible);
		}
//		if(cur->NumVisibleFiles()==0){
//			cur->evictable = true;
//		}
		cur = cur->prev;
		//printf("\n");
	}

}

uint64_t Version::TotalLevelSize(int level){

	  return TotalPartSize(level,DELETION_PART)+TotalPartSize(level,INSERTION_PART);
      //return TotalFileSizeMB(v->levels_[level][DELETION_PART]->files_)+TotalFileSizeMB(v->levels_[level][INSERTION_PART]->files_);
}

uint64_t Version::TotalPartSize(int level, SortedTableType type){

	  SortedTable *head = levels_[level][type];
	  SortedTable *cur = head;
	  uint64_t total = 0;
	  do{
		  total += TotalFileSizeMB(cur->files_);
		  cur = cur->next;
	  }while(cur!=head);

	  return total;
}

void Version::printVersion(){

	  fprintf(stderr,"---------------------------------------------------------------------------\n");
	  fprintf(stderr,"table cache usage:%2.3f\n", this->vset_->table_cache_->Usage());
	  //int max = config::kNumLevels-1;
	  int max = runtime::max_print_level;
	  for(;max>=0;max--){//last level contains files
	  	   if(levels_[max][DELETION_PART]->files_.size()+levels_[max][INSERTION_PART]->files_.size()!=0){
	  		   break;
	  	   }
	  }

	  int counter = 0;
	  for(int level=0;level<=max;level++){
	      if(this->NumFiles(level)+this->NumPartFiles(level,COMPACTION_BUFFER)==0)continue;
	      	/*if(runtime::two_phase_compaction&&llevel!=0){
	      		llevel = (llevel-1)/2+1;
	      	}*/
	      	fprintf(stderr,"level:%d\n",level);
	      	fprintf(stderr,"deletion part: ");
	      	SortedTable *head = levels_[level][DELETION_PART];
	      	SortedTable *cur = head;
	      	counter = 0;
	      	do{
		        fprintf(stderr,"\n\t%2d(%5ld files)| ",counter++,cur->files_.size());
	      		for(int j=0;j<cur->files_.size();j++){
	      			if(!runtime::print_dash){
	      				fprintf(stderr,"%ld ",cur->files_[j]->number);
	      			}else if(level==0||j%((int)pow(10,(level-1)))==0){
					    fprintf(stderr,"-");
					}

	      		}
	      		cur = cur->next;
	      	}while(cur!=head);
	        fprintf(stderr,"\n");
	        fprintf(stderr,"insertion part: ");
	      	head = levels_[level][INSERTION_PART];
	      	cur = head;
	      	counter = 0;

	      	do{
		        fprintf(stderr,"\n\t%2d(%5ld files)| ",counter++,cur->files_.size());
	      		for(int j=0;j<cur->files_.size();j++){

	      			if(!runtime::print_dash){
	      			     fprintf(stderr,"%ld ",cur->files_[j]->number);
	      			}else if(level==0||j%((int)pow(10,(level-1)))==0){
					    fprintf(stderr,"-");
					}
	      		}
	      		cur = cur->next;
	      	}while(cur!=head);
	        fprintf(stderr,"\n");
	        if(!runtime::print_compaction_buffer||level==0)continue;

	        fprintf(stderr,"compaction buffer: ");
	        head = levels_[level][COMPACTION_BUFFER];
	        cur = head;
	      	counter = 0;

	      	//fprintf(stderr,"\n");;
	        do{
	        	if(!cur->obsolete)
	        	{

					uint64_t num = cur->NumVisibleFiles();
					if(num!=0)
					{
						int count = 0;
						fprintf(stderr,"\n\t%2d(%5ld files)| ",counter,num);
						for(int j=0;j<cur->files_.size();j++){
							if(cur->files_[j]->visible)
							{
							  if(!runtime::print_dash){
									fprintf(stderr,"%ld ",cur->files_[j]->number);
							  }else if(level==0||j%((int)pow(10,(level-1)))==0){
								    fprintf(stderr,"-");
								}
							  count++;
							}
						}
						if(cur->evictable){
							fprintf(stderr,"* evictable");
						}
					}
					counter++;
	        	}
	        	cur = cur->next;
	       	}while(cur!=head);
	        fprintf(stderr,"\n");
	  }
}



// A helper class so we can efficiently apply a whole sequence
// of edits to a particular state without creating intermediate
// Versions that contain full copies of the intermediate state.
class BasicVersionSet::Builder {
 private:
  // Helper to sort by v->files_[file_number].smallest
  struct BySmallestKey {
    const InternalKeyComparator* internal_comparator;

    bool operator()(FileMetaData* f1, FileMetaData* f2) const {
      int r = internal_comparator->Compare(f1->smallest, f2->smallest);
      if (r != 0) {
        return (r < 0);
      } else {
        // Break ties by file number
        return (f1->number < f2->number);
      }
    }
  };

  typedef std::set<FileMetaData*, BySmallestKey> FileSet;
  struct LevelState {
    std::set<uint64_t> deleted_files;
    //FileSet* added_files;
    std::vector<FileMetaData*> added_files;
  };

  VersionSet* vset_;
  Version* base_;
  LevelState levels_[config::kNumLevels][3];

 public:
  // Initialize a builder with the files from *base and other info from *vset
  Builder(VersionSet* vset, Version* base)
      : vset_(vset),
        base_(base) {
    base_->Ref();
    BySmallestKey cmp;
    cmp.internal_comparator = &vset_->icmp_;
  }

  ~Builder() {
	for(int type = 0;type<3;type++)
    for (int level = 0; level < config::kNumLevels; level++) {
      for (uint32_t i = 0; i < levels_[level][type].added_files.size(); i++) {
        FileMetaData* f = levels_[level][type].added_files[i];
        f->refs--;
        if (f->refs <= 0) {
          delete f;
        }
      }
      levels_[level][type].added_files.clear();
    }
    base_->Unref();
  }

  // Apply all of the edits in *edit to the current state.
  void Apply(VersionEdit* edit) {

  //TODO change for deletion part insertion part now, compaction buffer later
  for(int type = 0;type<3;type++){
    // Delete files
    const VersionEdit::DeletedFileSet& del = edit->deleted_files_[type];
    for (VersionEdit::DeletedFileSet::const_iterator iter = del.begin();
         iter != del.end();
         ++iter) {
      const int level = iter->first;
      const uint64_t number = iter->second;
      levels_[level][type].deleted_files.insert(number);
    }

    // Add new files
    for (size_t i = 0; i < edit->new_files_[type].size(); i++) {
      const int level = edit->new_files_[type][i].first;
      FileMetaData* f = new FileMetaData(edit->new_files_[type][i].second);
      f->refs = 1;
      levels_[level][type].deleted_files.erase(f->number);
      levels_[level][type].added_files.push_back(f);
    }
  }
}

  // Save the current state in *v.
void SaveTo(Version* v) {
 BySmallestKey cmp;
 cmp.internal_comparator = &vset_->icmp_;
 //TODO for all three part of data, all insertion data is inserted at the head only, if at some point the greater key of the latter file is
 //greater than the former file, create a new sorted table and continue insert file at the head.
 for (int level = 0; level < config::kNumLevels; level++) {
	for(int type= 0;type<3;type++){
	  // before inserting new file into the head, go check the sorted table list from the tail, and insert the files which are not deleted. treat it as
	  // another insert list start from the last
	  SortedTable *curtable = base_->levels_[level][type]->prev;//set to the tail of the list

	  bool started = false;
	  while(curtable!=base_->levels_[level][type]){//for all sorted tables except the head one,

		  if(!curtable->obsolete){
			  if(!started){
				  started = true;
				  v->levels_[level][type]->evictable = curtable->evictable;
				  v->levels_[level][type]->secondchance = curtable->secondchance;
			  }
			  // Drop any deleted files.  Store the result in *v.
			  const std::vector<FileMetaData*>& base_files = curtable->files_;
			  std::vector<FileMetaData*>::const_iterator base_iter = base_files.begin();
			  std::vector<FileMetaData*>::const_iterator base_end = base_files.end();
			  //if the head of the list is unempty, create a new one to accept files from current table
			  if(v->levels_[level][type]->files_.size()!=0){
				  v->NewSortedTable(level,(SortedTableType)type);
				  v->levels_[level][type]->evictable = curtable->evictable;
				  v->levels_[level][type]->secondchance = curtable->secondchance;

			  }
			  v->levels_[level][type]->files_.reserve(base_files.size());
			  //add all files in curtable but not in the deleted file list to the new list of v
			  for (;base_iter != base_end;++base_iter) {
				  MaybeAddFile(v, type, level, *base_iter);
			  }
		  }
		  curtable = curtable->prev;
	  }

      //the head of the compaction buffer always empty
      if(type==COMPACTION_BUFFER&&v->levels_[level][type]->files_.size()!=0){
      	  v->NewSortedTable(level,(SortedTableType)type);
      }

	  //now insert the new file into the head
      // Merge the set of added files with the set of pre-existing files.
      // Drop any deleted files.  Store the result in *v.
      const std::vector<FileMetaData*>& base_files = base_->levels_[level][type]->files_;

      std::vector<FileMetaData*>::const_iterator base_iter = base_files.begin();
      std::vector<FileMetaData*>::const_iterator base_end = base_files.end();
      std::vector<FileMetaData*>& added = levels_[level][type].added_files;
      v->levels_[level][type]->files_.reserve(base_files.size() + added.size());
      InternalKey preLargest;
      bool first = true;
      for (int i=0;i<added.size();i++) {
    	//if the new files are not in the same sorted table, create a new one when current file's smallest key is smaller than the previous one's largest key
    	  //happens only when the base is empty
    	if(level>0&&i>0&&vset_->icmp_.Compare(added[i]->smallest, preLargest)<0){
    		v->NewSortedTable(level,(SortedTableType)type);
    	}

    	preLargest = added[i]->largest;

        // Add all smaller files listed in base_
        for (std::vector<FileMetaData*>::const_iterator bpos
                 = std::upper_bound(base_iter, base_end, added[i], cmp);
             base_iter != bpos;
             ++base_iter) {
          MaybeAddFile(v, type, level, *base_iter);
        }
        MaybeAddFile(v, type, level, added[i]);
        first = false;
      }

      // Add remaining base files
      for (; base_iter != base_end; ++base_iter) {
        MaybeAddFile(v, type, level, *base_iter);
      }
      //the head of the compaction buffer always empty
      if(type==COMPACTION_BUFFER&&v->levels_[level][type]->files_.size()!=0){
      	  v->NewSortedTable(level,(SortedTableType)type);
      }

    }//end type for loop
}//end level for loop


#ifndef NDEBUG
      // Make sure there is no overlap in the insertion part of levels > 0
  for(int level=0;level<config::kNumLevels;level++){
     for(int type = 0;type<3;type++)
      if (level > 0) {
        for (uint32_t i = 1; i < v->levels_[level][type]->files_.size(); i++) {
          const InternalKey& prev_end = v->levels_[level][type]->files_[i-1]->largest;
          const InternalKey& this_begin = v->levels_[level][type]->files_[i]->smallest;
          if (vset_->icmp_.Compare(prev_end, this_begin) >= 0) {
            fprintf(stderr, "overlapping ranges in same level %s vs. %s\n",
                    prev_end.DebugString().c_str(),
                    this_begin.DebugString().c_str());
            abort();
          }
        }
      }
 }
#endif

}

  void MaybeAddFile(Version* v, int type, int level, FileMetaData* f) {
    if (levels_[level][type].deleted_files.count(f->number) > 0) {
      // File is deleted: do nothing
    } else {
      std::vector<FileMetaData*>* files = &v->levels_[level][type]->files_;
      if (level > 0 && !files->empty()) {
        // Must not overlap
    	//if(vset_->icmp_.Compare((*files)[files->size()-1]->largest,f->smallest) >= 0)
    	//printf("%d %d\n",level, type);
        assert(vset_->icmp_.Compare((*files)[files->size()-1]->largest,f->smallest) < 0);
      }
      v->levels_[level][type]->AddFile(f);
      //f->refs++;
      //files->push_back(f);
    }
  }

};

VersionSet::VersionSet(const std::string& dbname,
                       const Options* options,
                       TableCache* table_cache,
                       const InternalKeyComparator* cmp
                       )
    : env_(options->env),
      dbname_(dbname),
      options_(options),
      table_cache_(table_cache),
      icmp_(*cmp),
      next_file_number_(2),
      manifest_file_number_(0),  // Filled by Recover()
      last_sequence_(0),
      log_number_(0),
      prev_log_number_(0),
      descriptor_file_(NULL),
      descriptor_log_(NULL),
      dummy_versions_(this),
      current_(NULL)
      {}

VersionSet::~VersionSet() {
}

BasicVersionSet::BasicVersionSet(const std::string& dbname,
                       const Options* options,
                       TableCache* table_cache,
                       const InternalKeyComparator* cmp
                       )
    : VersionSet(dbname, options, table_cache, cmp)
      {
	  AppendVersion(new Version(this));
	  for(int i=0;i<config::kNumLevels;i++){
		  this->num_hot_files_deleted_[i] = 0;
		  this->second_chance_[i] = true;
	  }
}

BasicVersionSet::~BasicVersionSet() {
  current_->Unref();
  assert(dummy_versions_.next_ == &dummy_versions_);  // List must be empty
  delete descriptor_log_;
  delete descriptor_file_;
}

void VersionSet::AppendVersion(Version* v) {
  // Make "v" current
  assert(v->refs_ == 0);
  assert(v != current_);
  if (current_ != NULL) {
    current_->Unref();
  }
  current_ = v;
  v->Ref();

  // Append to linked list
  v->prev_ = dummy_versions_.prev_;
  v->next_ = &dummy_versions_;
  v->prev_->next_ = v;
  v->next_->prev_ = v;
}

Status BasicVersionSet::LogAndApply(VersionEdit* edit, port::Mutex* mu) {
  if (edit->has_log_number_) {
    assert(edit->log_number_ >= log_number_);
    assert(edit->log_number_ < next_file_number_);
  } else {
    edit->SetLogNumber(log_number_);
  }

  if (!edit->has_prev_log_number_) {
    edit->SetPrevLogNumber(prev_log_number_);
  }

  edit->SetNextFile(next_file_number_);
  edit->SetLastSequence(last_sequence_);

  Version* v = new Version(this);
  {
    Builder builder(this, current_);
    builder.Apply(edit);
    builder.SaveTo(v);
    if(edit->GetRefineCB()){
    	//v->RefineCompactionBuffer(edit->GetRefineLevel());
    	if(edit->GetRefineLevel()-1!=0){
    		v->MarkAllEvictable(edit->GetRefineLevel()-1);
    	}
    }
  }
  Finalize(v);


  // Initialize new descriptor log file if necessary by creating
  // a temporary file that contains a snapshot of the current version.
  std::string new_manifest_file;
  Status s;
  mu->Unlock();
  assert(descriptor_file_ == NULL);
  new_manifest_file = DescriptorFileName(dbname_, manifest_file_number_);
  edit->SetNextFile(next_file_number_);
  env_->DeleteFile(new_manifest_file);
  s = env_->NewWritableFile(new_manifest_file, &descriptor_file_);
  assert(s.ok());
  descriptor_log_ = new log::Writer(descriptor_file_);
  s = WriteSnapshot(v,descriptor_log_,edit);
  assert(s.ok());
  s = descriptor_file_->Sync();
  assert(s.ok());
  s = SetCurrentFile(env_, dbname_, manifest_file_number_);
  assert(s.ok());

  mu->Lock();

  // Install the new version
  if (s.ok()) {
    AppendVersion(v);
    log_number_ = edit->log_number_;
    prev_log_number_ = edit->prev_log_number_;
  } else {
    delete v;
  }
  delete descriptor_log_;
  delete descriptor_file_;
  descriptor_log_ = NULL;
  descriptor_file_ = NULL;


  //printf("current: %ld is set\n",manifest_file_number_);
  //if(edit->deleted_files_[DELETION_PART].size()==0)
  printCurVersion();
  return s;
}

Status BasicVersionSet::Recover() {
  struct LogReporter : public log::Reader::Reporter {
    Status* status;
    virtual void Corruption(size_t bytes, const Status& s) {
      if (this->status->ok()) *this->status = s;
    }
  };

  // Read "CURRENT" file, which contains a pointer to the current manifest file
  std::string current;
  Status s = ReadFileToString(env_, CurrentFileName(dbname_), &current);
  if (!s.ok()) {
    return s;
  }
  if (current.empty() || current[current.size()-1] != '\n') {
    return Status::Corruption("CURRENT file does not end with newline");
  }
  current.resize(current.size() - 1);

  std::string dscname = dbname_ + "/" + current;
  SequentialFile* file;
  s = env_->NewSequentialFile(dscname, &file);
  if (!s.ok()) {
    return s;
  }

  bool have_log_number = false;
  bool have_prev_log_number = false;
  bool have_next_file = false;
  bool have_last_sequence = false;
  uint64_t next_file = 0;
  uint64_t last_sequence = 0;
  uint64_t log_number = 0;
  uint64_t prev_log_number = 0;
  Builder builder(this, current_);

  {
    LogReporter reporter;
    reporter.status = &s;
    log::Reader reader(file, &reporter, true/*checksum*/, 0/*initial_offset*/);
    Slice record;
    std::string scratch;
    while (reader.ReadRecord(&record, &scratch) && s.ok()) {
      VersionEdit edit;
      s = edit.DecodeFrom(record);
      if (s.ok()) {
        if (edit.has_comparator_ &&
            edit.comparator_ != icmp_.user_comparator()->Name()) {
          s = Status::InvalidArgument(
              edit.comparator_ + " does not match existing comparator ",
              icmp_.user_comparator()->Name());
        }
      }

      if (s.ok()) {
        builder.Apply(&edit);
        std::vector< std::pair<int, FileMetaData> >* new_files_ = edit.GetNewFiles();
        if(config::preload_metadata){
			printf("loading metadata\n");
			for(int i=0;i<3;i++){
				for(int j=0;j<new_files_[i].size();j++){
					this->table_cache_->LoadTable(new_files_[i][j].second.number,new_files_[i][j].second.file_size);
				}
			}
			printf("done loading metadata\n");
        }
      }

      if (edit.has_log_number_) {
        log_number = edit.log_number_;
        have_log_number = true;
      }

      if (edit.has_prev_log_number_) {
        prev_log_number = edit.prev_log_number_;
        have_prev_log_number = true;
      }

      if (edit.has_next_file_number_) {
        next_file = edit.next_file_number_;
        have_next_file = true;
      }

      if (edit.has_last_sequence_) {
        last_sequence = edit.last_sequence_;
        have_last_sequence = true;
      }
    }
  }
  delete file;
  file = NULL;

  if (s.ok()) {
    if (!have_next_file) {
      s = Status::Corruption("no meta-nextfile entry in descriptor");
    } else if (!have_log_number) {
      s = Status::Corruption("no meta-lognumber entry in descriptor");
    } else if (!have_last_sequence) {
      s = Status::Corruption("no last-sequence-number entry in descriptor");
    }

    if (!have_prev_log_number) {
      prev_log_number = 0;
    }

    MarkFileNumberUsed(prev_log_number);
    MarkFileNumberUsed(log_number);
  }

  if (s.ok()) {
    Version* v = new Version(this);
    builder.SaveTo(v);
    // Install recovered version
    Finalize(v);
    AppendVersion(v);
    manifest_file_number_ = next_file;
    next_file_number_ = next_file + 1;
    last_sequence_ = last_sequence;
    log_number_ = log_number;
    prev_log_number_ = prev_log_number;
  }

  return s;
}



std::string compacttype[]{"move","merge"};
void VersionSet::Finalize(Version* v) {
  // Precomputed best level for next compaction
  int best_level = -1;
  double best_score = -1;
  CompactionType best_type = INTERNAL_ROLLING_MERGE;
  for (int level = 0; level < config::kNumLevels; level++) {

  	    double score;
  	    CompactionType type;
  	    if (level == 0) {
  	      // We treat level-0 specially by bounding the number of files
  	      // instead of number of bytes for two reasons:
  	      //
  	      // (1) With larger write-buffer sizes, it is nice not to do too
  	      // many level-0 compactions.
  	      //
  	      // (2) The files in level-0 are merged on every read and
  	      // therefore we wish to avoid too many files when the individual
  	      // file size is small (perhaps because of a small write-buffer
  	      // setting, or very high compression ratios, or lots of
  	      // overwrites/deletions).



	      double score_ins = (double)v->TotalPartSize(0,INSERTION_PART) / MaxMBytesForLevel(0);
	      double score_del = (double)v->TotalPartSize(0,DELETION_PART) / config::kL0_CompactionTrigger;

	  	  //the insertion part of level 0 has higher priority over deletion part
  	      if(score_ins>1){//the insertion part of level 0 can move to level 1 if the deletion part of level 1 is empty
  	    	  //check if the insertion part of level 0 reach the maximum size, if so, ready to serve
  			  type = LEVEL_MOVE;
  			  score = score_ins;
  	      }else if(score_del>1){
  	    	  //internal rolling merge happens only when the number of deletion part's files reach one threshold
  	    	  //in the deletion part to a fully sorted structure into the insertion part
  	  	      type = INTERNAL_ROLLING_MERGE;
  	  	      //if both the deletion part and insertion part are "full", always move the insertion part down first
  	  	      //and also bound the max score to 1.01 to make sure the tree is in a good shape before level 0 start to accept new data
  		  	  score = score_del;

  	      }else{
  	    	  score = 0;
  	      }

  	      score = std::min(score,config::level0_max_score);
  	    } else {
  	    	if (v->NumFiles(level) == 0){
  	    			score = 0;
  	    	} else{
  	    		//deletion part is empty, the only possible operation on this level is level move to next level
  	    		if(v->NumPartFiles(level,DELETION_PART)==0){
  	    			//printf("%d why are you here? %d %ld\n",level,v->NumPartFiles(level,DELETION_PART),v->levels_[level][DELETION_PART]->files_.size());
		    		type = LEVEL_MOVE;

  	    			if(level+1<config::kNumLevels&&v->NumPartFiles(level+1,DELETION_PART)==0){
  	    				score =  (double)v->TotalPartSize(level,INSERTION_PART) / MaxMBytesForLevel(level);
  	    				if(level==2){
  	  	    				score =  (double)v->TotalPartSize(level,COMPACTION_BUFFER) / MaxMBytesForLevel(level);
  	    				}
  	    			}else{
  	    				score = 0;
  	    			}
  	    		//if the deletion part is not empty, then only internal rolling merge could happen in this level
  	    		}else{
  	    			type = INTERNAL_ROLLING_MERGE;
  	    			if(level==3){
  	  	    			score = ((double)v->TotalPartSize(level-1,COMPACTION_BUFFER)+(double)v->TotalPartSize(level,DELETION_PART)) / MaxMBytesForLevel(level-1);    // LX.R
  	    			}else{
  	    				score = ((double)v->TotalLevelSize(level-1)+(double)v->TotalPartSize(level,DELETION_PART)) / MaxMBytesForLevel(level-1);    // LX.R
  	    			}
  	    		}
  	    	}
  	    }
  	    if (score > best_score) {
  	      best_level = level;
  	      best_score = score;
  	      best_type = type;
  	    }
  }

  v->compaction_level_ = best_level;
  v->compaction_score_ = best_score;
  v->compaction_type_ = best_type;
}

//teng: print current version
void BasicVersionSet::printCurVersion(){
	  if(!leveldb::runtime::print_version_info){
		  return;
	  }
	  current_->printVersion();

}


Status VersionSet::WriteSnapshot(Version *v, log::Writer* log, VersionEdit *newedit) {
  // Break up into multiple records to reduce memory usage on recovery?

  // Save metadata
  VersionEdit edit;
  edit.SetComparatorName(icmp_.user_comparator()->Name());
  newedit->CloneMeta(&edit);

  // Save files
  for (int level = 0; level < config::kNumLevels; level++) {

	  for(int type=0;type<3;type++){
		  SortedTable *tail = v->levels_[level][type]->prev;
		  SortedTable *cur = tail;
		  do{
			  if(!cur->evictable){
				  const std::vector<FileMetaData*>& files = cur->files_;
				  for (int i = 0; i < files.size(); i++) {
						const FileMetaData* f = files[i];
						edit.AddFile((SortedTableType)type,level,f->number,f->file_size, f->smallest, f->largest);
				  }
			  }

			  cur = cur->prev;
		  }while(cur!=tail);

	  }
  }

  std::string record;
  edit.EncodeTo(&record);
  return log->AddRecord(record);
}

int BasicVersionSet::NumLevelFiles(int level){
  assert(level >= 0);
  assert(level < config::kNumLevels);
  return NumPartFiles(level,DELETION_PART)+NumPartFiles(level,INSERTION_PART);
}

int BasicVersionSet::NumPartFiles(int level,SortedTableType part){
	assert(level >= 0);
	assert(level < config::kNumLevels);
	return current_->NumPartFiles(level,part);

}




int64_t VersionSet::NumLevelBytes(int level) const {
  assert(level >= 0);
  assert(level < config::kNumLevels);
  return TotalFileSize(current_->levels_[level][DELETION_PART]->files_)+TotalFileSize(current_->levels_[level][INSERTION_PART]->files_);
}


// Stores the minimal range that covers all entries in inputs in
// *smallest, *largest.
// REQUIRES: inputs is not empty
void VersionSet::GetRange(const std::vector<FileMetaData*>& inputs,
                          InternalKey* smallest,
                          InternalKey* largest) {
  assert(!inputs.empty());
  smallest->Clear();
  largest->Clear();
  for (size_t i = 0; i < inputs.size(); i++) {
    FileMetaData* f = inputs[i];
    if (i == 0) {
      *smallest = f->smallest;
      *largest = f->largest;
    } else {
      if (icmp_.Compare(f->smallest, *smallest) < 0) {
        *smallest = f->smallest;
      }
      if (icmp_.Compare(f->largest, *largest) > 0) {
        *largest = f->largest;
      }
    }
  }
}

// Stores the minimal range that covers all entries in inputs1 and inputs2
// in *smallest, *largest.
// REQUIRES: inputs is not empty
void VersionSet::GetRange2(const std::vector<FileMetaData*>& inputs1,
                           const std::vector<FileMetaData*>& inputs2,
                           InternalKey* smallest,
                           InternalKey* largest) {
  std::vector<FileMetaData*> all = inputs1;
  all.insert(all.end(), inputs2.begin(), inputs2.end());
  GetRange(all, smallest, largest);
}


Iterator* BasicVersionSet::MakeInputIterator(Compaction* c) {
  ReadOptions options;
  options.verify_checksums = options_->paranoid_checks;
  options.fill_cache = false;

  // Level-0 files have to be merged together.  For other levels,
  // we will make a concatenating iterator per level.
  // (opt): use concatenating iterator for level-0 if there is no overlap

  const int space = (c->level() == 0 ? c->inputs_[0].size() + 1 : 2);
  Iterator** list = new Iterator*[space];
  int num = 0;
  for (int which = 0; which < 2; which++) {
    if (!c->inputs_[which].empty()) {
      if (c->level() + which == 0) {
        const std::vector<FileMetaData*>& files = c->inputs_[which];
        for (size_t i = 0; i < files.size(); i++) {
          list[num++] = table_cache_->NewIterator(
              options, files[i]->number, files[i]->file_size);
        }
      } else {
        // Create concatenating iterator for the files from this level
        list[num++] = NewTwoLevelIterator(
            new Version::LevelFileNumIterator(icmp_, &c->inputs_[which]),
            &GetFileIterator, table_cache_, options);
      }
    }
  }
  assert(num <= space);
  Iterator* result = NewMergingIterator(&icmp_, list, num);
  delete[] list;
  return result;
}

Compaction* BasicVersionSet::PickCompaction() {
  Compaction* c;
  int level;
  //TODO pick compaction error
  // We prefer compactions triggered by too much data in a level over
  // the compactions triggered by seeks.
  const bool size_compaction = (current_->compaction_score_ > runtime::compaction_min_score)||current_->compaction_type_==CompactionType::INTERNAL_ROLLING_MERGE;
  if (size_compaction) {
    level = current_->compaction_level_;

    assert(level >= 0);
    assert(level < config::kNumLevels);
    c = new Compaction(level,current_->compaction_type_);
    //no input is needed for level move
    if(c->type_ == LEVEL_MOVE)
    {
        fprintf(stderr,"level is %d, action is move, score is %f\n",level,current_->compaction_score_);
    	c->input_version_ = current_;
    	c->input_version_->Ref();
    	return c;
    }

    //for level 0, pick all files, and merge it to the deletion part of level 1
    //pick the first file of the deletion part for other levels
	assert(current_->levels_[level][DELETION_PART]->files_.size()>0);//must be true for rolling merge
    if(level == 0){
    	for(int i=0;i<current_->levels_[0][DELETION_PART]->files_.size();i++){

        	c->inputs_[0].push_back(current_->levels_[0][DELETION_PART]->files_[i]);
    	}
    }else{
    	int size = std::min(int(current_->levels_[level][DELETION_PART]->files_.size()),config::files_merged_each_round);
    	for(int i=0;i<size;i++){
            c->inputs_[0].push_back(current_->levels_[level][DELETION_PART]->files_[i]);
    	}
    }

  } else {
    return NULL;
  }

  c->input_version_ = current_;
  c->input_version_->Ref();

  SetupOtherInputs(c);

  //for internal rolling merge, tracking the number of hot files be disturbed in this round of internal rolling merge
  for(int i=0;i<c->inputs_[1].size();i++){
	  FileMetaData *f = c->inputs_[1][i];
	  Table *t;
	  this->table_cache_->GetTable(f->number,f->file_size,&t);
	  if(t->isHot()){
		  this->num_hot_files_deleted_[level]++;
		  printf("level: %d file: %d visited: %ld\n",level,i, t->GetVistedNum());
	  }

  }
  fprintf(stderr,"level is %d, action is merge, score is %f %ld+%ld \n",level,current_->compaction_score_,c->inputs_[0].size(),c->inputs_[1].size());
  return c;
}

void BasicVersionSet::SetupOtherInputs(Compaction* c) {

  const int level = c->level();
  InternalKey smallest, largest;
  GetRange(c->inputs_[0], &smallest, &largest);

  current_->GetOverlappingInputs(INSERTION_PART,level, &smallest, &largest, &c->inputs_[1]);

  // Get entire range covered by compaction
  InternalKey all_start, all_limit;
  GetRange2(c->inputs_[0], c->inputs_[1], &all_start, &all_limit);

  // See if we can grow the number of inputs in "level" without
  // changing the number of "level+1" files we pick up.
  if (!c->inputs_[1].empty()) {
    std::vector<FileMetaData*> expanded0;
    current_->GetOverlappingInputs(DELETION_PART, level, &all_start, &all_limit, &expanded0);
    const int64_t inputs0_size = TotalFileSize(c->inputs_[0]);
    const int64_t inputs1_size = TotalFileSize(c->inputs_[1]);
    const int64_t expanded0_size = TotalFileSize(expanded0);
    //deletion part is expanded
    if (expanded0.size() > c->inputs_[0].size() &&
        inputs1_size + expanded0_size < kExpandedCompactionByteSizeLimit) {
      InternalKey new_start, new_limit;
      GetRange(expanded0, &new_start, &new_limit);
      std::vector<FileMetaData*> expanded1;
      current_->GetOverlappingInputs(INSERTION_PART,level, &new_start, &new_limit,
                                     &expanded1);
      //but the insertion part is not, then we can get one more file from deletion part
      if (expanded1.size() == c->inputs_[1].size()) {
        Log(options_->info_log,
            "Expanding@%d %d+%d (%ld+%ld bytes) to %d+%d (%ld+%ld bytes)\n",
            level,
            int(c->inputs_[0].size()),
            int(c->inputs_[1].size()),
            long(inputs0_size), long(inputs1_size),
            int(expanded0.size()),
            int(expanded1.size()),
            long(expanded0_size), long(inputs1_size));
        smallest = new_start;
        largest = new_limit;
        c->inputs_[0] = expanded0;
        c->inputs_[1] = expanded1;
        GetRange2(c->inputs_[0], c->inputs_[1], &all_start, &all_limit);
      }
    }
  }

  if (false) {
    Log(options_->info_log, "Compacting %d '%s' .. '%s'",
        level,
        smallest.DebugString().c_str(),
        largest.DebugString().c_str());
  }

}

//teng: function to move the insertion part to next level as its deletion part
Status BasicVersionSet::MoveLevelDown(int level, port::Mutex *mutex_) {

	assert(level+1<config::kNumLevels);
    //assert(current()->files_[level+1].size() == 0);
	//for some reason the deletion part is not empty(caused by unfinished compaction, shutdown, etc.), clear it first
    if(current()->levels_[level+1][DELETION_PART]->files_.size() != 0){
    	this->ClearLevel(level+1,mutex_);
    }

    //leveldb::FileMetaData* const* files = &this->current()->files_[level][0];
   // size_t num_files = this->current()->files_[level].size();

	leveldb::FileMetaData* const* files = &this->current()->levels_[level][INSERTION_PART]->files_[0];
	size_t num_files = this->current()->levels_[level][INSERTION_PART]->files_.size();

    VersionEdit edit;
    //move from insertion part of this level, to the deletion part of next level.

    //TODO, operation on the compaction buffer
    for(int i = 0; i < num_files; i++) {
    	leveldb::FileMetaData* f = files[i];
    	edit.DeleteFile(INSERTION_PART ,level, f->number);
    	edit.AddFile(DELETION_PART, level+1, f->number, f->file_size,
    	                       f->smallest, f->largest);
    	edit.AddFile(COMPACTION_BUFFER, level+1, f->number, f->file_size,f->smallest, f->largest);
    }
    edit.EnableRefineCB(true,level+1);
    leveldb::Status status = this->LogAndApply(&edit, mutex_);
    return status;
}


//clear the deletion part for the level move from the insertion part of the previous level
Status BasicVersionSet::ClearLevel(int level, port::Mutex *mutex_) {
	assert(level<config::kNumLevels);

    //leveldb::FileMetaData* const* files = &this->current()->files_[level][0];
    //size_t num_files = this->current()->files_[level].size();
	leveldb::FileMetaData* const* files = &this->current()->levels_[level][DELETION_PART]->files_[0];
	size_t num_files = this->current()->levels_[level][DELETION_PART]->files_.size();
    VersionEdit edit;
    for(int i = 0; i < num_files; i++) {
    	leveldb::FileMetaData* f = files[i];
    	edit.DeleteFile(DELETION_PART, level, f->number);
    }
    //TODO apply to deletion part
    leveldb::Status status = this->LogAndApply(&edit, mutex_);
    return status;
}

int BasicVersionSet::CompactionTargetLevel(int level){
	return level;
}

int BasicVersionSet::PhysicalStartLevel(int level){
	return level;
}
int BasicVersionSet::PhysicalEndLevel(int level){
	return level;
}

//teng: return the logical level one physical level belongs to
int BasicVersionSet::LogicalLevel(int plevel){
	return plevel;
}



Compaction::Compaction(int level,CompactionType type)
    : level_(level),
      max_output_file_size_(MaxFileSizeForLevel(level)),
      input_version_(NULL),
      type_(type){

}

Compaction::~Compaction() {
  if (input_version_ != NULL) {
    input_version_->Unref();
  }
}

bool Compaction::IsTrivialMove() const {
  // Avoid a move if there is lots of overlapping grandparent data.
  // Otherwise, the move could create a parent file that will require
  // a very expensive merge later on.
  return (num_input_files(0) == 1 &&
          num_input_files(1) == 0);
}


bool Compaction::IsLevelNeedsMove(){
   return type_==LEVEL_MOVE;
}


//this function only calls by internal rolling merge
void Compaction::AddInputDeletions(VersionEdit* edit,int level) {
  //delete the input of the insertion part
  for (size_t i = 0; i < inputs_[INSERTION_PART].size(); i++) {
	 edit->DeleteFile(INSERTION_PART, level, inputs_[INSERTION_PART][i]->number);
  }
  //delete the input of the deletion part
  for (size_t i = 0; i < inputs_[DELETION_PART].size(); i++) {
	 edit->DeleteFile(DELETION_PART, level, inputs_[DELETION_PART][i]->number);
  }
}


void Compaction::ReleaseInputs() {
  if (input_version_ != NULL) {
    input_version_->Unref();
    input_version_ = NULL;
  }
}

}  // namespace leveldb
