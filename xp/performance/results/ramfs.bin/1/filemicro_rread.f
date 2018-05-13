Filebench Version 1.5-alpha3
0.000: Allocated 173MB of shared memory
0.001: FileMicro-ReadRand Version 2.2 personality successfully loaded
0.001: Populating and pre-allocating filesets
0.001: Removing bigfile1 tree (if exists)
0.002: Pre-allocating directories in bigfile1 tree
0.002: Pre-allocating files in bigfile1 tree
4.554: Waiting for pre-allocation to finish (in case of a parallel pre-allocation)
4.554: Population and pre-allocation of filesets completed
4.554: Starting 1 filereader instances
5.555: Running...
7.555: Run took 2 seconds...
7.555: Per-Operation Breakdown
finish               65537ops    32765ops/s   0.0mb/s      0.0ms/op [0.00ms -  0.01ms]
write-file           65538ops    32765ops/s  64.0mb/s      0.0ms/op [0.00ms - 63.85ms]
7.555: IO Summary: 65538 ops 32765.461 ops/s 32765/0 rd/wr  64.0mb/s   0.0ms/op
7.555: Shutting down processes
