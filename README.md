# simpleShell
[![Build status:Can't load apt.travis-ci.org](https://api.travis-ci.org/unsignedzero/simpleShell.png?branch=master)](https://travis-ci.org/unsignedzero/simpleShell)
[![Coverage Status](https://coveralls.io/repos/unsignedzero/simpleShell/badge.png?branch=master)](https://coveralls.io/r/unsignedzero/simpleShell?branch=master)
[![Bitdeli Badge](https://d2weczhvl823v0.cloudfront.net/unsignedzero/simpleshell/trend.png)](https://bitdeli.com/free "Bitdeli Badge")

A simpleShell that supports piping (specifically |, < and >) and forking.

Currently now the main work is done in terminal.c but the messages that
it prints can be edited externally in termlang.h and compiled in, allowing
the user to translate or insert different messages as needed.

Created by David Tran (unsignedzero)

* * * *

## Version/Changelog #

## 1.5.0 #
* Coverage works. Updating version number.
* Added more tests.
* Added code coverage to this codebase and added Coveralls.io badge.
* Added BitDeli badge.

## 1.4.0 #
* Test works. Updating version number.
* Added default file permissions on the open.
* Added more includes to resolve Travis-CI build issue.
* Output file that not be accessible. Touched first.
* Creating tests for Travis-CI.

## 1.3.0 #
* Termlang.h are all the constant strings allowing people to translate the
  tool, recompile it and have messages in different languages as needed.
* Terminal.h contains the main comments for the functions, how they operate
  and any assumptions.
* Broke terminal.c into three pieces and created a makefile

## 1.2.0 #
* Refactored mostly all strings as defines
* Cleaned up code
* Removed unneeded comments and commented out code
* Added exit into shell

## 1.1.0 #
* Cleaned code
* Removed execlp and used execvp

## 1.0.0 #
* First work version.
* Code need to be reordered but works just fine

## 0.5.0 #
* Parses user input and prints it out
* Code outline created
