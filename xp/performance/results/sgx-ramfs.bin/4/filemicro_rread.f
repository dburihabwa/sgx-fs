Filebench Version 1.5-alpha3
0.000: Allocated 173MB of shared memory
0.001: FileMicro-ReadRand Version 2.2 personality successfully loaded
0.001: Populating and pre-allocating filesets
0.001: Removing bigfile1 tree (if exists)
0.002: Pre-allocating directories in bigfile1 tree
0.003: Pre-allocating files in bigfile1 tree
29.704: Waiting for pre-allocation to finish (in case of a parallel pre-allocation)
29.704: Population and pre-allocation of filesets completed
29.704: Starting 1 filereader instances
30.707: Running...
33.707: Run took 3 seconds...
33.707: Per-Operation Breakdown
finish               65537ops    21844ops/s   0.0mb/s      0.0ms/op [0.00ms -  0.00ms]
write-file           65538ops    21844ops/s  42.7mb/s      0.0ms/op [0.00ms - 53.93ms]
33.707: IO Summary: 65538 ops 21843.881 ops/s 21844/0 rd/wr  42.7mb/s   0.0ms/op
33.707: Shutting down processes
