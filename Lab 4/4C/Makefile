default:
	gcc -Wextra -Wall -lmraa -lm -g -std=gnu11 lab4c_tcp.c -o lab4c_tcp
	gcc -Wextra -Wall -lmraa -lm -g -std=gnu11 -L/usr/local/ssl/lib  -lssl -lcrypto lab4c_tls.c -o lab4c_tls

clean:
	rm -rf lab4c-005081232.tar.gz lab4c_tls lab4c_tcp

dist:
	tar -zcvf lab4c-005081232.tar.gz lab4c_tcp.c lab4c_tls.c Makefile README