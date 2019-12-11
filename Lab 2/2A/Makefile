build:
	gcc -Wextra -Wall -pthread lab2_add.c -o lab2_add
	gcc -Wextra -Wall -pthread -g SortedList.c lab2_list.c -o lab2_list

clean:
	rm -rf lab2_list lab2_add lab2a-005081232.tar.gz
dist: graphs
	tar -zcvf lab2a-005081232.tar.gz Makefile README *.gp *.png *.csv *.c *.h s.sh

graphs: tests
	gnuplot lab2_add.gp
	gnuplot lab2_list.gp

tests: clean build
	./s.sh