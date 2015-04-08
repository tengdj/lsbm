// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctime>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>

#include "db/db_impl.h"
#include "db/version_set.h"
#include "leveldb/cache.h"
#include "leveldb/db.h"
#include "leveldb/env.h"
#include "leveldb/write_batch.h"
#include "port/port.h"
#include "util/crc32c.h"
#include "util/histogram.h"
#include "util/mutexlock.h"
#include "util/random.h"
#include "util/testutil.h"
#include "util/ssd_cache.h"
#include "generator.h"
#include "dlsm_param.h"

// Comma-separated list of operations to run in the specified order
//   Actual benchmarks:
//      fillseq       -- write N values in sequential key order in async mode
//      fillrandom    -- write N values in random key order in async mode
//      overwrite     -- overwrite N values in random key order in async mode
//      fillsync      -- write N/100 values in random key order in sync mode
//      fill100K      -- write N/1000 100K values in random order in async mode
//      deleteseq     -- delete N keys in sequential order
//      deleterandom  -- delete N keys in random order
//      readseq       -- read N times sequentially
//      readreverse   -- read N times in reverse order
//      readrandom    -- read N times in random order
//      readmissing   -- read N missing keys in random order
//      readhot       -- read N times in random order from 1% section of DB
//      seekrandom    -- N random seeks
//      crc32c        -- repeated crc32c of 4K of data
//      acquireload   -- load N*1000 times
//   Meta operations:
//      compact     -- Compact the entire DB
//      stats       -- Print DB stats
//      sstables    -- Print sstable info
//      heapprofile -- Dump a heap profile (if supported by this port)
static const char* FLAGS_benchmarks =
    "fillseq,"
    "fillsync,"
    "fillrandom,"
    "overwrite,"
    "readrandom,"
    "readrandom,"  // Extra run to allow previous compactions to quiesce
    "readseq,"
    "readreverse,"
    "compact,"
    "readrandom,"
    "readseq,"
    "readreverse,"
    "fill100K,"
    "crc32c,"
    "snappycomp,"
    "snappyuncomp,"
    "acquireload,"
    ;

// Number of read operations to do.  If negative, infinite.
static int FLAGS_reads = -1;

// Number of writes operations to do.  If negative, infinite.
static int FLAGS_writes = -1;

// Number of concurrent threads to run.
static int FLAGS_threads = 1;

// Size of each value
static int FLAGS_value_size = 100;

// Arrange to generate values that shrink to this fraction of
// their original size after compression
static double FLAGS_compression_ratio = 0.5;

// Print histogram of operation timings
static bool FLAGS_histogram = false;

// Number of bytes to buffer in memtable before compacting
// (initialized to default value by "main")
static int FLAGS_write_buffer_size = 0;

// Number of bytes to use as a cache of uncompressed data.
// Negative means use default settings.
static int FLAGS_cache_size = -1;

//teng: parameters for ssd cache
static const char * FLAGS_ssd_cache_path = NULL;
//teng: ssd cache size in bytes
size_t FLAGS_ssd_cache_size = -1;

// Maximum number of files to keep open at the same time (use default if == 0)
static int FLAGS_open_files = 0;

// Bloom filter bits per key.
// Negative means use default settings.
static int FLAGS_bloom_bits = -1;

// If true, do not destroy the existing database.  If you set this
// flag and also specify a benchmark that wants a fresh database, that
// benchmark will fail.
static bool FLAGS_use_existing_db = false;

// Use the db with the following name.
static const char* FLAGS_db = NULL;

/************* Extened Flags *****************/

//key range of requests in r/w benchmark
static int64_t FLAGS_write_from = 0;
static int64_t FLAGS_write_upto = -1;
static int64_t FLAGS_write_span = -1;

static int64_t FLAGS_read_from = 0;
static int64_t FLAGS_read_upto = -1;
static int64_t FLAGS_read_span = -1;

/*uniform zipfian latest*/
static char *FLAGS_ycsb_workload = NULL;

static double FLAGS_range_size = 0.001;

static leveldb::port::Mutex range_query_mu_;
static long long range_count_ = 0;
static int range_total_ = 0;

static int64_t FLAGS_write_throughput = 0;
static int64_t FLAGS_read_throughput = 0;
static int64_t write_latency = 0;
static int64_t read_latency = 0;

static int FLAGS_range_threads = 0;
static long long FLAGS_range_query_number = 10000000000;//max number of range query


//end of teng's parameters

//below are the parameters used by teng for testing
long long llmax_ = 9223372036854775807ll;

static double FLAGS_countdown = -1;

static double rwrandom_wspeed = 0;

static int FLAGS_random_seed = 301;

static volatile int rwrandom_read_completed = 0;
static volatile int rwrandom_write_completed = 0;

static int monitor_interval = -1; //microseconds
static bool first_monitor_interval = true;
static FILE* monitor_log = stdout;

static leveldb::Histogram intv_read_hist_;
static leveldb::Histogram intv_write_hist_;
static double intv_start_;
static leveldb::port::Mutex intv_mu_;
static leveldb::port::Mutex rwrandom_read_mu_;


/************* Extened Flags (END) *****************/

namespace leveldb {

namespace {

// Helper for quickly generating random data.
class RandomGenerator {
 private:
  std::string data_;
  int pos_;

 public:
  RandomGenerator() {
    // We use a limited amount of data over and over again and ensure
    // that it is larger than the compression window (32KB), and also
    // large enough to serve all typical value sizes we want to write.
    Random rnd(301);
    std::string piece;
    while (data_.size() < 1048576) {
      // Add a short fragment that is as compressible as specified
      // by FLAGS_compression_ratio.
      test::CompressibleString(&rnd, FLAGS_compression_ratio, 100, &piece);
      data_.append(piece);
    }
    pos_ = 0;
  }

  Slice Generate(size_t len) {
    if (pos_ + len > data_.size()) {
      pos_ = 0;
      assert(len < data_.size());
    }
    pos_ += len;
    return Slice(data_.data() + pos_ - len, len);
  }
};

static Slice TrimSpace(Slice s) {
  size_t start = 0;
  while (start < s.size() && isspace(s[start])) {
    start++;
  }
  size_t limit = s.size();
  while (limit > start && isspace(s[limit-1])) {
    limit--;
  }
  return Slice(s.data() + start, limit - start);
}

static void AppendWithSpace(std::string* str, Slice msg) {
  if (msg.empty()) return;
  if (!str->empty()) {
    str->push_back(' ');
  }
  str->append(msg.data(), msg.size());
}

class Stats {
 private:
  double start_;
  double finish_;
  double seconds_;
  int done_;
  int read_done_;
  int write_done_;
  int next_report_;
  int64_t bytes_;
  double last_op_finish_;
  Histogram hist_;
  Histogram read_hist_;
  Histogram write_hist_;
  std::string message_;
  double intv_end_;

 public:
	int tid_;
	int pid_;
  Stats() { Start(); }

  void Start() {
    next_report_ = 100;
    last_op_finish_ = start_;
    hist_.Clear();
    read_hist_.Clear();
    write_hist_.Clear();
    done_ = 0;
    read_done_ = 0;
    write_done_ = 0;
    bytes_ = 0;
    seconds_ = 0;
    start_ = Env::Default()->NowMicros();
    finish_ = start_;
    message_.clear();
  }

  void Merge(const Stats& other) {
    hist_.Merge(other.hist_);
    read_hist_.Merge(other.read_hist_);
    write_hist_.Merge(other.write_hist_);
    done_ += other.done_;
    read_done_ += other.read_done_;
    write_done_ += other.write_done_;
    bytes_ += other.bytes_;
    seconds_ += other.seconds_;
    if (other.start_ < start_) start_ = other.start_;
    if (other.finish_ > finish_) finish_ = other.finish_;

    // Just keep the messages from one thread
    if (message_.empty()) message_ = other.message_;
  }

  void Stop() {
    finish_ = Env::Default()->NowMicros();
    seconds_ = (finish_ - start_) * 1e-6;
  }

  void AddMessage(Slice msg) {
    AppendWithSpace(&message_, msg);
  }

  void FinishedReadOp() {
    if (monitor_interval != -1) {
      double now = Env::Default()->NowMicros();
      double micros = now - last_op_finish_;
      read_hist_.Add(micros);
      intv_read_hist_.AtomicAdd(micros);	
    }
    read_done_++;

    FinishedSingleOp(1);
  }

  void FinishedWriteOp() {
    if (monitor_interval != -1) {
      double now = Env::Default()->NowMicros();
      double micros = now - last_op_finish_;
      write_hist_.Add(micros);
      intv_write_hist_.AtomicAdd(micros); 
    }
    write_done_++;

    FinishedSingleOp(2);
  }

  void FinishedSingleOp(int rw) {
    if (FLAGS_histogram) {
      double now = Env::Default()->NowMicros();
      double micros = now - last_op_finish_;
      hist_.Add(micros);
     /* if (micros > 20000) {
        fprintf(stderr, "long op: %.1f micros%30s\r", micros, "");
        fflush(stderr);
      }*/
      last_op_finish_ = now;
    }

    done_++;
    if (done_ >= next_report_) {/*
      if      (next_report_ < 1000)   next_report_ += 100;
      else if (next_report_ < 5000)   next_report_ += 500;
      else if (next_report_ < 10000)  next_report_ += 1000;
      else if (next_report_ < 50000)  next_report_ += 5000;
      else if (next_report_ < 100000) next_report_ += 10000;
      else if (next_report_ < 500000) next_report_ += 50000;
      else                            next_report_ += 100000;*/
      next_report_ += 1000;
      fprintf(stderr, "... finished %d %s\r", done_, rw==1?"reads":"writes");
      fflush(stderr);
    }

    intv_end_ = Env::Default()->NowMicros();
    if (monitor_interval != -1 && intv_end_ - intv_start_ > monitor_interval) {
    	intv_mu_.Lock();
    	if (intv_end_ - intv_start_ > monitor_interval) {
    		if (first_monitor_interval) {
    			fprintf(monitor_log, "\nPID\tTID\tRL\tWL\tRD\tWD\tRT\tWT\n");
    			first_monitor_interval = false;
    		}
    		fprintf(monitor_log, "%d\t%d\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\n",
    				pid_, tid_,
					intv_read_hist_.Average(), intv_write_hist_.Average(), 
					intv_read_hist_.StandardDeviation(), intv_write_hist_.StandardDeviation(), 
					intv_read_hist_.Num() * 1000000 /(intv_end_ - intv_start_),
					intv_write_hist_.Num() * 1000000 /(intv_end_ - intv_start_));

    		intv_start_ = intv_end_;
    		intv_read_hist_.Clear();
    		intv_write_hist_.Clear();
    	}
    	intv_mu_.Unlock();
    }
  }

  void AddBytes(int64_t n) {
    bytes_ += n;
  }

  void Report(const Slice& name) {
    // Pretend at least one op was done in case we are running a benchmark
    // that does not call FinishedSingleOp().
    if (done_ < 1) done_ = 1;

    std::string extra;
    double elapsed = (finish_ - start_) * 1e-6;
    if (bytes_ > 0) {
      // Rate is computed on actual elapsed time, not the sum of per-thread
      // elapsed times.
      char rate[100];
      snprintf(rate, sizeof(rate), "%6.1f MB/s",
               (bytes_ / 1048576.0) / elapsed);
      extra = rate;
    }
    AppendWithSpace(&extra, message_);	// only involve one thread

    fprintf(stdout, "%-12s : %11.3f micros/op;\t%11.3f ops/s%s%s\n",
            name.ToString().c_str(),
            seconds_ * 1e6 / done_,
            done_ / elapsed,
            (extra.empty() ? "" : " "),
            extra.c_str());
    if (FLAGS_histogram) {
      fprintf(stdout, "Microseconds per op:\n%s\n", hist_.ToString().c_str());
    if (read_done_ > 0)
      fprintf(stdout, "Microseconds per ReadOp:\n%s\n", read_hist_.ToString().c_str());
    if (write_done_ > 0)
      fprintf(stdout, "Microseconds per WriteOp:\n%s\n", write_hist_.ToString().c_str());
    }
    fflush(stdout);
  }
};

// State shared by all concurrent executions of the same benchmark.
struct SharedState {
  port::Mutex mu;
  port::CondVar cv;
  int total;

  // Each thread goes through the following states:
  //    (1) initializing
  //    (2) waiting for others to be initialized
  //    (3) running
  //    (4) done

  int num_initialized;
  int num_done;
  bool start;

  SharedState() : cv(&mu) { }
};

// Per-thread state for concurrent executions of the same benchmark.
struct ThreadState {
  int tid;             // 0..n-1 when running in n threads
  Random *rand;         // Threads share the same seed
  Stats stats;
  SharedState* shared;

  ThreadState(int index, Random *r)
      : tid(index),
        rand(r) {
			stats.tid_ = index;
			stats.pid_ = getpid();
  }
};

}  // namespace

class Benchmark {
 private:
  Cache* cache_;
  SSDCache* ssd_cache_;
  const FilterPolicy* filter_policy_;
  DB* db_;
  int value_size_;
  int entries_per_batch_;
  WriteOptions write_options_;
  int reads_;
  int heap_counter_;

  void PrintHeader() {
    const int kKeySize = 16;
    int num = FLAGS_reads>0?FLAGS_reads:0+FLAGS_writes>0?FLAGS_writes:0;
    PrintEnvironment();
    fprintf(stdout, "Keys:       %d bytes each\n", kKeySize);
    fprintf(stdout, "Values:     %d bytes each (%d bytes after compression)\n",
            FLAGS_value_size,
            static_cast<int>(FLAGS_value_size * FLAGS_compression_ratio + 0.5));
    fprintf(stdout, "Entries:    %d\n", num);
    fprintf(stdout, "RawSize:    %.1f MB (estimated)\n",
            ((static_cast<int64_t>(kKeySize + FLAGS_value_size) * num)
             / 1048576.0));
    fprintf(stdout, "FileSize:   %.1f MB (estimated)\n",
            (((kKeySize + FLAGS_value_size * FLAGS_compression_ratio) * num)
             / 1048576.0));
    PrintWarnings();
    fprintf(stdout, "------------------------------------------------\n");
  }

  void PrintWarnings() {
#if defined(__GNUC__) && !defined(__OPTIMIZE__)
    fprintf(stdout,
            "WARNING: Optimization is disabled: benchmarks unnecessarily slow\n"
            );
#endif
#ifndef NDEBUG
    fprintf(stdout,
            "WARNING: Assertions are enabled; benchmarks unnecessarily slow\n");
#endif

    // See if snappy is working by attempting to compress a compressible string
    const char text[] = "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy";
    std::string compressed;
    if (!port::Snappy_Compress(text, sizeof(text), &compressed)) {
      fprintf(stdout, "WARNING: Snappy compression is not enabled\n");
    } else if (compressed.size() >= sizeof(text)) {
      fprintf(stdout, "WARNING: Snappy compression is not effective\n");
    }
  }

  void PrintEnvironment() {
    fprintf(stderr, "LevelDB:    version %d.%d\n",
            kMajorVersion, kMinorVersion);

#if defined(__linux)
    time_t now = time(NULL);
    fprintf(stderr, "Date:       %s", ctime(&now));  // ctime() adds newline

    FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
    if (cpuinfo != NULL) {
      char line[1000];
      int num_cpus = 0;
      std::string cpu_type;
      std::string cache_size;
      while (fgets(line, sizeof(line), cpuinfo) != NULL) {
        const char* sep = strchr(line, ':');
        if (sep == NULL) {
          continue;
        }
        Slice key = TrimSpace(Slice(line, sep - 1 - line));
        Slice val = TrimSpace(Slice(sep + 1));
        if (key == "model name") {
          ++num_cpus;
          cpu_type = val.ToString();
        } else if (key == "cache size") {
          cache_size = val.ToString();
        }
      }
      fclose(cpuinfo);
      fprintf(stderr, "CPU:        %d * %s\n", num_cpus, cpu_type.c_str());
      fprintf(stderr, "CPUCache:   %s\n", cache_size.c_str());
    }
#endif
  }

 public:
  Benchmark()
  : cache_(FLAGS_cache_size >= 0 ? NewLRUCache(FLAGS_cache_size) : NULL),
    filter_policy_(FLAGS_bloom_bits >= 0
                   ? NewBloomFilterPolicy(FLAGS_bloom_bits)
                   : NULL),
    db_(NULL),
    value_size_(FLAGS_value_size),
    entries_per_batch_(1),
    reads_(FLAGS_reads),
    heap_counter_(0) {

	if(FLAGS_ssd_cache_size >= (size_t)0){
	   assert(FLAGS_ssd_cache_path != NULL);
	   std::string cache_path = std::string(FLAGS_ssd_cache_path);
	   ssd_cache_ = new SSDCache(cache_path,BLOCKSIZE,(FLAGS_ssd_cache_size*1024)/(BLOCKSIZE/1024));
	}else{
		ssd_cache_ = NULL;
	}

    std::vector<std::string> files;
    Env::Default()->GetChildren(FLAGS_db, &files);
    for (size_t i = 0; i < files.size(); i++) {
      if (Slice(files[i]).starts_with("heap-")) {
        Env::Default()->DeleteFile(std::string(FLAGS_db) + "/" + files[i]);
      }
    }
    if (!FLAGS_use_existing_db) {
      DestroyDB(FLAGS_db, Options());
    }
  }

  ~Benchmark() {

    delete db_;
    delete cache_;
    delete filter_policy_;
  }

  void Run() {

    PrintHeader();
    Open();


    const char* benchmarks = FLAGS_benchmarks;
    while (benchmarks != NULL) {
      const char* sep = strchr(benchmarks, ',');
      Slice name;
      if (sep == NULL) {
        name = benchmarks;
        benchmarks = NULL;
      } else {
        name = Slice(benchmarks, sep - benchmarks);
        benchmarks = sep + 1;
      }

      // Reset parameters that may be overriddden bwlow
      reads_ = FLAGS_reads;
      value_size_ = FLAGS_value_size;
      entries_per_batch_ = 1;
      write_options_ = WriteOptions();

      void (Benchmark::*method)(ThreadState*) = NULL;
      bool fresh_db = false;
      int num_threads = FLAGS_threads;
      /*if (name == Slice("rangequery")) {
          method = &Benchmark::RangeQuery;
      } else*/ if (name == Slice("stats")) {
        PrintStats("leveldb.stats");
      } else if (name == Slice("sstables")) {
        PrintStats("leveldb.sstables");
      } else if (name == Slice("rwrandom")) {
        method = &Benchmark::RWRandom_Write;
        monitor_interval = 2000000;
      } else {
        if (name != Slice()) {  // No error message for empty name
          fprintf(stderr, "unknown benchmark '%s'\n", name.ToString().c_str());
        }
      }

      if (fresh_db) {
        if (FLAGS_use_existing_db) {
          fprintf(stdout, "%-12s : skipped (--use_existing_db is true)\n",
                  name.ToString().c_str());
          method = NULL;
        } else {
          delete db_;
          db_ = NULL;
          DestroyDB(FLAGS_db, Options());
          Open();
        }
      }

      if (method != NULL) {
        RunBenchmark(num_threads, name, method);
      }
    }
  }

 private:
  struct ThreadArg {
    Benchmark* bm;
    SharedState* shared;
    ThreadState* thread;
    void (Benchmark::*method)(ThreadState*);
  };

  static void ThreadBody(void* v) {
    ThreadArg* arg = reinterpret_cast<ThreadArg*>(v);
    SharedState* shared = arg->shared;
    ThreadState* thread = arg->thread;
    {
      MutexLock l(&shared->mu);
      shared->num_initialized++;
      if (shared->num_initialized >= shared->total) {
        shared->cv.SignalAll();
      }
      while (!shared->start) {
        shared->cv.Wait();
      }
    }

    thread->stats.Start();
    (arg->bm->*(arg->method))(thread);
    thread->stats.Stop();

    {
      MutexLock l(&shared->mu);
      shared->num_done++;
      if (shared->num_done >= shared->total) {
        shared->cv.SignalAll();
      }
    }
  }

  void RunBenchmark(int n, Slice name,
                    void (Benchmark::*method)(ThreadState*)) {
	/*read_latency is the overall latency, latency for each thread should longer*/
	read_latency = read_latency*(FLAGS_threads-1);
	//std::cout<<read_latency<<std::endl;
  	Random rand_(FLAGS_random_seed);
    SharedState shared;
    shared.total = n;
    shared.num_initialized = 0;
    shared.num_done = 0;
    shared.start = false;

	//Intialization for request latency monitoring
	intv_read_hist_.Clear();
	intv_write_hist_.Clear();
	intv_start_ = Env::Default()->NowMicros();

    ThreadArg* arg = new ThreadArg[n];
    for (int i = 0; i < n; i++) {
      arg[i].bm = this;
      arg[i].method = method;
      arg[i].shared = &shared;
      arg[i].thread = new ThreadState(i, &rand_);
      arg[i].thread->shared = &shared;
      Env::Default()->StartThread(ThreadBody, &arg[i]);
      if (method == &Benchmark::RWRandom_Write) // multiple read threads, one write thread
         method = &Benchmark::RWRandom_Read;
    }


    shared.mu.Lock();
    while (shared.num_initialized < n) {
      shared.cv.Wait();
    }

    shared.start = true;
    shared.cv.SignalAll();
    while (shared.num_done < n) {
      shared.cv.Wait();
    }
    shared.mu.Unlock();

    for (int i = 1; i < n; i++) {
      arg[0].thread->stats.Merge(arg[i].thread->stats);
    }
    arg[0].thread->stats.Report(name);

    for (int i = 0; i < n; i++) {
      delete arg[i].thread;
    }
    delete[] arg;
    fprintf(stderr,"\ntotally operated %lld range queries, and %d results are found\n",range_count_,range_total_);

  }

  void Open() {

    assert(db_ == NULL);
    Options options;
    options.create_if_missing = true;//!FLAGS_use_existing_db;
    options.block_cache = cache_;
    options.ssd_block_cache = ssd_cache_;
    options.write_buffer_size = FLAGS_write_buffer_size;
    options.max_open_files = FLAGS_open_files;
    options.filter_policy = filter_policy_;
    options.compression = leveldb::kNoCompression;

    Status s = DB::Open(options, FLAGS_db, &db_);
    if (!s.ok()) {
      fprintf(stderr, "open error: %s\n", s.ToString().c_str());
      exit(1);
    }
  }

double random()
{
	return (double)(rand()/(double)RAND_MAX);
}
/*modification required for key*/

void RangeQuery(ThreadState* thread)
{
	ReadOptions options;
	options.fill_cache = false;
	sequentialread(options, FLAGS_range_size);
}

void sequentialread(const ReadOptions options, double range)
{
      int64_t bytes = 0;
      int count = 0;

	  range_query_mu_.Lock();
      range_count_++;
	  range_query_mu_.Unlock();
      double from_portion = random();
      from_portion = from_portion+range>1?1-range:from_portion;
      long long base = 1000000;
      long long fromll = (long long)(base*from_portion);
      long long rangell = (long long)(base*range);

      long long read_key_from = fromll*(llmax_/base);
      long long read_key_to = llabs(read_key_from + rangell*(llmax_/base));
      char startch[100],endch[100];
      snprintf(startch, sizeof(startch), "user%019lld", read_key_from);
      snprintf(endch, sizeof(endch), "user%019lld", read_key_to);
      std::string startstr(startch);
      std::string endstr(endch);
      Slice start(startstr);
      Slice end(endstr);
      count = db_->GetRange(options,start,end);
      range_query_mu_.Lock();
      range_total_ += count;
      range_query_mu_.Unlock();
}
//bool cached[110000];
  void RWRandom_Read(ThreadState* thread) {

    ReadOptions options;
    std::string value;
    int rangeid = 0;

    RandomGenerator gen;
    WriteBatch batch;
    Status s;
    int64_t bytes = 0;
    bool isRead, isFound,issequential;
    time_t begin, now;
	struct timeval start, end;
	int64_t latency = 0;
	range_query_mu_.Lock();
	rangeid = FLAGS_range_threads--;
	if(FLAGS_range_threads>=0)
		issequential=true;
	else
		issequential=false;
	range_query_mu_.Unlock();

    int found = 0;
    int bnum = 0;
    batch.Clear();

    int done = 0;
	generator::IntegerGenerator *mygenerator;
	if(strcmp(FLAGS_ycsb_workload,"zipfian")==0){
	   mygenerator = new generator::ZipfianGenerator(FLAGS_read_from,(long int)FLAGS_read_upto);
	}
	else if(strcmp(FLAGS_ycsb_workload,"latest")==0){
	  generator::CounterGenerator base(FLAGS_read_upto);
	  mygenerator = new generator::SkewedLatestGenerator(base);
	}
	else {
	  mygenerator = new generator::UniformIntegerGenerator(FLAGS_read_from,FLAGS_read_upto);
	}
	uint64_t k = FLAGS_read_from;
    char key[100];
    int cachesize = ssd_cache_->FreeListSize();
    int cur_cache_size = 0;
    while(leveldb::runtime::isWarmingUp()){

    	snprintf(key, sizeof(key), "user%019lld", generator::YCSBKey_hash(k++));
        db_->Get(options,key,&value);
        //cached[k] = true;
        cur_cache_size = ssd_cache_->FreeListSize();
        //(double)cur_cache_size/cachesize<=0.01 || ;
        if(k>=FLAGS_read_upto){
        	leveldb::runtime::warming_up = false;
        }else{
        	fprintf(stdout,"%10d\%10d current used cache is %f%\n",k,FLAGS_read_upto,100.0*(1.0-(double)cur_cache_size/cachesize));
        	fflush(stdout);
        }
    }
    k = FLAGS_read_from;
	time(&begin);
    int notfoundforcache = 0;
    int foundnotcached = 0;
    while(true)
    {
	  //gettimeofday(&start,NULL);
      time(&now);

	  if (difftime(now, begin) > FLAGS_countdown || (done>=reads_&&reads_>=0)){
		    printf("%d I need to exit!\n",issequential);
    		break;
	  }

      if(!issequential){
       if(FLAGS_read_throughput==0&&!issequential){
    	   Env::Default()->SleepForMicroseconds(FLAGS_countdown*1000000);
    	   break;
       }
       int ok = k;
       k = (k+1)%FLAGS_read_upto+FLAGS_read_from; //mygenerator->nextInt();
       snprintf(key, sizeof(key), "user%019lld", generator::YCSBKey_hash(k));
       s = db_->Get(options, key, &value);
       isFound = s.ok();

       done++;
       if (isFound) {
         found++;

       }
       thread->stats.FinishedReadOp();

       rwrandom_read_mu_.Lock();
       rwrandom_read_completed++;
       rwrandom_read_mu_.Unlock();
        /*
	    gettimeofday(&end,NULL);
	    latency = (end.tv_sec-start.tv_sec)*1000000+(end.tv_usec-start.tv_usec);
	    if(FLAGS_read_throughput>0&&read_latency>latency)
	    {
	     Env::Default()->SleepForMicroseconds(read_latency-latency);
	    }*/
      }//random read
      else{

          long long range_count_tmp = 0;
          options.fill_cache = false;
          sequentialread(options,FLAGS_range_size);
    	  range_query_mu_.Lock();
    	  range_count_tmp = range_count_;
    	  range_query_mu_.Unlock();
    	  if(range_count_tmp>=FLAGS_range_query_number)
          break;
      }//sequential read
    }//end while

    char msg[100];
    snprintf(msg, sizeof(msg), "(%d of %d found in one read thread)", found, done);
    thread->stats.AddMessage(msg);

    time(&now);
    if(!issequential)
    fprintf(stderr, "rwrandom completes %d read ops (out of %d) in %.3f seconds, %d found\n",
      done, rwrandom_read_completed, difftime(now, begin), found);
  }
/*modification required for key*/

  void RWRandom_Write(ThreadState* thread) {
    ReadOptions options;
    std::string value;

    RandomGenerator gen;
    WriteBatch batch;
    Status s;
    int64_t bytes = 0;
    bool isRead;
	
    time_t begin, now;
	struct timeval start, end;
	int64_t latency = 0;

    int found = 0;
    int bnum = 0;
    batch.Clear();

    while(leveldb::runtime::isWarmingUp()){
    	//sleep 100ms and check warmup again to see if it already fill up the cache
    	Env::Default()->SleepForMicroseconds(100000);
    }
    int i = 0;
    int wnum = FLAGS_countdown * FLAGS_write_throughput;
    if(FLAGS_writes>=0)
    	wnum=FLAGS_writes;
    else
    	wnum = -1;
    if(wnum>0)
        fprintf(stderr, "RWRandom_Write will try to write %d ops\n", wnum);

    int done = 0;
	generator::IntegerGenerator *mygenerator;
	if(strcmp(FLAGS_ycsb_workload,"zipfian")==0){
	   mygenerator = new generator::ZipfianGenerator(FLAGS_write_from,(long int )FLAGS_write_upto);
	}
	else if(strcmp(FLAGS_ycsb_workload,"latest")==0){
	  generator::CounterGenerator base(FLAGS_write_upto);
	  mygenerator = new generator::SkewedLatestGenerator(base);
	}
	else {
	  mygenerator = new generator::UniformIntegerGenerator(FLAGS_write_from,FLAGS_write_upto);
	}
	if(FLAGS_write_throughput==0)
	{
	  Env::Default()->SleepForMicroseconds(FLAGS_countdown*1000000);
	  return;
	}
    time(&begin);

	uint64_t k;
    while(true){
      char key[100];
      /*
	  gettimeofday(&start,NULL);*/
      time(&now);
	  if (difftime(now, begin) >= FLAGS_countdown)
          break;
      if(done>=wnum&&wnum>=0)
      {
    	  if (difftime(now, begin) > FLAGS_countdown){
	            break;
    	  }
    	  else{
               fprintf(stderr,"I have finished %d write and now I need to sleep for %d seconds\n",wnum, (int)(FLAGS_countdown-difftime(now,begin)));
               Env::Default()->SleepForMicroseconds((FLAGS_countdown-difftime(now,begin))*1000000);
               break;
    	  }
      }

      k = (k+1)%FLAGS_write_upto+FLAGS_write_from;//mygenerator->nextInt();
      snprintf(key, sizeof(key), "user%019lld", generator::YCSBKey_hash(k));
      batch.Put(key, gen.Generate(value_size_));
      bytes += value_size_ + strlen(key);
      bnum ++;
      done ++;

      if (bnum == entries_per_batch_) {
          bnum = 0;
          s = db_->Write(write_options_, &batch);
          batch.Clear();
          if (!s.ok()) {
            fprintf(stderr, "put error: %s\n", s.ToString().c_str());
            exit(1);
          }
          thread->stats.AddBytes(bytes);
          bytes = 0;
        }
    	thread->stats.FinishedWriteOp();
	    rwrandom_write_completed++;
      /*
	  gettimeofday(&end,NULL);
	  latency = (end.tv_sec-start.tv_sec)*1000000+(end.tv_usec-start.tv_usec);
	  if(write_latency>latency&&FLAGS_write_throughput>0)
	  { 
	     Env::Default()->SleepForMicroseconds(write_latency-latency);
	  }*/
    }

    time(&now);
    fprintf(stderr, "rwrandom completes %d write ops in %.3f seconds\n",done, difftime(now, begin));

  }


  void PrintStats(const char* key) {
    std::string stats;
    if (!db_->GetProperty(key, &stats)) {
      stats = "(failed)";
    }
    fprintf(stdout, "\n%s\n", stats.c_str());
  }

  static void WriteToFile(void* arg, const char* buf, int n) {
    reinterpret_cast<WritableFile*>(arg)->Append(Slice(buf, n));
  }

  void HeapProfile() {
    char fname[100];
    snprintf(fname, sizeof(fname), "%s/heap-%04d", FLAGS_db, ++heap_counter_);
    WritableFile* file;
    Status s = Env::Default()->NewWritableFile(fname, &file);
    if (!s.ok()) {
      fprintf(stderr, "%s\n", s.ToString().c_str());
      return;
    }
    bool ok = port::GetHeapProfile(WriteToFile, file);
    delete file;
    if (!ok) {
      fprintf(stderr, "heap profiling not supported\n");
      Env::Default()->DeleteFile(fname);
    }
  }
};

}  // namespace leveldb

int main(int argc, char** argv) {
  FLAGS_write_buffer_size = leveldb::Options().write_buffer_size;
  FLAGS_open_files = leveldb::Options().max_open_files;
  std::string default_db_path;

  for (int i = 1; i < argc; i++) {
    double d;
    int n;
    long long ll;
    int64_t n64;
    char junk;
    if (leveldb::Slice(argv[i]).starts_with("--benchmarks=")) {
      FLAGS_benchmarks = argv[i] + strlen("--benchmarks=");
    } else if (sscanf(argv[i], "--compression_ratio=%lf%c", &d, &junk) == 1) {
      FLAGS_compression_ratio = d;
    } else if (sscanf(argv[i], "--histogram=%d%c", &n, &junk) == 1 &&
               (n == 0 || n == 1)) {
      FLAGS_histogram = n;
    } else if (sscanf(argv[i], "--use_existing_db=%d%c", &n, &junk) == 1 &&
               (n == 0 || n == 1)) {
      FLAGS_use_existing_db = n;
    } else if (sscanf(argv[i], "--reads=%d%c", &n, &junk) == 1) {
      FLAGS_reads = n;
    } else if (sscanf(argv[i], "--writes=%d%c", &n, &junk) == 1) {
      FLAGS_writes = n;
    } else if (sscanf(argv[i], "--threads=%d%c", &n, &junk) == 1) {
      FLAGS_threads = n;
    } else if (sscanf(argv[i], "--value_size=%d%c", &n, &junk) == 1) {
      FLAGS_value_size = n;
    } else if (sscanf(argv[i], "--write_buffer_size=%d%c", &n, &junk) == 1) {
      FLAGS_write_buffer_size = n;
    } else if (sscanf(argv[i], "--cache_size=%d%c", &n, &junk) == 1) {
      FLAGS_cache_size = n;
    } else if (sscanf(argv[i], "--ssd_cache_size=%d%c", &n, &junk) == 1) {
        FLAGS_ssd_cache_size = n;
    } else if (sscanf(argv[i], "--bloom_bits=%d%c", &n, &junk) == 1) {
      FLAGS_bloom_bits = n;
    } else if (sscanf(argv[i], "--bloom_bits_use=%d%c", &n, &junk) == 1) {
      leveldb::config::bloom_bits_use = n;
    } else if (sscanf(argv[i], "--open_files=%d%c", &n, &junk) == 1) {
      FLAGS_open_files = n;
    } else if (strncmp(argv[i], "--db=", 5) == 0) {
      FLAGS_db = argv[i] + 5;
      leveldb::config::primary_storage_path = FLAGS_db;
    } else if (strncmp(argv[i], "--ssd_cache_path=", 17) == 0) {
      FLAGS_ssd_cache_path = argv[i] + 17;
    } else if (sscanf(argv[i], "--read_key_from=%ld%c", &n64, &junk) == 1) {
      FLAGS_read_from = n64;
    } else if (sscanf(argv[i], "--read_key_upto=%ld%c", &n64, &junk) == 1) {
      FLAGS_read_upto = n64;
    } else if (sscanf(argv[i], "--write_key_from=%ld%c", &n64, &junk) == 1) {
      FLAGS_write_from = n64;
    } else if (sscanf(argv[i], "--write_key_upto=%ld%c", &n64, &junk) == 1) {
      FLAGS_write_upto = n64;
    } else if (sscanf(argv[i], "--file_size=%d%c", &n, &junk) == 1) {
      leveldb::config::kTargetFileSize = n * 1048576; // in MiB
    } else if (sscanf(argv[i], "--level0_size=%d%c", &n, &junk) == 1) {
      leveldb::config::kL0_size = n;
    } else if (sscanf(argv[i], "--countdown=%lf%c", &d, &junk) == 1) {
      FLAGS_countdown = d;
    } else if (sscanf(argv[i], "--random_seed=%lf%c", &d, &junk) == 1) {
      FLAGS_random_seed = d;
    } else if (sscanf(argv[i], "--run_compaction=%d%c", &n, &junk) == 1) {
      leveldb::config::run_compaction = n;
    } else if (sscanf(argv[i], "--two_phase_compaction=%d%c", &n, &junk) == 1) {
        leveldb::runtime::two_phase_compaction = n;
    } else if (sscanf(argv[i], "--warmup=%d%c", &n, &junk) == 1) {
        leveldb::runtime::warming_up = n;
    } else if (strncmp(argv[i], "--monitor_log=", 14) == 0) {
      monitor_log = fopen(argv[i] + 14, "w");
	} else if (strncmp(argv[i], "--workload=",11)==0) {
	  FLAGS_ycsb_workload = argv[i] + 11;
    } else if (sscanf(argv[i], "--writespeed=%d%c", &n, &junk) == 1)  {
	  FLAGS_write_throughput = n;
	  if(FLAGS_write_throughput>0)
	  write_latency = 1000000/FLAGS_write_throughput;
    } else if (sscanf(argv[i], "--readspeed=%d%c", &n, &junk) == 1) {
	  FLAGS_read_throughput = n;
	  if(FLAGS_read_throughput>0)
	  read_latency = 1000000/FLAGS_read_throughput;
    } else if (sscanf(argv[i], "--range_size=%lf%c", &d, &junk) == 1) {
        FLAGS_range_size = d;
    } else if (sscanf(argv[i], "--range_threads=%d%c", &n, &junk) == 1) {
        FLAGS_range_threads = n;
    } else if (sscanf(argv[i], "--range_query_num=%d%c", &n, &junk) == 1) {
        FLAGS_range_query_number = n;
    } else if (sscanf(argv[i], "--dbmode=%d%c", &n, &junk) == 1) {
        leveldb::config::dbmode = n;
        if(n!=0&&n!=1&&n!=2){
        	fprintf(stderr,"error dbmode, can only be 0(LSM) or 1(dLSM) 2(SM)\n");
        	exit(0);
        }
    } else {
      fprintf(stderr, "Invalid flag '%s'\n", argv[i]);
      exit(1);
    }
  }
  if(leveldb::config::isdLSM()||leveldb::config::isSM()){
	  leveldb::runtime::two_phase_compaction = true;
  }
  FLAGS_read_span = FLAGS_read_upto - FLAGS_read_from;
  FLAGS_write_span = FLAGS_write_upto - FLAGS_write_from;
  fprintf(stderr, "Range: %ld(w) %ld(r)\n", FLAGS_write_span, FLAGS_read_span);

  // Choose a location for the test database if none given with --db=<path>
  if (FLAGS_db == NULL) {
      leveldb::Env::Default()->GetTestDirectory(&default_db_path);
      default_db_path += "/dbbench";
      FLAGS_db = default_db_path.c_str();
  }
  leveldb::Benchmark benchmark;
  benchmark.Run();
  return 0;
}
