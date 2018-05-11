Filebench Version 1.5-alpha3
0.000: Allocated 173MB of shared memory
0.001: FileMicro-WriteRand Version 2.1 personality successfully loaded
0.001: Populating and pre-allocating filesets
0.001: Removing bigfile1 tree (if exists)
0.002: Pre-allocating directories in bigfile1 tree
0.002: Pre-allocating files in bigfile1 tree
17.050: Waiting for pre-allocation to finish (in case of a parallel pre-allocation)
17.050: Population and pre-allocation of filesets completed
17.050: Starting 1 filewriter instances
18.051: Running...
22.052: Run took 4 seconds...
22.052: Per-Operation Breakdown
finish               65537ops    16382ops/s   0.0mb/s      0.0ms/op [0.00ms -  0.00ms]
write-file           65538ops    16383ops/s  32.0mb/s      0.1ms/op [0.06ms - 53.33ms]
22.052: IO Summary: 65538 ops 16382.739 ops/s 0/16383 rd/wr  32.0mb/s   0.1ms/op
22.052: Shutting down processes
