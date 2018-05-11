Filebench Version 1.5-alpha3
0.000: Allocated 173MB of shared memory
0.001: FileMicro-SeqWrite Version 2.2 personality successfully loaded
0.001: Populating and pre-allocating filesets
0.001: Removing bigfile tree (if exists)
0.002: Pre-allocating directories in bigfile tree
0.002: Pre-allocating files in bigfile tree
0.003: Waiting for pre-allocation to finish (in case of a parallel pre-allocation)
0.003: Population and pre-allocation of filesets completed
0.003: Starting 1 filewriter instances
1.004: Running...
37.007: Run took 36 seconds...
37.007: Per-Operation Breakdown
finish               1ops        0ops/s   0.0mb/s      0.0ms/op [0.00ms -  0.00ms]
write-file           1025ops       28ops/s  28.4mb/s     34.7ms/op [0.36ms - 109.65ms]
37.007: IO Summary:  1025 ops 28.470 ops/s 0/28 rd/wr  28.4mb/s  34.7ms/op
37.007: Shutting down processes
