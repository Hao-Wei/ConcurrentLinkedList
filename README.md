
# CPSC521: Debugging a concurrent linked list

## Building and running

```
cd ConcurrentLinkedList
mkdir build
cd build
cmake ..
cd benchmarks
make
./list_OL -help
usage: ./list_OL [-n <size>] [-r <rounds>] [-p <procs>] [-z <zipfian_param>] [-u <update percent>] [-tt <runtime (seconds)>]
```

Run `./list_OL` and `./list_HOH` to benchmark throughput of the optimistic locking (OL) and hand-over-hand (HOH) versions of the linked list. Can vary 
1. the initial size of the linked list (n), 
2. the number of processes (p), 
3. the zipfian_parameter (z, note that z=0 draws keys from the uniform distribution), 
4. percentage of updates (u: number between 0 and 100), and how long to run each experiment for (tt),
5. and number of rounds (r). 

After each round, the benchmark will report throughput (measured in millions of operations per second).

Default workloads:
- high contention: `./list_OL -n 20  -p 8 -r 3 -z 0.99 -u 100`
- low contention:  `./list_OL -n 100 -p 8 -r 3 -z 0    -u 20`

## Building in debug mode

```
cd ConcurrentLinkedList
mkdir build/debug
cd build/debug
cmake -DCMAKE_BUILD_TYPE=Debug ../..
cd benchmarks
make
./list_OL -help
```

## Using a scalable memory allocator

install mimalloc: https://microsoft.github.io/mimalloc/build.html
using mimalloc: https://microsoft.github.io/mimalloc/overrides.html

Example usage (mac followed by linux):

```
env DYLD_INSERT_LIBRARIES=/usr/local/lib/libmimalloc.dylib  ./list_OL -n 100 -p 4 -u 20 -z 0
LD_PRELOAD=/usr/lib/libmimalloc.so ./list_OL -n 100 -p 4 -u 20 -z 0
```

You should see a significant increase in throughput using mimalloc.

## Directory Structure

```
- ConcurentLinkedList
  - CMakeLists.txt
  - include
    - mylock
      - lock.h
    - parlay
  - structures
    - list_HOH    // linked list with hand-over-hand locking
      - set.h
    - list_OL     // linked list with optimistic locking
      - set.h
  - benchmark
    - CMakeLists.txt
    - test_sets.cpp     // the benchmarking driver
    - [ various .h files]
```

## Bugs

All bugs only show up when run with multiple threads. Can set the number of threads using the PARLAY_NUM_THREADS environment variable. Example usage (mac followed by linux):

```
env PARLAY_NUM_THREADS=1 env DYLD_INSERT_LIBRARIES=/usr/local/lib/libmimalloc.dylib  ./list_OL -n 100 -p 4 -u 20 -z 0
PARLAY_NUM_THREADS=1 LD_PRELOAD=/usr/local/lib/libmimalloc.dylib  ./list_OL -n 100 -p 4 -u 20 -z 0
```

Try to fix bugs in the following order:
  1. 3 correctness bugs in structures/list_OL/set.h (causes crashes)
  2. 1 linearizability bug in structures/list_OL/set.h (silent failure, not caught by any of the sanity checks)
  3. 1 performance bug in benchmark/test_sets.cpp (between lines 107-148)
  4. 1 performance bug in include/mylock/lock.h
  5. at least 2 additional performance optimizations can be applied to structures/list_OL/set.h (you are welcome to try to find more!)

For performance bugs, you should check how much of a difference your fix makes by running both the low contention and high contention workloads.

Mystery bugs: Even after fixing all of the issues listed above, you may notice that the code occasionally hangs/seg faults/bus errors once every ~20 runs.