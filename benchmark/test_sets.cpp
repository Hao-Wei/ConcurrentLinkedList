using K = unsigned long;
using V = unsigned long;

#include "set.h"

#include <string>
#include <iostream>
#include <sstream>
#include <limits>

#include <parlay/primitives.h>
#include <parlay/random.h>
#include <parlay/io.h>
#include <parlay/internal/get_time.h>
#include <parlay/internal/group_by.h>
#include "zipfian.h"
#include "parse_command_line.h"

void assert_key_exists(bool b) {
  if(!b) {
    std::cout << "key not found" << std::endl;
    abort();
  }
}

enum op_type : char {Find, Insert, Remove};

int main(int argc, char* argv[]) {
  commandLine P(argc,argv,"[-n <size>] [-r <rounds>] [-p <procs>] [-z <zipfian_param>] [-u <update percent>]");
  Set<K,V> list;
  
  // processes to run experiments with
  int p = P.getOptionIntValue("-p", parlay::num_workers());  

  int rounds = P.getOptionIntValue("-r", 1);
  double trial_time = P.getOptionDoubleValue("-tt", 1.0);
  
  // size of linked list (keys will be selected among 2n distinct keys)
  long initial_size = P.getOptionIntValue("-n", 100);
  long maxkey = 2*initial_size; // generate keys from range [1, maxkey]

  // number of samples 
  long m = P.getOptionIntValue("-m", (long) (trial_time * 5000000 * std::min(p, 100)));

  // run a trivial test
  bool init_test = P.getOption("-i"); // run trivial test

  // use zipfian distribution
  double zipfian_param = P.getOptionDoubleValue("-z", 0.0);
  bool use_zipfian = (zipfian_param != 0.0);

  // for mixed update/query, the percent that are updates
  int update_percent = P.getOptionIntValue("-u", 20); 

  if (init_test) {  // trivial test inserting 4 elements and deleting one
    std::cout << "rumaxkeying sanity checks" << std::endl;
    list.print();
    list.insert(3, 123);
    list.print();
    list.insert(7, 123);
    list.print();
    list.insert(1, 123);
    list.print();
    list.insert(11, 123);
    list.print();
    list.remove(3);
    list.print();
    assert_key_exists(list.find(7).has_value());
    assert_key_exists(list.find(1).has_value());
    assert_key_exists(list.find(11).has_value());
    assert(!list.find(10).has_value());
    assert(!list.find(3).has_value());
    std::cout << "sanity checks passed" << std::endl;

  } else {  // main benchmark
    using key_type = unsigned long;

    // generate maxkey unique numbers in random order
    parlay::sequence<key_type> permutation = parlay::random_shuffle(parlay::tabulate(maxkey, [] (key_type i) { return i+1; }));

    Zipfian z(maxkey, zipfian_param); // generate numbers from zipfian distribution

    // sample m keys according to zipfian distribution
    parlay::sequence<key_type> keys = parlay::tabulate(m, [&] (int i) { return permutation[z(i)]; });
    
    // sample m operation types
    parlay::sequence<op_type> op_types = parlay::tabulate(m, [&] (size_t i) -> op_type {
        auto h = parlay::hash64(m+i)%200;
        if (h < update_percent) return Insert; 
        else if (h < 2*update_percent) return Remove;
        else return Find; });
    
    parlay::internal::timer t;

    for (int i = 1; i <= rounds; i++) {
      // sanity check that list is initially empty
      long len = list.count_keys_and_check_consistency();
      if (len != 0) std::cout << "BAD LENGTH = " << len << std::endl;
      
      std::cout << "round " << i << std::endl;

      // prefill linked list with all even keys in the range [1,max_key]
      parlay::parallel_for(0, initial_size, [&] (size_t i) {
        list.insert(2*(i+1), 123); }, 10, true);

      // check that list is prefilled correctly
      size_t expected = initial_size;
      size_t got = list.count_keys_and_check_consistency();
      if (expected != got)
        std::cout << "warning: expected " << expected << ", found " << got << std::endl;

      parlay::sequence<size_t> totals(p);
      parlay::sequence<long> succesful_inserts(p);
      parlay::sequence<long> succesful_removes(p);
      parlay::sequence<long> query_counts(p);
      parlay::sequence<long> query_success_counts(p);
      size_t mp = m/p;   // divide keys[] and op_types[] between p threads
                         // thread i gets indices [i*mp, (i+1)*mp)
      t.start();
      auto start = std::chrono::system_clock::now();
      // spawn p threads, each performing insert/delete/find operations for trial_time seconds
      parlay::parallel_for(0, p, [&] (size_t tid) {
        size_t j = tid*mp; // j is the index of the next operation to be performed
        while (true) {
          // every once in a while check if time is over
          if (totals[tid] % 128 == 0) { 
            auto current = std::chrono::system_clock::now();
            double duration = std::chrono::duration_cast<std::chrono::milliseconds>(current - start).count();
            if (duration > 1000*trial_time)
              return;
          }
        
          if (op_types[j] == Find) {
            query_counts[tid]++;
            query_success_counts[tid] += list.find(keys[j]).has_value(); 
          }
          else if (op_types[j] == Insert) {
            if (list.insert(keys[j], 123)) succesful_inserts[tid]++;
          }
          else if (op_types[j] == Remove) {
            if (list.remove(keys[j])) succesful_removes[tid]++;
          }

          if (++j >= (tid+1)*mp) j -= mp; // pick next operation to run, wrap around if needed
          totals[tid]++;
        }
      }, 1, true);
      double duration = t.stop();

      //std::cout << duration << " : " << trial_time << std::endl;
      size_t num_ops = parlay::reduce(totals);
      std::cout << std::setprecision(4)
          << P.commandName() << ","
          << update_percent << "%update,"
          << "n=" << initial_size << ","
          << "p=" << p << ","
          << "z=" << zipfian_param << ",throughput (operations per microsecond)="
          << num_ops / (duration * 1e6) << std::endl;
      
      // sanity check linked list state
      size_t queries = parlay::reduce(query_counts);
      size_t queries_success = parlay::reduce(query_success_counts);
      double qratio = (double) queries_success / queries;
      if (qratio < .4 || qratio > .6)
        std::cout << "warning: query success ratio = " << qratio;
      size_t final_cnt = list.count_keys_and_check_consistency();
      long updates = parlay::reduce(succesful_inserts) - parlay::reduce(succesful_removes);

      if (initial_size + updates != final_cnt) {
        std::cout << "bad size: intial size = " << initial_size 
                  << ", added " << updates
                  << ", final size = " << final_cnt 
                  << std::endl;
        abort();
      } else {
              std::cout << "CHECK PASSED" << std::endl;
      }

      // remove all keys from linked list
      parlay::parallel_for(0, maxkey, [&] (size_t i) { list.remove(i+1); }, 10, true);
    }
  }
}
