CC=gcc
CCSPEEDFLAGS=-O3
CCDEBUGFLAGS=-Wall -Wextra
CCTESTFLAGS=-fprofile-arcs -ftest-coverage -O0 -g

default: terminal.x

terminal.x:
	$(CC) $(CCSPEEDFLAGS) $(CCDEBUGFLAGS) -o $@ terminal.h terminal.c -D DEBUG=0

test: clean terminal.x
	$(CC) $(CCSPEEDFLAGS) $(CCDEBUGFLAGS) -o terminal.x terminal.h terminal.c -D DEBUG=4
	cat test | ./terminal.x

profile: clean
	$(CC) $(CCTESTFLAGS) -o terminal.x terminal.h terminal.c -D DEBUG=4
	cat test | ./terminal.x
	gcov terminal.c
	coveralls --verbose | grep 'coverage' | grep '1'

clean:
	rm -rvf *.x *.o *.out *.gcda *.gcov *.gcno
