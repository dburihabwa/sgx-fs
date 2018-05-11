#! /usr/bin/env gnuplot

set term post color eps 22 enhanced
set output "plot.eps"

set size 1.6,1

set grid noxtics ytics

set style data histograms
set title "Throughput"
set ylabel "Throughput in MB/s"
set xlabel "filebench workloads"

set style line 1 lt 1 lc rgb "black" #"0xADFF2F"  #"0x2E8B57"
set style line 2 lt 1 lc rgb "blue"
set style line 3 lt 1 lc rgb "red"
set style fill pattern 5 border 0
set boxwidth 0.90


set key autotitle columnhead
set key outside bottom right horizontal

plot "data/tput.txt" u ($2):xtic(1) ls 600,\
     "data/tput.txt" u ($3):xtic(1) ls 700


!epstopdf "plot.eps"
!rm "plot.eps"
quit
