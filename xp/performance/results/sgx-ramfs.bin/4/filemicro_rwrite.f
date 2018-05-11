Filebench Version 1.5-alpha3
0.000: Allocated 173MB of shared memory
0.001: FileMicro-WriteRand Version 2.1 personality successfully loaded
0.001: Populating and pre-allocating filesets
0.001: Removing bigfile1 tree (if exists)
0.002: Pre-allocating directories in bigfile1 tree
0.002: Pre-allocating files in bigfile1 tree
26.563: Waiting for pre-allocation to finish (in case of a parallel pre-allocation)
26.563: Population and pre-allocation of filesets completed
26.563: Starting 1 filewriter instances
27.566: Running...
40.567: Run took 13 seconds...
40.567: Per-Operation Breakdown
finish               65537ops     5041ops/s   0.0mb/s      0.0ms/op [0.00ms -  0.00ms]
write-file           65538ops     5041ops/s   9.8mb/s      0.2ms/op [0.06ms - 55.24ms]
40.567: IO Summary: 65538 ops 5040.927 ops/s 0/5041 rd/wr   9.8mb/s   0.2ms/op
40.567: Shutting down processes
