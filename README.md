# simpleShell #

This was a school project where students create a working shell
that supports piping (specifically |, < and >) and forking.

Currently now the main work is done in terminal.c but the messages that
it prints can be edited externally in termlang.h and compiled in, allowing
the user to translate or insert different messages as needed.

Created by David Tran (unsignedzero)

* * * *

## Version/Changelog #

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
