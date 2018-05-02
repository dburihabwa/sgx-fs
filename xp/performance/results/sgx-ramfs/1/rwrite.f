Filebench Version 1.5-alpha3
0.000: Allocated 173MB of shared memory
0.001: FileMicro-WriteRand Version 2.1 personality successfully loaded
0.001: Populating and pre-allocating filesets
0.001: Removing bigfile1 tree (if exists)
0.005: Pre-allocating directories in bigfile1 tree
0.006: Pre-allocating files in bigfile1 tree
19.087: Waiting for pre-allocation to finish (in case of a parallel pre-allocation)
19.087: Population and pre-allocation of filesets completed
19.087: Starting 1 filewriter instances
20.089: Running...
26.089: Run took 6 seconds...
26.089: Per-Operation Breakdown
finish               65537ops    10922ops/s   0.0mb/s      0.0ms/op [0.00ms -  0.00ms]
write-file           65538ops    10922ops/s  21.3mb/s      0.1ms/op [0.06ms - 83.06ms]
26.089: IO Summary: 65538 ops 10921.997 ops/s 0/10922 rd/wr  21.3mb/s   0.1ms/op
26.089: Shutting down processes
