Filebench Version 1.5-alpha3
0.000: Allocated 173MB of shared memory
0.001: FileMicro-WriteRand Version 2.1 personality successfully loaded
0.001: Populating and pre-allocating filesets
0.001: Removing bigfile1 tree (if exists)
0.002: Pre-allocating directories in bigfile1 tree
0.002: Pre-allocating files in bigfile1 tree
4.835: Waiting for pre-allocation to finish (in case of a parallel pre-allocation)
4.835: Population and pre-allocation of filesets completed
4.835: Starting 1 filewriter instances
5.836: Running...
7.837: Run took 2 seconds...
7.837: Per-Operation Breakdown
finish               65537ops    32765ops/s   0.0mb/s      0.0ms/op [0.00ms -  0.00ms]
write-file           65538ops    32765ops/s  64.0mb/s      0.0ms/op [0.01ms - 67.33ms]
7.837: IO Summary: 65538 ops 32765.330 ops/s 0/32765 rd/wr  64.0mb/s   0.0ms/op
7.837: Shutting down processes
