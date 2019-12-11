build:
	gcc -Wextra -Wall -pthread -std=gnu11 lab2_add.c -o lab2_add
	gcc -Wextra -Wall -pthread -std=gnu11 -g SortedList.c lab2_list.c -o lab2_list

clean:
	rm -rf lab2_list lab2_add lab2b-005081232.tar.gz

dist: graphs profile
	tar -zcvf lab2b-005081232.tar.gz Makefile README *.gp *.png *.csv *.c *.h *.sh *.out

graphs: tests
	gnuplot lab2.gp

profile:
	-rm -f profile.gperf 
	LD_PRELOAD=/usr/lib64/libprofiler.so CPUPROFILE=profile.gperf ./lab2_list --threads=12 --iterations=1000 --sync=s
	pprof --text lab2_list profile.gperf > profile.out
	pprof --list=listWork lab2_list profile.gperf >> profile.out
	rm -rf profile.gperf

tests: clean build
	./a.sh
	./s.sh