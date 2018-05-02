Filebench Version 1.5-alpha3
0.000: Allocated 173MB of shared memory
0.001: FileMicro-SeqWrite Version 2.2 personality successfully loaded
0.001: Populating and pre-allocating filesets
0.001: Removing bigfile tree (if exists)
0.004: Pre-allocating directories in bigfile tree
0.005: Pre-allocating files in bigfile tree
0.005: Waiting for pre-allocation to finish (in case of a parallel pre-allocation)
0.005: Population and pre-allocation of filesets completed
0.005: Starting 1 filewriter instances
1.007: Running...
33.009: Run took 32 seconds...
33.010: Per-Operation Breakdown
finish               1ops        0ops/s   0.0mb/s      0.0ms/op [0.00ms -  0.00ms]
write-file           1025ops       32ops/s  32.0mb/s     30.8ms/op [0.50ms - 138.74ms]
33.010: IO Summary:  1025 ops 32.029 ops/s 0/32 rd/wr  32.0mb/s  30.8ms/op
33.010: Shutting down processes
