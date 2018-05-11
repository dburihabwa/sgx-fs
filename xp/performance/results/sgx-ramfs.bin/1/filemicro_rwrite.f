Filebench Version 1.5-alpha3
0.000: Allocated 173MB of shared memory
0.001: FileMicro-WriteRand Version 2.1 personality successfully loaded
0.001: Populating and pre-allocating filesets
0.001: Removing bigfile1 tree (if exists)
0.002: Pre-allocating directories in bigfile1 tree
0.003: Pre-allocating files in bigfile1 tree
16.140: Waiting for pre-allocation to finish (in case of a parallel pre-allocation)
16.140: Population and pre-allocation of filesets completed
16.140: Starting 1 filewriter instances
17.141: Running...
23.142: Run took 6 seconds...
23.142: Per-Operation Breakdown
finish               65537ops    10922ops/s   0.0mb/s      0.0ms/op [0.00ms -  0.01ms]
write-file           65538ops    10922ops/s  21.3mb/s      0.1ms/op [0.05ms - 52.34ms]
23.142: IO Summary: 65538 ops 10922.174 ops/s 0/10922 rd/wr  21.3mb/s   0.1ms/op
23.142: Shutting down processes
