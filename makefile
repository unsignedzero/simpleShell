CC=gcc
CFLAGS=-O3 -Wall -Wextra

default: terminal.x

terminal.x:
	$(CC) $(CFLAGS) -o $@ terminal.h terminal.c

clean:
	rm -rvf *.x *.o *.out
