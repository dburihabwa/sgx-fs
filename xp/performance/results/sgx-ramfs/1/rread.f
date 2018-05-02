Filebench Version 1.5-alpha3
0.000: Allocated 173MB of shared memory
0.001: FileMicro-ReadRand Version 2.2 personality successfully loaded
0.001: Populating and pre-allocating filesets
0.001: Removing bigfile1 tree (if exists)
0.005: Pre-allocating directories in bigfile1 tree
0.005: Pre-allocating files in bigfile1 tree
19.312: Waiting for pre-allocation to finish (in case of a parallel pre-allocation)
19.312: Population and pre-allocation of filesets completed
19.312: Starting 1 filereader instances
20.313: Running...
24.313: Run took 4 seconds...
24.313: Per-Operation Breakdown
finish               65537ops    16383ops/s   0.0mb/s      0.0ms/op [0.00ms -  0.00ms]
write-file           65538ops    16383ops/s  32.0mb/s      0.1ms/op [0.00ms - 84.27ms]
24.314: IO Summary: 65538 ops 16383.132 ops/s 16383/0 rd/wr  32.0mb/s   0.1ms/op
24.314: Shutting down processes
