// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// The representation of a DBImpl consists of a set of Versions.  The
// newest version is called "current".  Older versions may be kept
// around to provide a consistent view to live iterators.
//
// Each Version keeps track of a set of Table files per level.  The
// entire set of versions is maintained in a VersionSet.
//
// Version,VersionSet are thread-compatible, but require external
// synchronization on all accesses.

#ifndef STORAGE_LEVELDB_DB_VERSION_SET_H_
#define STORAGE_LEVELDB_DB_VERSION_SET_H_

#include <map>
#include <set>
#include <vector>

#include "dbformat.h"
#include "lsbm/params.h"
#include "version_edit.h"
#include "port/port.h"
#include "port/thread_annotations.h"

#include <cmath>

#define digit_base 1024
namespace leveldb {

namespace log { class Writer; }

extern const int64_t kMaxGrandParentOverlapBytes;

extern const int64_t kExpandedCompactionByteSizeLimit;

extern uint64_t MaxBytesForLevel(int level);

extern uint64_t TotalFileSize(const std::vector<FileMetaData*>& files);

extern uint64_t MaxKBytesForLevel(int level);
extern uint64_t MaxMBytesForLevel(int level);

extern uint64_t TotalFileSizeKB(const std::vector<FileMetaData*>& files);
extern uint64_t TotalFileSizeMB(const std::vector<FileMetaData*>& files);

extern uint64_t MaxFileSizeForLevel(int level);

extern bool AfterFile(const Comparator* ucmp,
                      const Slice* user_key, const FileMetaData* f);

extern bool BeforeFile(const Comparator* ucmp,
                       const Slice* user_key, const FileMetaData* f);


class Compaction;
class Iterator;
class MemTable;
class TableBuilder;
class TableCache;
class Version;
class VersionSet;
class WritableFile;

// Return the smallest index i such that files[i]->largest >= key.
// Return files.size() if there is no such file.
// REQUIRES: "files" contains a sorted list of non-overlapping files.
extern int FindFile(const InternalKeyComparator& icmp,
                    const std::vector<FileMetaData*>& files,
                    const Slice& key);



class SortedTable{

public:
	SortedTable(){
		next = NULL;
		prev = NULL;
		secondchance = true;
	}
	void AddFile(FileMetaData *file){
		if(file!=NULL){
			file->refs++;
			files_.push_back(file);
		}
	}
	bool secondchance;
	SortedTable *next;
	SortedTable *prev;
	std::vector<FileMetaData*> files_;
	std::vector<bool> visible_;
	//unref all files
	~SortedTable(){
		next = NULL;
		prev = NULL;
		for (size_t i = 0; i < files_.size(); i++) {
		      FileMetaData* f = files_[i];
		      assert(f->refs > 0);
		      f->refs--;
		      if (f->refs <= 0) {
		        delete f;
		      }
		 }

	}

	uint64_t NumVisibleFiles(){
		uint64_t count = 0;
		for(int i=0;i<files_.size();i++){
			count += files_[i]->visible;
		}
		return count;
	}

	uint64_t SizeofVisibleFiles(){
		uint64_t size = 0;
		for(int i=0;i<files_.size();i++){
			size += files_[i]->visible*files_[i]->file_size;
		}
		return size;
	}

};

enum CompactionType{
	LEVEL_MOVE = 0,
	INTERNAL_ROLLING_MERGE = 1
};
class Version {
 public:

  // Append to *iters a sequence of iterators that will
  // yield the contents of this Version when merged together.
  // REQUIRES: This version has been saved (see VersionSet::SaveTo)
  void AddIterators(const ReadOptions&, std::vector<Iterator*>* iters);

  // Lookup the value for key.  If found, store it in *val and
  // return OK.  Else return a non-OK status.  Fills *stats.
  // REQUIRES: lock is not held
  struct GetStats {
    FileMetaData* seek_file;
    int seek_file_level;
  };
  Status searchFile(const ReadOptions& options,FileMetaData *f, Slice user_key, std::string *value);
  Status Get(const ReadOptions&, const LookupKey& key, std::string* val);
  Status Get2(const ReadOptions&, const LookupKey& key, std::string* val);

  //teng: range query
  int RangeQuery(const ReadOptions& options,
                        const LookupKey &start,
                        const LookupKey &end);

  // Reference count management (so Versions do not disappear out from
  // under live iterators)
  void Ref();
  void Unref();

  void NewSortedTable(int level, SortedTableType type);
  void GetOverlappingInputs(
	  int type,
      int level,
      const InternalKey* begin,         // NULL means before all keys
      const InternalKey* end,           // NULL means after all keys
      std::vector<FileMetaData*>* inputs);


  int NumFiles(int level) const {
	  return NumPartFiles(level,INSERTION_PART)+NumPartFiles(level,DELETION_PART);
  }

  int NumPartFiles(int level, SortedTableType type) const{
    SortedTable *head = levels_[level][type];
    SortedTable *cur = head;
    int filesize = 0;
    do{
      if(type==SortedTableType::COMPACTION_BUFFER){
        filesize += cur->NumVisibleFiles();
      }else{
        filesize += cur->files_.size();
      }
      cur = cur->next;
    }while(cur!=head);
    return filesize;
  }
  uint64_t TotalLevelSize(int level);
  uint64_t TotalPartSize(int level, SortedTableType type);
  int AvgCachedBlock(const std::vector<FileMetaData*>& files, bool test_visible);
  // Return a human readable string that describes this version's contents.
  std::string DebugString() const;
  void RefineCompactionBuffer(const int level);
//  bool MaybeEvictTail(const int level){
//	  if(level==config::kNumLevels-1){
//		  return false;
//	  }
//	  //if(this->levels_[level+1][DELETION_PART]->next->files_.size()==0)
//	  {
//		  SortedTable *head = this->levels_[level][COMPACTION_BUFFER];
//		  SortedTable *cur = head->prev;
//		  bool evicted = false;
//		  while(cur != head){
//			  if(!cur->redundant){
//				  return false;
//			  }else if(!cur->obsolete){
//				  cur->obsolete = true;
//				  return true;
//			  }
//			  cur = cur->prev;
//		  }
//		  return evicted;
//	  }
//
//	  return false;
//  }

//  bool EvictTail(const int level){
//  	  if(level==config::kNumLevels-1){
//  		  return false;
//  	  }
//  	  if(true){
//  		  SortedTable *head = this->levels_[level][COMPACTION_BUFFER];
//  		  SortedTable *cur = head->prev;
//  		  bool evicted = false;
//  		  while(cur != head){
//  			  if(!cur->redundant){
//  				  return evicted;
//  			  }else if(!cur->obsolete){
//  				  cur->obsolete = true;
//  				  evicted = true;
//  			  }
//  			  cur = cur->prev;
//  		  }
//  		  return evicted;
//  	  }
//
//  }
//  void MarkTailredundant(const int level){
//  	  SortedTable *head = this->levels_[level][COMPACTION_BUFFER];
//  	  SortedTable *cur = head->prev;
//
//  	  while(cur!=head){
//  		  if(!cur->obsolete)
//  		  {
//  			  cur->redundant = true;
//  			  break;
//  		  }
//  		  cur = cur->prev;
//  	  }
//    }
//  void MarkAllredundant(const int level){
//	  SortedTable *head = this->levels_[level][COMPACTION_BUFFER];
//	  SortedTable *cur = head->prev;
//	  while(cur!=head){
//		  if(cur->redundant){
//			  cur->obsolete = true;
//		  }
//		  cur->redundant = true;
//		  cur = cur->prev;
//	  }
//  }
//
//  void MarkAllobsolete(const int level){
//	  SortedTable *head = this->levels_[level][COMPACTION_BUFFER];
//	  SortedTable *cur = head->prev;
//	  while(cur!=head){
//		  cur->obsolete = true;
//		  cur->redundant = true;
//		  cur = cur->prev;
//	  }
//  }



 private:
  friend class Compaction;
  friend class VersionSet;
  friend class BasicVersionSet;

  // An internal iterator.  For a given version/level pair, yields
  // information about the files in the level.  For a given entry, key()
  // is the largest key that occurs in the file, and value() is an
  // 16-byte value containing the file number and file size, both
  // encoded using EncodeFixed64.
  class LevelFileNumIterator : public Iterator {
   public:
    LevelFileNumIterator(const InternalKeyComparator& icmp,
                         const std::vector<FileMetaData*>* flist)
        : icmp_(icmp),
          flist_(flist),
          index_(flist->size()) {        // Marks as invalid
    }
    virtual bool Valid() const {
      return index_ < flist_->size();
    }
    virtual void Seek(const Slice& target) {
      index_ = FindFile(icmp_, *flist_, target);
    }
    virtual void SeekToFirst() { index_ = 0; }
    virtual void SeekToLast() {
      index_ = flist_->empty() ? 0 : flist_->size() - 1;
    }
    virtual void Next() {
      assert(Valid());
      index_++;
    }
    virtual void Prev() {
      assert(Valid());
      if (index_ == 0) {
        index_ = flist_->size();  // Marks as invalid
      } else {
        index_--;
      }
    }
    Slice key() const {
      assert(Valid());
      return (*flist_)[index_]->largest.Encode();
    }
    Slice value() const {
      assert(Valid());
      EncodeFixed64(value_buf_, (*flist_)[index_]->number);
      EncodeFixed64(value_buf_+8, (*flist_)[index_]->file_size);
      return Slice(value_buf_, sizeof(value_buf_));
    }
    virtual Status status() const { return Status::OK(); }
   private:
    const InternalKeyComparator icmp_;
    const std::vector<FileMetaData*>* const flist_;
    uint32_t index_;

    // Backing store for value().  Holds the file number and size.
    mutable char value_buf_[16];
  };
  Iterator* NewConcatenatingIterator(const ReadOptions&, int type, int level) const;

  // Call func(arg, level, f) for every file that overlaps user_key in
  // order from newest to oldest.  If an invocation of func returns
  // false, makes no more calls.
  //
  // REQUIRES: user portion of internal_key == user_key.
  void ForEachOverlapping(Slice user_key, Slice internal_key,
                          void* arg,
                          bool (*func)(void*, int, FileMetaData*));

  VersionSet* vset_;            // VersionSet to which this Version belongs
  Version* next_;               // Next version in linked list
  Version* prev_;               // Previous version in linked list
  int refs_;                    // Number of live refs to this version
  int level_num_;

  // List of files per level
  //std::vector<FileMetaData*> files_[config::kNumLevels];

  //three sorted table list, one for deletion part, one for insertion part, and one for compaction buffer
  SortedTable* levels_[config::kNumLevels][4];


  // Level that should be compacted next and its compaction score.
  // Score < 1 means compaction is not strictly needed.  These fields
  // are initialized by Finalize().
  double compaction_score_;
  int compaction_level_;
  CompactionType compaction_type_;

  explicit Version(VersionSet* vset, int level=config::kNumLevels);



  ~Version();

  // No copying allowed
  Version(const Version&);
  void operator=(const Version&);
  void printVersion();
  void clearPart(int level, SortedTableType type);
};

class VersionSet {
 public:
  VersionSet(const std::string& dbname,
             const Options* options,
             TableCache* table_cache,
             const InternalKeyComparator*
             );
  virtual ~VersionSet();

  // Apply *edit to the current version to form a new descriptor that
  // is both saved to persistent state and installed as the new
  // current version.  Will release *mu while actually writing to the file.
  // REQUIRES: *mu is held on entry.
  // REQUIRES: no other thread concurrently calls LogAndApply()
  virtual Status LogAndApply(VersionEdit* edit, port::Mutex* mu)=0
      EXCLUSIVE_LOCKS_REQUIRED(mu);

  // Recover the last saved descriptor from persistent storage.
  virtual Status Recover()=0;


  // Return the current version.
  Version* current() const { return current_; }

  // Return the current manifest file number
  uint64_t ManifestFileNumber() const { return manifest_file_number_; }

  // Allocate and return a new file number
  uint64_t NewFileNumber() { return next_file_number_++; }

  // Arrange to reuse "file_number" unless a newer file number has
  // already been allocated.
  // REQUIRES: "file_number" was returned by a call to NewFileNumber().
  void ReuseFileNumber(uint64_t file_number) {
    if (next_file_number_ == file_number + 1) {
      next_file_number_ = file_number;
    }
  }

  // Return the number of Table files at the specified level.
  virtual int NumLevelFiles(int level)=0 ;
  virtual int NumPartFiles(int level,SortedTableType part) = 0;

  // Return the combined file size of all files at the specified level.
  int64_t NumLevelBytes(int level) const;

  // Return the last sequence number.
  uint64_t LastSequence() const { return last_sequence_; }

  // Set the last sequence number to s.
  void SetLastSequence(uint64_t s) {
    assert(s >= last_sequence_);
    last_sequence_ = s;
  }

  // Mark the specified file number as used.
  void MarkFileNumberUsed(uint64_t number){
	  if (next_file_number_ <= number) {
	    next_file_number_ = number + 1;
	  };
  }

  // Return the current log file number.
  uint64_t LogNumber() const { return log_number_; }

  // Return the log file number for the log file that is currently
  // being compacted, or zero if there is no such log file.
  uint64_t PrevLogNumber() const { return prev_log_number_; }

  // Pick level and inputs for a new compaction.
  // Returns NULL if there is no compaction to be done.
  // Otherwise returns a pointer to a heap-allocated object that
  // describes the compaction.  Caller should delete the result.
  virtual Compaction* PickCompaction()=0;


  // Create an iterator that reads over the compaction inputs for "*c".
  // The caller should delete the iterator when no longer needed.
  virtual Iterator* MakeInputIterator(Compaction* c)=0;

  // Returns true iff some level needs a compaction.
  bool NeedsCompaction() const {
    Version* v = current_;
    return (v->compaction_score_ > leveldb::runtime::compaction_min_score||v->compaction_type_==CompactionType::INTERNAL_ROLLING_MERGE);
  }

  // Add all files listed in any live version to *live.
  // May also mutate some internal state.
  void AddLiveFiles(std::set<uint64_t>* live) {

    for (Version* v = dummy_versions_.next_;
         v != &dummy_versions_;
         v = v->next_) {

  	for (int level = 0; level < config::kNumLevels; level++) {
  		for(int type=0;type<4;type++){

  			SortedTable *head = v->levels_[level][type];
  			SortedTable *cur = head;

  			do{//go through the entire list
  				const std::vector<FileMetaData*>& files = cur->files_;
  					for (size_t i = 0; i < files.size(); i++) {
  						live->insert(files[i]->number);
  				}
  				cur = cur->next;
  			}while(cur!=head);

  		}
  	}
    }
  }


  // Return a human-readable short (single-line) summary of the number
  // of files per level.  Uses *scratch as backing store.
  struct LevelSummaryStorage {
    char buffer[100];
  };

  //teng: for two phase compaction
  virtual Status MoveLevelDown(int level, leveldb::port::Mutex *mutex_)=0;
  virtual Status ClearLevel(int level, leveldb::port::Mutex *mutex_)=0;

  virtual void printCurVersion() = 0;

  virtual uint64_t TotalLevelSize(int level)=0;
  virtual uint64_t TotalPartSize(int level, SortedTableType type)=0;

  void GetRange(const std::vector<FileMetaData*>& inputs,
                  InternalKey* smallest,
                  InternalKey* largest);

  void GetRange2(const std::vector<FileMetaData*>& inputs1,
                   const std::vector<FileMetaData*>& inputs2,
                   InternalKey* smallest,
                   InternalKey* largest);
  uint64_t num_hot_files_deleted_[config::kNumLevels];
  bool second_chance_[config::kNumLevels];

 protected:
  class Builder;

  friend class Compaction;
  friend class Version;
  friend class LazyVersionSet;
  friend class BasicVersionSet;


  void Finalize(Version* v);


  virtual void SetupOtherInputs(Compaction* c)=0;

  // Save current contents to *log
  Status WriteSnapshot(Version *,log::Writer* log, VersionEdit *edit);

  void AppendVersion(Version* v);

  Env* const env_;
  const std::string dbname_;
  const Options* const options_;
  TableCache* const table_cache_;
  uint64_t next_file_number_;
  uint64_t manifest_file_number_;
  uint64_t last_sequence_;
  uint64_t log_number_;
  uint64_t prev_log_number_;  // 0 or backing store for memtable being compacted

  // Opened lazily
  WritableFile* descriptor_file_;
  log::Writer* descriptor_log_;
  Version dummy_versions_;  // Head of circular doubly-linked list of versions.
  Version* current_;        // == dummy_versions_.prev_
  const InternalKeyComparator icmp_;



  // No copying allowed
  VersionSet(const VersionSet&);
  void operator=(const VersionSet&);
};


class BasicVersionSet: public VersionSet {
   public:
	  BasicVersionSet(const std::string& dbname,
	             const Options* options,
	             TableCache* table_cache,
	             const InternalKeyComparator*
	             );
	  ~BasicVersionSet();
	  Status LogAndApply(VersionEdit* edit, port::Mutex* mu);
	  //teng: to calculate next available entry for sm levels
	  int CompactionTargetLevel(int level);
	  Status Recover();
	  Iterator* MakeInputIterator(Compaction* c);
	  Compaction* PickCompaction();
	  void SetupOtherInputs(Compaction* c);
	  Status MoveLevelDown(int level, port::Mutex *mutex_);
	  Status ClearLevel(int level, port::Mutex *mutex_);

	  uint64_t TotalLevelSize(int level){
		  return current_->TotalLevelSize(level);
	  }
	  uint64_t TotalPartSize(int level, SortedTableType type ){
		  return current_->TotalPartSize(level,type);
	  }
	  int NumLevelFiles(int level);
	  int NumPartFiles(int level,SortedTableType part);

	  class Builder;
	  int PhysicalStartLevel(int level);
	  int PhysicalEndLevel(int level);
	  int LogicalLevel(int level);
	  void printCurVersion();


   private:
	  friend class Compaction;
	  friend class Version;
	  friend class Builder;
	  friend class VersionSet;

};


// A Compaction encapsulates information about a compaction.
class Compaction {
 public:
  ~Compaction();

  // Return the level that is being compacted.  Inputs from "level"
  // and "level+1" will be merged to produce a set of "level+1" files.
  int level() const { return level_; }

  // Return the object that holds the edits to the descriptor done
  // by this compaction.
  VersionEdit* edit() { return &edit_; }

  // "which" must be either 0 or 1
  int num_input_files(int which) const { return inputs_[which].size(); }

  // Return the ith input file at "level()+which" ("which" must be 0 or 1).
  FileMetaData* input(int which, int i) const { return inputs_[which][i]; }

  // Maximum size of files to build during this compaction.
  uint64_t MaxOutputFileSize() const { return max_output_file_size_; }

  // Is this a trivial compaction that can be implemented by just
  // moving a single input file to the next level (no merging or splitting)
  // teng: also check two phase compaction
  bool IsTrivialMove() const;
  void IsTrivialMove(bool trivial){
	  istrivial = trivial;
  }

  // Add all inputs to this compaction as delete operations to *edit.
  void AddInputDeletions(VersionEdit* edit, int level);

  // Returns true if the information we have available guarantees that
  // the compaction is producing data in "level+1" for which no data exists
  // in levels greater than "level+1".
  bool IsBaseLevelForKey(const Slice& user_key);


  // Release the input version for the compaction, once the compaction
  // is successful.
  void ReleaseInputs();

  //teng:  check if this level needs to be move down entirely
  bool IsLevelNeedsMove();
  Compaction(int level, CompactionType type);

 private:
  friend class Version;
  friend class VersionSet;
  friend class LazyVersionSet;
  friend class BasicVersionSet;



  int level_;
  CompactionType type_;
  uint64_t max_output_file_size_;
  Version* input_version_;
  VersionEdit edit_;

  bool istrivial = true;

  // Each compaction reads inputs from "deletion part" and "insertion part"
  std::vector<FileMetaData*> inputs_[2];      // The two sets of inputs

};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_VERSION_SET_H_
