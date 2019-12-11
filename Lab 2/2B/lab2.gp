#! /usr/bin/gnuplot
#
# input: lab2_add.csv
#	1. test name
#	2. # threads
#	3. # iterations per thread
#	4. # add operations
#	5. run time (ns)
#	6. run time per operation (ns)
#	7. total sum at end of run (should be zero)
#
# input: lab2b_list.csv
#	1. test name
#	2. # threads
#	3. # iterations per thread
#	4. # lists
#	5. # operations performed (threads x iterations x (ins + lookup + delete))
#	6. run time (ns)
#	7. run time per operation (ns)
#	8. (optional) average wait time for a lock (ns)
#

# general plot parameters
set terminal png
set datafile separator ","

# throughput vs number of threads for mutex and spin-lock synchronized adds and list operations
set title "2b_1: Thoughput vs Threads for mutex and spin-lock"
set xlabel "Threads"
set logscale x 2
set xrange [0.75:]
set ylabel "Throughput"
set logscale y 10
set output 'lab2b_1.png'

# grep out only successful (sum=0) yield runs
plot \
     "< grep -E \"add-m,[0-9]+,10000,\" lab2_add.csv" using ($2):(1000000000/($6)) \
	title 'add: mutex' with linespoints lc rgb 'red', \
     "< grep -E \"add-s,[0-9]+,10000,\" lab2_add.csv" using ($2):(1000000000/($6)) \
	title 'add: spin-lock' with linespoints lc rgb 'green', \
     "< grep -E \"list-none-m,[0-9]+,1000,\" lab2b_list.csv" using ($2):(1000000000/($7)) \
	title 'list: mutex' with linespoints lc rgb 'blue', \
     "< grep -E \"list-none-s,[0-9]+,1000,\" lab2b_list.csv" using ($2):(1000000000/($7)) \
	title 'list: spin-lock' with linespoints lc rgb 'orange'

# time per op and avg wait for lock time vs number of threads for mutex
set title "2b_2: Time per op and Avg lock wait time vs Threads for mutex"
set xlabel "Threads"
set logscale x 2
set xrange [0.75:]
set ylabel "Time"
set logscale y 10
set output 'lab2b_2.png'
set key left top

# grep out only successful (sum=0) yield runs
plot \
     "< grep -E \"list-none-m,2?[0-9],1000,1,|list-none-m,16,1000,1,\" lab2b_list.csv" using ($2):($7) \
	title 'time per op' with linespoints lc rgb 'red', \
     "< grep -E \"list-none-m,2?[0-9],1000,1,|list-none-m,16,1000,1,\" lab2b_list.csv" using ($2):($8) \
	title 'avg wait time for a lock' with linespoints lc rgb 'green'

set title "2b_3: Iterations that run without failure"
set logscale x 2
set xrange [0.75:]
set xlabel "Threads"
set ylabel "successful iterations"
set logscale y 10
set output 'lab2b_3.png'
set key right top
plot \
    "< grep -E \"list-id-none,[0-9]+,[0-9]+,4,\" lab2b_list.csv" using ($2):($3) \
	with points lc rgb "red" title "Unprotected", \
    "< grep -E \"list-id-m,[0-9]+,[0-9]+,4,\" lab2b_list.csv" using ($2):($3) \
	with points lc rgb "green" title "Mutex", \
    "< grep -E \"list-id-s,[0-9]+,[0-9]+,4,\" lab2b_list.csv" using ($2):($3) \
	with points lc rgb "blue" title "Spin-Lock"

set title "2b_4: Sublist thoughput vs Threads for mutex"
set xlabel "Threads"
set logscale x 2
set xrange [0.75:]
set ylabel "Throughput"
set logscale y 10
set output 'lab2b_4.png'

# grep out only successful (sum=0) yield runs
plot \
     "< grep -E \"list-none-m,[0-9],1000,1,|list-none-m,12,1000,1,\" lab2b_list.csv" using ($2):(1000000000/($7)) \
	title '1 list' with linespoints lc rgb 'red', \
     "< grep -E \"list-none-m,[0-9],1000,4,|list-none-m,12,1000,4,\" lab2b_list.csv" using ($2):(1000000000/($7)) \
	title '4 lists' with linespoints lc rgb 'green', \
     "< grep -E \"list-none-m,[0-9],1000,8,|list-none-m,12,1000,8,\" lab2b_list.csv" using ($2):(1000000000/($7)) \
	title '8 lists' with linespoints lc rgb 'blue', \
     "< grep -E \"list-none-m,[0-9],1000,16,|list-none-m,12,1000,16,\" lab2b_list.csv" using ($2):(1000000000/($7)) \
	title '16 lists' with linespoints lc rgb 'orange'

set title "2b_5: Sublist thoughput vs Threads for spin-lock"
set xlabel "Threads"
set logscale x 2
set xrange [0.75:]
set ylabel "Throughput"
set logscale y 10
set output 'lab2b_5.png'

# grep out only successful (sum=0) yield runs
plot \
     "< grep -E \"list-none-s,[0-9]+,1000,1,|list-none-s,12,1000,1,\" lab2b_list.csv" using ($2):(1000000000/($7)) \
	title '1 list' with linespoints lc rgb 'red', \
     "< grep -E \"list-none-s,[0-9]+,1000,4,|list-none-s,12,1000,4,\" lab2b_list.csv" using ($2):(1000000000/($7)) \
	title '4 lists' with linespoints lc rgb 'green', \
     "< grep -E \"list-none-s,[0-9]+,1000,8,|list-none-s,12,1000,8,\" lab2b_list.csv" using ($2):(1000000000/($7)) \
	title '8 lists' with linespoints lc rgb 'blue', \
     "< grep -E \"list-none-s,[0-9]+,1000,16,|list-none-s,12,1000,16,\" lab2b_list.csv" using ($2):(1000000000/($7)) \
	title '16 lists' with linespoints lc rgb 'orange'