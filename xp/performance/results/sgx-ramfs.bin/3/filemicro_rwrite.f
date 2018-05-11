Filebench Version 1.5-alpha3
0.000: Allocated 173MB of shared memory
0.001: FileMicro-WriteRand Version 2.1 personality successfully loaded
0.001: Populating and pre-allocating filesets
0.001: Removing bigfile1 tree (if exists)
0.002: Pre-allocating directories in bigfile1 tree
0.002: Pre-allocating files in bigfile1 tree
16.141: Waiting for pre-allocation to finish (in case of a parallel pre-allocation)
16.141: Population and pre-allocation of filesets completed
16.141: Starting 1 filewriter instances
17.143: Running...
21.143: Run took 4 seconds...
21.143: Per-Operation Breakdown
finish               65537ops    16383ops/s   0.0mb/s      0.0ms/op [0.00ms -  0.00ms]
write-file           65538ops    16383ops/s  32.0mb/s      0.1ms/op [0.06ms - 52.99ms]
21.143: IO Summary: 65538 ops 16383.038 ops/s 0/16383 rd/wr  32.0mb/s   0.1ms/op
21.143: Shutting down processes
