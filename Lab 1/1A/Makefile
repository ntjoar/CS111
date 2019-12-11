# Name: Nathan Tjoar
# UID: 005081232
# Email: ntjoar@g.ucla.edu
default:
	gcc -Wall -Wextra -g -o simpsh lab1a.c
clean:
	rm -f simpsh *.o *.txt *.tar.gz
dist:
	tar -czvf lab1-005081232.tar.gz lab1a.c Makefile README
check: rdonly rdonlyF wronly wronlyF verbose
	if [ -s log.txt ]; \
	then \
	    cat log.txt; rm -f log.txt; \
	else \
	    echo "Passed Smoke Check"; \
	fi 

### Smoke tests ###
rdonly:
	echo "Some text" > r.txt; \
	./simpsh --rdonly r.txt; \
	if [ $$? -ne 0 ]; \
	then \
  		echo "rdonly failed" > log.txt; \
	fi; \
	rm -f r.txt

rdonlyF:
	./simpsh --rdonly doesNotExist.txt; \
	if [ $$? -ne 1 ]; \
	then \
  		echo "rdonlyF failed" > log.txt; \
	fi; \

wronly:
	echo "Some text" > w.txt; \
	./simpsh --wronly w.txt; \
	if [ $$? -ne 0 ]; \
	then \
  		echo "wronly failed" > log.txt; \
	fi; \
	rm -f w.txt

wronlyF:
	./simpsh --wronly doesNotExist.txt; \
	if [ $$? -ne 1 ]; \
	then \
  		echo "rdonlyF failed" > log.txt; \
	fi; \

verbose:
	echo "Some text" > w.txt; \
	echo "--wronly w.txt  " > ans.txt; \
	./simpsh --verbose --wronly w.txt > out.txt; \
	cmp -s ans.txt out.txt; \
	if [ $$? -ne 0 ]; \
	then \
  		echo "verbose failed" > log.txt; \
	fi; \
	rm -f ans.txt out.txt w.txt

command:
	echo "Foo" > n.txt; \
	echo "Bar" > a.txt; \
	echo "Rocks" > t.txt; \
	./simpsh --rdonly n.txt --wronly a.txt --wronly t.txt --command 0 1 2 cat > del.txt; \
	if [ $? -ne 0 ]; \
	then \
  		echo "command failed" > log.txt; \
	fi; \
	rm -f n.txt a.txt t.txt del.txt
