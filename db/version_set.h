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
#include "db/dbformat.h"
#include "db/version_edit.h"
#include "port/port.h"
#include "port/thread_annotations.h"
#include "dlsm_param.h"

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

extern bool SomeFileOverlapsRange(
    const InternalKeyComparator& icmp,
    bool disjoint_sorted_files,
    const std::vector<FileMetaData*>& files,
    const Slice* smallest_user_key,
    const Slice* largest_user_key);

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

// Returns true iff some file in "files" overlaps the user key range
// [*smallest,*largest].
// smallest==NULL represents a key smaller than all keys in the DB.
// largest==NULL represents a key largest than all keys in the DB.
// REQUIRES: If disjoint_sorted_files, files[] contains disjoint ranges
//           in sorted order.
extern bool SomeFileOverlapsRange(
    const InternalKeyComparator& icmp,
    bool disjoint_sorted_files,
    const std::vector<FileMetaData*>& files,
    const Slice* smallest_user_key,
    const Slice* largest_user_key);

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
  Status Get(const ReadOptions&, const LookupKey& key, std::string* val,
             GetStats* stats,const int endlevel=config::kNumLevels);

  //teng: get range
  int GetRange(const ReadOptions& options,
                        const LookupKey &start,
                        const LookupKey &end,
                        GetStats* stats);

  // Adds "stats" into the current state.  Returns true if a new
  // compaction may need to be triggered, false otherwise.
  // REQUIRES: lock is held
  bool UpdateStats(const GetStats& stats);

  // Record a sample of bytes read at the specified internal key.
  // Samples are taken approximately once every config::kReadBytesPeriod
  // bytes.  Returns true if a new compaction may need to be triggered.
  // REQUIRES: lock is held
  bool RecordReadSample(Slice key);

  // Reference count management (so Versions do not disappear out from
  // under live iterators)
  void Ref();
  void Unref();

  void GetOverlappingInputs(
      int level,
      const InternalKey* begin,         // NULL means before all keys
      const InternalKey* end,           // NULL means after all keys
      std::vector<FileMetaData*>* inputs);

  // Returns true iff some file in the specified level overlaps
  // some part of [*smallest_user_key,*largest_user_key].
  // smallest_user_key==NULL represents a key smaller than all keys in the DB.
  // largest_user_key==NULL represents a key largest than all keys in the DB.
  bool OverlapInLevel(int level,
                      const Slice* smallest_user_key,
                      const Slice* largest_user_key);

  // Return the level at which we should place a new memtable compaction
  // result that covers the range [smallest_user_key,largest_user_key].
  int PickLevelForMemTableOutput(const Slice& smallest_user_key,
                                 const Slice& largest_user_key);

  int NumFiles(int level) const { return files_[level].size(); }

  // Return a human readable string that describes this version's contents.
  std::string DebugString() const;

 private:
  friend class Compaction;
  friend class VersionSet;
  friend class LazyVersionSet;
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
  Iterator* NewConcatenatingIterator(const ReadOptions&, int level) const;

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
  std::vector<FileMetaData*> files_[config::kNumLevels];

  // Next file to compact based on seek stats.
  FileMetaData* file_to_compact_;
  int file_to_compact_level_;

  // Level that should be compacted next and its compaction score.
  // Score < 1 means compaction is not strictly needed.  These fields
  // are initialized by Finalize().
  double compaction_score_;
  int compaction_level_;


  explicit Version(VersionSet* vset, int level = config::kNumLevels)
      : vset_(vset), next_(this), prev_(this), refs_(0),
        file_to_compact_(NULL),
        level_num_(level),
        file_to_compact_level_(-1),
        compaction_score_(-1),
        compaction_level_(-1) {
  }

  ~Version();

  // No copying allowed
  Version(const Version&);
  void operator=(const Version&);
  void printVersion();
};

class VersionSet {
 public:
  VersionSet(const std::string& dbname,
             const Options* options,
             TableCache* table_cache,
             const InternalKeyComparator*
             );
  virtual ~VersionSet();

  void FilesCoveredInLevel(int level, const InternalKey smallest, const InternalKey largest,std::vector<FileMetaData *>* results);

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
  void MarkFileNumberUsed(uint64_t number);

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

  // Return a compaction object for compacting the range [begin,end] in
  // the specified level.  Returns NULL if there is nothing in that
  // level that overlaps the specified range.  Caller should delete
  // the result.
  Compaction* CompactRange(
      int level,
      const InternalKey* begin,
      const InternalKey* end);

  // Return the maximum overlapping data (in bytes) at next level for any
  // file at a level >= 1.
  int64_t MaxNextLevelOverlappingBytes();

  // Create an iterator that reads over the compaction inputs for "*c".
  // The caller should delete the iterator when no longer needed.
  virtual Iterator* MakeInputIterator(Compaction* c)=0;

  // Returns true iff some level needs a compaction.
  bool NeedsCompaction() const {
    Version* v = current_;
    return (v->compaction_score_ >= 1) || (v->file_to_compact_ != NULL);
  }

  // Add all files listed in any live version to *live.
  // May also mutate some internal state.
  void AddLiveFiles(std::set<uint64_t>* live);

  // Return the approximate offset in the database of the data for
  // "key" as of version "v".
  uint64_t ApproximateOffsetOf(Version* v, const InternalKey& key);

  // Return a human-readable short (single-line) summary of the number
  // of files per level.  Uses *scratch as backing store.
  struct LevelSummaryStorage {
    char buffer[100];
  };
  const char* LevelSummary(LevelSummaryStorage* scratch) const;

  //teng: for two phase compaction
  virtual Status MoveLevelDown(int level, leveldb::port::Mutex *mutex_)=0;
  virtual int CompactionTargetLevel(int level)=0;
  virtual int PhysicalStartLevel(int level) = 0;
  virtual int PhysicalEndLevel(int level) = 0;
  virtual int LogicalLevel(int level) = 0;

  virtual void printCurVersion() = 0;

  virtual uint64_t TotalLevelSize(Version * v,int level)=0;
  double calculate_compaction_score(Version *, int level);

  void GetRange(const std::vector<FileMetaData*>& inputs,
                  InternalKey* smallest,
                  InternalKey* largest);

  void GetRange2(const std::vector<FileMetaData*>& inputs1,
                   const std::vector<FileMetaData*>& inputs2,
                   InternalKey* smallest,
                   InternalKey* largest);
  const std::string GetCompactionPointer(int level){
	  return this->compact_pointer_[level];
  }
  const void ResetPointer(int level){
	  int startlevel = this->PhysicalStartLevel(level);
	  int endlevel = this->PhysicalEndLevel(level);
	  for(int i=startlevel;i<=endlevel;i++){
		  this->compact_pointer_[i].clear();
		  assert(this->compact_pointer_[i].empty());
	  }
  }

 protected:
  class Builder;

  friend class Compaction;
  friend class Version;
  friend class LazyVersionSet;
  friend class BasicVersionSet;


  void Finalize(Version* v);


  virtual void SetupOtherInputs(Compaction* c)=0;

  // Save current contents to *log
  Status WriteSnapshot(log::Writer* log);

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


  //teng:
  WritableFile* descriptor_file_sm_;
  log::Writer* descriptor_log_sm_;
  // Opened lazily
  WritableFile* descriptor_file_;
  log::Writer* descriptor_log_;
  Version dummy_versions_;  // Head of circular doubly-linked list of versions.
  Version* current_;        // == dummy_versions_.prev_
  const InternalKeyComparator icmp_;

  // Per-level key at which the next compaction at that level should start.
  // Either an empty string, or a valid InternalKey.
  std::string compact_pointer_[config::kNumLevels];


  //teng: save current target level
  int targetPLevel[config::LogicalLevelnum];

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
	  uint64_t TotalLevelSize(Version *v, int level);
	  int NumLevelFiles(int level);
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

  // Add all inputs to this compaction as delete operations to *edit.
  void AddInputDeletions(VersionEdit* edit, int startlevel, int levelnum, int targetlevel);

  // Returns true if the information we have available guarantees that
  // the compaction is producing data in "level+1" for which no data exists
  // in levels greater than "level+1".
  bool IsBaseLevelForKey(const Slice& user_key);

  // Returns true iff we should stop building the current output
  // before processing "internal_key".
  bool ShouldStopBefore(const Slice& internal_key);

  // Release the input version for the compaction, once the compaction
  // is successful.
  void ReleaseInputs();

  //teng:  check if this level needs to be move down entirely
  bool IsLevelNeedsMove();

 private:
  friend class Version;
  friend class VersionSet;
  friend class LazyVersionSet;
  friend class BasicVersionSet;


  explicit Compaction(int level);

  int level_;
  uint64_t max_output_file_size_;
  Version* input_version_;
  VersionEdit edit_;

  // Each compaction reads inputs from "level_" and "level_+1"
  // for sm, this can be as large as levels_per_logical_level
  std::vector<FileMetaData*> inputs_[config::levels_per_logical_level+1];      // The two sets of inputs

  // State used to check for number of of overlapping grandparent files
  // (parent == level_ + 1, grandparent == level_ + 2)
  std::vector<FileMetaData*> grandparents_;
  size_t grandparent_index_;  // Index in grandparent_starts_
  bool seen_key_;             // Some output key has been seen
  int64_t overlapped_bytes_;  // Bytes of overlap between current output
                              // and grandparent files

  // State for implementing IsBaseLevelForKey

  // level_ptrs_ holds indices into input_version_->levels_: our state
  // is that we are positioned at one of the file ranges for each
  // higher level than the ones involved in this compaction (i.e. for
  // all L >= level_ + 2).
  size_t level_ptrs_[config::kNumLevels];
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_VERSION_SET_H_
