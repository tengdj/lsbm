// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "lazy_version_set.h"
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
#include "dlsm_param.h"

namespace leveldb {

// A helper class so we can efficiently apply a whole sequence
// of edits to a particular state without creating intermediate
// Versions that contain full copies of the intermediate state.
class LazyVersionSet::Builder {
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
    FileSet* added_files;
  };

  VersionSet* vset_;
  Version* base_;
  LevelState levels_[config::kNumLevels];

 public:
  // Initialize a builder with the files from *base and other info from *vset
  Builder(VersionSet* vset, Version* base)
      : vset_(vset),
        base_(base) {
    base_->Ref();
    BySmallestKey cmp;
    cmp.internal_comparator = &vset_->icmp_;
    for (int level = 0; level < config::kNumLevels; level++) {
      levels_[level].added_files = new FileSet(cmp);
    }
  }

  ~Builder() {
    for (int level = 0; level < config::kNumLevels; level++) {
      const FileSet* added = levels_[level].added_files;
      std::vector<FileMetaData*> to_unref;
      to_unref.reserve(added->size());
      for (FileSet::const_iterator it = added->begin();
          it != added->end(); ++it) {
        to_unref.push_back(*it);
      }
      delete added;
      for (uint32_t i = 0; i < to_unref.size(); i++) {
        FileMetaData* f = to_unref[i];
        f->refs--;
        if (f->refs <= 0) {
          delete f;
        }
      }
    }
    base_->Unref();
  }

  // Apply all of the edits in *edit to the current state.
  void Apply(VersionEdit* edit) {
    // Update compaction pointers
    for (size_t i = 0; i < edit->compact_pointers_.size(); i++) {
      const int level = edit->compact_pointers_[i].first;
      vset_->compact_pointer_[level] =
          edit->compact_pointers_[i].second.Encode().ToString();
    }
/*
    // Update target physical level
   for (size_t i = 0; i < edit->targetPLevel.size(); i++) {
       const int level = edit->targetPLevel[i].first;
       vset_->targetPLevel[level] =
       edit->targetPLevel[i].second;
    }
*/
    // Delete files
    const VersionEdit::DeletedFileSet& del = edit->deleted_files_;
    for (VersionEdit::DeletedFileSet::const_iterator iter = del.begin();
         iter != del.end();
         ++iter) {
      const int level = iter->first;
      const uint64_t number = iter->second;
      levels_[level].deleted_files.insert(number);
    }

    // Add new files
    for (size_t i = 0; i < edit->new_files_.size(); i++) {
      const int level = edit->new_files_[i].first;
      FileMetaData* f = new FileMetaData(edit->new_files_[i].second);
      f->refs = 1;

      // We arrange to automatically compact this file after
      // a certain number of seeks.  Let's assume:
      //   (1) One seek costs 10ms
      //   (2) Writing or reading 1MB costs 10ms (100MB/s)
      //   (3) A compaction of 1MB does 25MB of IO:
      //         1MB read from this level
      //         10-12MB read from next level (boundaries may be misaligned)
      //         10-12MB written to next level
      // This implies that 25 seeks cost the same as the compaction
      // of 1MB of data.  I.e., one seek costs approximately the
      // same as the compaction of 40KB of data.  We are a little
      // conservative and allow approximately one seek for every 16KB
      // of data before triggering a compaction.
      f->allowed_seeks = (f->file_size / 16384);
      if (f->allowed_seeks < 100) f->allowed_seeks = 100;

      levels_[level].deleted_files.erase(f->number);
      levels_[level].added_files->insert(f);
    }
  }

  // Save the current state in *v.
  void SaveTo(Version* v) {
    BySmallestKey cmp;
    cmp.internal_comparator = &vset_->icmp_;
    for (int level = 0; level < config::kNumLevels; level++) {
      // Merge the set of added files with the set of pre-existing files.
      // Drop any deleted files.  Store the result in *v.
      const std::vector<FileMetaData*>& base_files = base_->files_[level];
      std::vector<FileMetaData*>::const_iterator base_iter = base_files.begin();
      std::vector<FileMetaData*>::const_iterator base_end = base_files.end();
      const FileSet* added = levels_[level].added_files;
      v->files_[level].reserve(base_files.size() + added->size());
      for (FileSet::const_iterator added_iter = added->begin();
           added_iter != added->end();
           ++added_iter) {
        // Add all smaller files listed in base_
        for (std::vector<FileMetaData*>::const_iterator bpos
                 = std::upper_bound(base_iter, base_end, *added_iter, cmp);
             base_iter != bpos;
             ++base_iter) {
          MaybeAddFile(v, level, *base_iter);
        }

        MaybeAddFile(v, level, *added_iter);
      }

      // Add remaining base files
      for (; base_iter != base_end; ++base_iter) {
        MaybeAddFile(v, level, *base_iter);
      }

#ifndef NDEBUG
      // Make sure there is no overlap in levels > 0
      if (level > 0) {
        for (uint32_t i = 1; i < v->files_[level].size(); i++) {
          const InternalKey& prev_end = v->files_[level][i-1]->largest;
          const InternalKey& this_begin = v->files_[level][i]->smallest;
          if (vset_->icmp_.Compare(prev_end, this_begin) >= 0) {
            fprintf(stderr, "overlapping ranges in same level %s vs. %s\n",
                    prev_end.DebugString().c_str(),
                    this_begin.DebugString().c_str());
            abort();
          }
        }
      }
#endif
    }
  }

  void MaybeAddFile(Version* v, int level, FileMetaData* f) {
    if (levels_[level].deleted_files.count(f->number) > 0) {
      // File is deleted: do nothing
    } else {
      std::vector<FileMetaData*>* files = &v->files_[level];
      if (level > 0 && !files->empty()) {
        // Must not overlap
        assert(vset_->icmp_.Compare((*files)[files->size()-1]->largest,
                                    f->smallest) < 0);
      }
      f->refs++;
      files->push_back(f);
    }
  }
};


LazyVersionSet::LazyVersionSet(const std::string& dbname,
                       const Options* options,
                       TableCache* table_cache,
                       const InternalKeyComparator* cmp
                       )
    : VersionSet(dbname, options, table_cache, cmp)
      {
  AppendVersion(new Version(this));
  //teng: the target level for the SM's two phase compaction, initially the last level of the being compacted levels
  targetPLevel[0] = 0;
  targetPLevel[1] = config::levels_per_logical_level;
  targetPLevel[2] = config::levels_per_logical_level*2;
  for(int i=3;i<config::LogicalLevelnum;i++){
      targetPLevel[i] = i*config::levels_per_logical_level+1;
  }

}

LazyVersionSet::~LazyVersionSet() {
  current_->Unref();
  assert(dummy_versions_.next_ == &dummy_versions_);  // List must be empty
  delete descriptor_log_sm_;
  delete descriptor_file_sm_;
}

Status LazyVersionSet::LogAndApply(VersionEdit* edit, port::Mutex* mu) {
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
  }
  Finalize(v);
  //save target p level
  edit->targetPLevel.clear();
  for(int level=0;level<config::LogicalLevelnum;level++){
	edit->SetTargetPLevel(level,targetPLevel[level]);
  }
  // Initialize new descriptor log file if necessary by creating
  // a temporary file that contains a snapshot of the current version.
  std::string new_manifest_file;
  Status s;
  if (descriptor_log_sm_ == NULL) {
    // No reason to unlock *mu here since we only hit this path in the
    // first call to LogAndApply (when opening the database).
    assert(descriptor_file_sm_ == NULL);
    new_manifest_file = DescriptorFileName(dbname_, manifest_file_number_);
    //teng: for secondary version
    new_manifest_file = new_manifest_file+"_sm";

    edit->SetNextFile(next_file_number_);
    s = env_->NewWritableFile(new_manifest_file, &descriptor_file_sm_);
    if (s.ok()) {
      descriptor_log_sm_ = new log::Writer(descriptor_file_sm_);
      s = WriteSnapshot(descriptor_log_sm_);
    }
  }

  // Unlock during expensive MANIFEST log write
  {
    mu->Unlock();

    // Write new record to MANIFEST log
    if (s.ok()) {
      std::string record;
      edit->EncodeTo(&record);
      //printf("%s \n",record.c_str());
      s = descriptor_log_sm_->AddRecord(record);
      if (s.ok()) {
        s = descriptor_file_sm_->Sync();
      }
      if (!s.ok()) {
        Log(options_->info_log, "MANIFEST write: %s\n", s.ToString().c_str());
      }
    }

    // If we just created a new descriptor file, install it by writing a
    // new CURRENT file that points to it.
    if (s.ok() && !new_manifest_file.empty()) {
      s = SetCurrentSMFile(env_, dbname_, manifest_file_number_);
    }

    mu->Lock();
  }

  // Install the new version
  if (s.ok()) {
    AppendVersion(v);
    log_number_ = edit->log_number_;
    prev_log_number_ = edit->prev_log_number_;
  } else {
    delete v;
    if (!new_manifest_file.empty()) {
      delete descriptor_log_sm_;
      delete descriptor_file_sm_;
      descriptor_log_sm_ = NULL;
      descriptor_file_sm_ = NULL;
      env_->DeleteFile(new_manifest_file);
    }
  }

  printCurVersion();
  return s;
}

Status LazyVersionSet::Recover() {
  struct LogReporter : public log::Reader::Reporter {
    Status* status;
    virtual void Corruption(size_t bytes, const Status& s) {
      if (this->status->ok()) *this->status = s;
    }
  };

  // Read "CURRENT" file, which contains a pointer to the current manifest file
  std::string current;
  Status s = ReadFileToString(env_, CurrentFileName(dbname_)+"_sm", &current);
  if (!s.ok()) {
    return s;
  }
  if (current.empty() || current[current.size()-1] != '\n') {
    return Status::Corruption("CURRENT file does not end with newline");
  }
  current.resize(current.size() - 1);

  std::string dscname = dbname_ + "/" + current+"_sm";

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
        for(int i=0;i<edit.targetPLevel.size();i++){
        	this->targetPLevel[edit.targetPLevel[i].first] = edit.targetPLevel[i].second;
        	//printf("the target level for %d is %d\n",edit.targetPLevel[i].first,edit.targetPLevel[i].second);
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

//teng: modify latter for sm mode
Iterator* LazyVersionSet::MakeInputIterator(Compaction* c) {
  ReadOptions options;
  options.verify_checksums = options_->paranoid_checks;
  options.fill_cache = false;

  // Level-0 files have to be merged together.  For other levels,
  // we will make a concatenating iterator per level.
  // (opt): use concatenating iterator for level-0 if there is no overlap

  const int totallevels = config::levels_per_logical_level+1;
  const int space = (c->level() == 0 ? c->inputs_[0].size() + 1 : totallevels);
  Iterator** list = new Iterator*[space];
  int num = 0;
  for (int which = 0; which < totallevels; which++) {
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

Compaction* LazyVersionSet::PickCompaction() {
  Compaction* c;
  int level;
//TODO pick compaction error
  // We prefer compactions triggered by too much data in a level over
  // the compactions triggered by seeks.
  const bool size_compaction = (current_->compaction_score_ > leveldb::runtime::compaction_min_score);
  if (size_compaction) {
    level = current_->compaction_level_;

    assert(level >= 0);
    assert(level+1 < config::LogicalLevelnum);
    c = new Compaction(level);
    //for level 0, compaction in normal way
    if(level==0){
      // Pick the first file that comes after compact_pointer_[level]
      for (size_t i = 0; i < current_->files_[level].size(); i++) {
        FileMetaData* f = current_->files_[level][i];
        if (compact_pointer_[level].empty() ||
            icmp_.Compare(f->largest.Encode(), compact_pointer_[level]) > 0) {
          c->inputs_[0].push_back(f);
          break;
        }
      }
      if (c->inputs_[0].empty()) {
        // Wrap-around to the beginning of the key space
        c->inputs_[0].push_back(current_->files_[level][0]);
      }
    }
    else{// for SM, each level take off one for merge
      int startlevel = this->PhysicalStartLevel(level);
      for(int i=0;i<config::levels_per_logical_level;i++){
    	  if(current_->files_[i+startlevel].size()!=0){
    	    for(int j=0;j<current_->files_[i+startlevel].size();j++){
    	    	FileMetaData* f = current_->files_[i+startlevel][j];
    	    	if (compact_pointer_[level].empty() ||
    	    	      icmp_.Compare(f->largest.Encode(), compact_pointer_[level]) > 0) {
    	    	      c->inputs_[i].push_back(f);
    	    	      break;
    	    	}
    	    }
    	  }
      }
    }


  }else {
    return NULL;
  }

  c->input_version_ = current_;
  c->input_version_->Ref();

  // Files in level 0 may overlap each other, so pick up all overlapping ones
  if (level == 0) {
    InternalKey smallest, largest;
    GetRange(c->inputs_[0], &smallest, &largest);
    // Note that the next call will discard the file we placed in
    // c->inputs_[0] earlier and replace it with an overlapping set
    // which will include the picked file.
    current_->GetOverlappingInputs(0, &smallest, &largest, &c->inputs_[0]);
    assert(!c->inputs_[0].empty());
  }

  SetupOtherInputs(c);
  return c;
}
//TODO modify it for sm mode
void LazyVersionSet::SetupOtherInputs(Compaction* c) {

  const int level = c->level();
  InternalKey smallest, largest,localsmallest[config::levels_per_logical_level],locallargest[config::levels_per_logical_level];
  bool boundseted = false;
  for(int i=0;i<config::levels_per_logical_level;i++){
	  if(c->inputs_[i].size()==0){
		  continue;
	  }else{
		  if(!boundseted){
			  GetRange(c->inputs_[i], &smallest,&largest);
		  }
		  boundseted = true;
	  }

	  GetRange(c->inputs_[i], &localsmallest[i], &locallargest[i]);
	  if(icmp_.Compare(localsmallest[i],smallest)<0){
         smallest = localsmallest[i];
	  }
	  if(icmp_.Compare(locallargest[i],largest)>0){
          largest = locallargest[i];
	  }
  }


  current_->GetOverlappingInputs(CompactionTargetLevel(level+1), &smallest, &largest, &c->inputs_[config::levels_per_logical_level]);
  //TODO latter
/*
  // Get entire range covered by compaction
  InternalKey all_start, all_limit;
  std::vector<FileMetaData*> all;
  for(int i=0;i<config::levels_per_logical_level+1;i++){
      all.insert(all.end(), c->inputs_[i].begin(), c->inputs_[i].end());
  }
  GetRange(all, &all_start, &all_limit);
  // See if we can grow the number of inputs in "level" without
  // changing the number of "level+1" files we pick up.

  int startlevel = config::physicalStartLevel(level);
  if (!c->inputs_[config::levels_per_logical_level].empty()) {

	std::vector<FileMetaData*> expanded[config::levels_per_logical_level];
	std::vector<FileMetaData*> expanded1;
    const int64_t inputs1_size = TotalFileSize(c->inputs_[config::levels_per_logical_level]);
    int64_t expanded0_size = 0;
	for(int i=0;i<config::levels_per_logical_level;i++){

      current_->GetOverlappingInputs(startlevel+i, &all_start, &all_limit, &expanded[i]);
      expanded0_size += TotalFileSize(expanded[i]);
	}
	current_->GetOverlappingInputs(CompactionTargetLevel(level+1), &all_start, &all_limit, &expanded1);

	if (inputs1_size + expanded0_size < kExpandedCompactionByteSizeLimit) {
	         InternalKey new_start, new_limit;
	         GetRange(expanded0, &new_start, &new_limit);
	         std::vector<FileMetaData*> expanded1;
	         current_->GetOverlappingInputs(CompactionTargetLevel(level+1), &new_start, &new_limit,
	                                     &expanded1);
	      }
      if (expanded1.size() == c->inputs_[config::levels_per_logical_level].size()) {
        smallest = new_start;
        largest = new_limit;
        c->inputs_[i] = expanded0;
        GetRange2(c->inputs_[0], c->inputs_[1], &all_start, &all_limit);
      }
	}

  // Compute the set of grandparent files that overlap this compaction
  // (parent == level+1; grandparent == level+2)
  if (level + 2 < config::kNumLevels) {
    current_->GetOverlappingInputs(level + 2, &all_start, &all_limit,
                                   &c->grandparents_);
  }
  */

  if (false) {
    Log(options_->info_log, "Compacting %d '%s' .. '%s'",
        level,
        smallest.DebugString().c_str(),
        largest.DebugString().c_str());
  }

  // Update the place where we will do the next compaction for this level.
  // We update this immediately instead of waiting for the VersionEdit
  // to be applied so that if the compaction fails, we will try a different
  // key range next time.
  int firstplevel = this->PhysicalStartLevel(level);
  for(int i=firstplevel;i<=this->PhysicalEndLevel(level);i++){
	compact_pointer_[i] = locallargest[i-firstplevel].Encode().ToString();
	c->edit_.SetCompactPointer(i, locallargest[i-firstplevel]);
  }

}

//teng: function to move level down
//TODO modify to move sub levels for one level to another layer
Status LazyVersionSet::MoveLevelDown(int level, port::Mutex *mutex_) {

    int startlevel = this->PhysicalStartLevel(level);
    int endlevel = this->PhysicalEndLevel(level);
    VersionEdit edit;
    for(int curlevel = startlevel;curlevel<=endlevel;curlevel++){
    	leveldb::FileMetaData* const* files = &current()->files_[curlevel][0];
    	size_t num_files = current()->files_[curlevel].size();
    	for(int i = 0; i < num_files; i++) {
    		leveldb::FileMetaData* f = files[i];
    	  	edit.DeleteFile(curlevel, f->number);
    	  	if(level!=2){
    	   	    edit.AddFile(curlevel+config::levels_per_logical_level, f->number, f->file_size,
    	  	                       f->smallest, f->largest);
    	  	}
    	  	else{
    	  		edit.AddFile(targetPLevel[3], f->number, f->file_size,
    	  		    	  	      f->smallest, f->largest);
    	  	}
    	}
    }
    //reset current level targetPLevel to the last level plus one(it will be set to the last level latter)
    if(level>2){
    targetPLevel[level] = level*config::levels_per_logical_level+1;
    }
    //make room for this turn compaction
    if(level+2<config::LogicalLevelnum&&level!=2){
    	targetPLevel[level+2]--;
    }
    this->ResetPointer(level+1);
    leveldb::Status status = LogAndApply(&edit, mutex_);
    return status;
}

//teng: function to clear this level
Status LazyVersionSet::ClearLevel(int level, port::Mutex *mutex_) {

    int startlevel = this->PhysicalStartLevel(level);
    int endlevel = this->PhysicalEndLevel(level);
    VersionEdit edit;
    for(int curlevel = startlevel;curlevel<=endlevel;curlevel++){
    	leveldb::FileMetaData* const* files = &current()->files_[curlevel][0];
    	size_t num_files = current()->files_[curlevel].size();
    	for(int i = 0; i < num_files; i++) {
    		leveldb::FileMetaData* f = files[i];
    	  	edit.DeleteFile(curlevel, f->number);
    	}
    }
    //reset current level targetPLevel to the last level plus one(it will be set to the last level latter)
    if(level>2){
    	targetPLevel[level] = level*config::levels_per_logical_level+1;
    }
    //create a new recycle bin for next level
    if(level+2<config::LogicalLevelnum&&level!=2){
    	targetPLevel[level+2]--;
    }
    this->ResetPointer(level+1);
    leveldb::Status status = LogAndApply(&edit, mutex_);
    return status;
}

int LazyVersionSet::CompactionTargetLevel(int level){
   assert(level>0);
   return targetPLevel[level];
}

int LazyVersionSet::PhysicalStartLevel(int llevel){
	if(llevel == 0){
			return llevel;
	}else{
			return (llevel-1)*config::levels_per_logical_level+1;
	}
}
int LazyVersionSet::PhysicalEndLevel(int llevel){
	if(llevel == 0){
		return llevel;
	}else{
		return llevel*config::levels_per_logical_level;
	}
}

//teng: print all versions
void LazyVersionSet::printCurVersion(){
	  if(!leveldb::runtime::print_lazy_version_info){
		  return;
	  }
	  fprintf(stderr,"lazy------------------------------------------------------------------------\n");
	  int max = PhysicalEndLevel(runtime::max_print_level);
	  for(;max>=0;max--){
	  	    	if(current_->files_[max].size()!=0)
	  	    	{
	  	    		break;
	  	    	}
	  }

	  for(int i=0;i<=max;i++){
	      if(i%config::levels_per_logical_level==1)fprintf(stderr,"\n");
	      if(current_->files_[i].size()==0)continue;
	      	int llevel = this->LogicalLevel(i);
	      	/*if(runtime::two_phase_compaction&&llevel!=0){
	      		llevel = (llevel-1)/2+1;
	      	}*/
	      	fprintf(stderr,"plevel:%d   llevel:%d |",i,llevel);
	          for(int j=0;j<current_->files_[i].size();j++){
	        	  fprintf(stderr,"%ld ",current_->files_[i][j]->number);
	          }
	          fprintf(stderr,"\n");
	  }
}

//teng: return the logical level one physical level belongs to
int LazyVersionSet::LogicalLevel(int plevel){
	if(plevel == 0){
		return plevel;
	}else{
		return (plevel-1)/config::levels_per_logical_level+1;
	}
}

uint64_t LazyVersionSet::TotalLevelSize(Version *v, int level){
    assert(level<config::LogicalLevelnum);
	uint64_t sum = 0;
	for(int i=this->PhysicalStartLevel(level);i<=this->PhysicalEndLevel(level);i++){
		sum += TotalFileSizeMB(v->files_[i]);
	}
    return sum;
}

int LazyVersionSet::NumLevelFiles(int level){
	    assert(level >= 0);
	    assert(level < config::LogicalLevelnum);
	    int startlevel = this->PhysicalStartLevel(level);
	    int endlevel = this->PhysicalEndLevel(level);
	    int size = 0;
	    for(int i=startlevel;i<=endlevel;i++){
	    	size += current_->files_[i].size();
	    }
	    return current_->files_[level].size();
}

}  // namespace leveldb
