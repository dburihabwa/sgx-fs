Filebench Version 1.5-alpha3
0.000: Allocated 173MB of shared memory
0.001: FileMicro-ReadRand Version 2.2 personality successfully loaded
0.001: Populating and pre-allocating filesets
0.001: Removing bigfile1 tree (if exists)
0.002: Pre-allocating directories in bigfile1 tree
0.003: Pre-allocating files in bigfile1 tree
48.998: Waiting for pre-allocation to finish (in case of a parallel pre-allocation)
48.998: Population and pre-allocation of filesets completed
48.998: Starting 1 filereader instances
50.002: Running...
52.003: Run took 2 seconds...
52.003: Per-Operation Breakdown
finish               65537ops    32766ops/s   0.0mb/s      0.0ms/op [0.00ms -  0.00ms]
write-file           65538ops    32766ops/s  64.0mb/s      0.0ms/op [0.00ms - 54.06ms]
52.003: IO Summary: 65538 ops 32766.051 ops/s 32766/0 rd/wr  64.0mb/s   0.0ms/op
52.003: Shutting down processes
