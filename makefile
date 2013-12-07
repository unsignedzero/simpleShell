CC=gcc
CFLAGS=-O3 -Wall -Wextra

default: terminal.x

terminal.x:
	$(CC) $(CFLAGS) -o $@ terminal.h terminal.c -D DEBUG=4

test: clean terminal.x
	cat test | ./terminal.x

clean:
	rm -rvf *.x *.o *.out
