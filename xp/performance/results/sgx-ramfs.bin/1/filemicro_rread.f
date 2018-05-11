Filebench Version 1.5-alpha3
0.000: Allocated 173MB of shared memory
0.004: FileMicro-ReadRand Version 2.2 personality successfully loaded
0.004: Populating and pre-allocating filesets
0.004: Removing bigfile1 tree (if exists)
0.005: Pre-allocating directories in bigfile1 tree
0.005: Pre-allocating files in bigfile1 tree
28.837: Waiting for pre-allocation to finish (in case of a parallel pre-allocation)
28.837: Population and pre-allocation of filesets completed
28.837: Starting 1 filereader instances
29.838: Running...
31.839: Run took 2 seconds...
31.839: Per-Operation Breakdown
finish               65537ops    32765ops/s   0.0mb/s      0.0ms/op [0.00ms -  0.00ms]
write-file           65538ops    32766ops/s  64.0mb/s      0.0ms/op [0.03ms - 47.26ms]
31.839: IO Summary: 65538 ops 32765.887 ops/s 32766/0 rd/wr  64.0mb/s   0.0ms/op
31.839: Shutting down processes
