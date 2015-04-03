/*
 * lazy_version_set.h
 *
 *  Created on: Mar 21, 2015
 *      Author: teng
 */

#ifndef LAZY_VERSION_SET_H_
#define LAZY_VERSION_SET_H_

#include "db/version_set.h"

namespace leveldb{

class LazyVersionSet: public VersionSet {
   public:
	  LazyVersionSet(const std::string& dbname,
	             const Options* options,
	             TableCache* table_cache,
	             const InternalKeyComparator*
	             );
	  ~LazyVersionSet();
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


}


#endif /* LAZY_VERSION_SET_H_ */
