Filebench Version 1.5-alpha3
0.000: Allocated 173MB of shared memory
0.009: FileMicro-WriteRand Version 2.1 personality successfully loaded
0.009: Populating and pre-allocating filesets
0.009: Removing bigfile1 tree (if exists)
0.013: Pre-allocating directories in bigfile1 tree
0.013: Pre-allocating files in bigfile1 tree
21.980: Waiting for pre-allocation to finish (in case of a parallel pre-allocation)
21.980: Population and pre-allocation of filesets completed
21.980: Starting 1 filewriter instances
22.982: Running...
25.983: Run took 3 seconds...
25.983: Per-Operation Breakdown
finish               65537ops    21842ops/s   0.0mb/s      0.0ms/op [0.00ms -  0.00ms]
write-file           65538ops    21842ops/s  42.7mb/s      0.0ms/op [0.03ms - 81.14ms]
25.983: IO Summary: 65538 ops 21842.134 ops/s 0/21842 rd/wr  42.7mb/s   0.0ms/op
25.983: Shutting down processes
