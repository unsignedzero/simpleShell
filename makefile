# Used to generate the libs and the executable.
# Created by David Tran (unsignedzero)
# Last Modified: 01-24-2014

CC=gcc
CCSPEEDFLAGS=-O3
CCDEBUGFLAGS=-Wall -Wextra
CCTESTFLAGS=-coverage -O0 -g

default: terminal.x

terminal.x:
	$(CC) $(CCDEBUGFLAGS) $(CCSPEEDFLAGS) -o $@ terminal.h terminal.c -D DEBUG=0

test: clean
	$(CC) $(CCDEBUGFLAGS) $(CCTESTFLAGS) -o terminal.x terminal.h terminal.c -D DEBUG=4
	cat test | ./terminal.x

lcov: clean test
	lcov --directory . --capture --output-file app.info
	genhtml --output-directory cov_htmp app.info

profile: clean test
	gcov terminal.c
	coveralls --exclude lib --exclude tests --verbose | grep 'coverage' | grep '1'

clean:
	rm -rvf *.x *.o *.out *.gcda *.gcov *.gcno
	rm -rvf app.info
	rm -rvf cov_htmp*
