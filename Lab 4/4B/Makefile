default:
	gcc -Wextra -Wall -lmraa -lm -g -std=gnu11 lab4b.c -o lab4b

clean:
	rm -rf lab4b-005081232.tar.gz lab4b

dist:
	tar -zcvf lab4b-005081232.tar.gz lab4b.c Makefile README smoke.sh

check: default
	chmod +x smoke.sh
	./smoke.sh