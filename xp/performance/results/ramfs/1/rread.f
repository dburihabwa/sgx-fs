Filebench Version 1.5-alpha3
0.000: Allocated 173MB of shared memory
0.006: FileMicro-ReadRand Version 2.2 personality successfully loaded
0.006: Populating and pre-allocating filesets
0.006: Removing bigfile1 tree (if exists)
0.009: Pre-allocating directories in bigfile1 tree
0.010: Pre-allocating files in bigfile1 tree
25.753: Waiting for pre-allocation to finish (in case of a parallel pre-allocation)
25.753: Population and pre-allocation of filesets completed
25.753: Starting 1 filereader instances
26.754: Running...
29.755: Run took 3 seconds...
29.755: Per-Operation Breakdown
finish               65537ops    21843ops/s   0.0mb/s      0.0ms/op [0.00ms -  0.00ms]
write-file           65538ops    21844ops/s  42.7mb/s      0.0ms/op [0.00ms - 81.12ms]
29.755: IO Summary: 65538 ops 21843.568 ops/s 21844/0 rd/wr  42.7mb/s   0.0ms/op
29.755: Shutting down processes
