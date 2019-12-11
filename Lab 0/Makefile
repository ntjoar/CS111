# Name: Nathan Tjoar
# UID: 005081232
# Email: ntjoar@g.ucla.edu
default:
	gcc -Wall -Wextra -g -o lab0 lab0.c
clean:
	rm -f lab0 *.o *.txt *.tar.gz
dist:
	tar -czvf lab0-005081232.tar.gz lab0.c Makefile backtrace.png breakpoint.png README
check:default Beta Core IO Unregistered Validity Output Catcher Segfault
	if [ -s log.txt ]; \
	then \
	        cat log.txt; rm -f log.txt;\
	else \
	        echo "Passed Smoke Check"; \
	fi 
	

### Smoke tests ###
Beta:
	echo "foo bar" > file.txt; \
	./lab0 --input=file.txt --output=out.txt; \
	cmp file.txt out.txt; \
	if [[ $$? -ne 0 ]]; \
	then \
		echo "error copying from input file to output file: comparison failed" > log.txt; \
	fi; \
	rm -f file.txt out.txt 

Core:
	echo "123321" > in.txt; \
	./lab0 --input=in.txt --output=out.txt; \
	if [[ $$? -ne 0 ]]; \
	then \
		echo "error copying from input file to output file: exit code 0 not received" > log.txt; \
	fi; \
	rm -f in.txt out.txt;

IO:
	echo "goodInput" | ./lab0; \
	if [[ $$? -ne 0 ]]; \
	then \
		echo "error copying from stdin to stdout: exit code 0 not received" > log.txt; \
	fi;

Unregistered:
	echo "this input is bad" | ./lab0 --invalid; \
	if [[ $$? -ne 1 ]]; \
	then \
		echo "error handling invalid parameter: exit code 1 not received" > log.txt; \
	fi;

Validity:
	./lab0 --input=thisfiledoesnotexistsoithasareallylongtitle.txt; \
	if [[ $$? -ne 2 ]]; \
	then \
		echo "error handling invalid input file: exit code 2 not received" > log.txt; \
	fi;

Output:
	echo "123321" > tester.txt; \
	echo "123321" > out.txt; \
	chmod -w out.txt; \
	./lab0 --input=tester.txt --output=out.txt; \
	if [[ $$? -ne 3 ]]; \
	then \
		echo "error handling unwritable output file: exit code 3 not received" > log.txt; \
	fi; \
	rm -f tester.txt out.txt;

Catcher:
	./lab0 --segfault --catch; \
	if [[ $$? -ne 4 ]]; \
	then \
		echo "error handling catch: exit code 4 not received" > log.txt; \
	fi;

Segfault:
	./lab0 --segfault; \
	if [[ $$? -ne 139 ]]; \
	then \
		echo "error handling catch: error code 139 not received" > log.txt; \
	fi;
