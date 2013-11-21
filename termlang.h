#ifndef TERMLANG_H
#define TERMLANG_H

/* Contained are all strings that will be printed/used by terminal.c
 *
 * This file can be copied and modified for different languages which
 * will not effect how terminal operates.
 */

#define TERMLANG "English"

///////////////////////////////////////////////////////////////////////////////
//// Global error strings
#define CLOSE_PIPE_FAIL "--sh: exiting shell. Can't close internal pipes"
#define COMMAND_NOT_FOUND "--sh: %s: command not found"
#define FORK_FAIL "--sh: can't fork program"
#define NO_COMMAND_ERROR "--sh: %s: command not found"
#define PFD_A_CLOSE_FAIL "--sh: Parent can't close internal pipe a"
#define PFD_B_CLOSE_FAIL "--sh: Parent can't close internal pipe b"
#define PFD_OPEN_ERROR "--sh: can't create internal pipes"
#define PFD_CLOSE_ERROR "--sh: can't close internal pipes"
#define STDIN_CLOSE_ERROR "--sh: can't redirect stdin"
#define STDIN_OPEN_ERROR "--sh: can't redirect stdin to a file"
#define STDOUT_CLOSE_ERROR "--sh: can't redirect stdout"
#define STDOUT_OPEN_ERROR "--sh: can't redirect stdout to a file"
#define UNEXPECTED_EOL "--sh: syntax error near unexpected token `newline'\n"

///////////////////////////////////////////////////////////////////////////////
//// Global debug strings
#define DEBUG_STRING_CUR_POS "--sh: current position %d\n"
#define DEBUG_STRING_CUR_INPUT "--sh: input is > %s\n"

#define DEBUG_STRING_NEWLINE_FOUND "--sh: 'newline' found\n"
#define DEBUG_STRING_PIPE_FOUND "\n--sh: pipe symbol '|' located\n\n"
#define DEBUG_STRING_LESS_FOUND "\n--sh: pipe symbol '<' located\n\n"
#define DEBUG_STRING_MORE_FOUND "\n--sh: pipe symbol '>' located\n\n"
#define DEBUG_STRING_NEWLINE_DELIMIT "--sh: parsing case a, string delimiter '\n"
#define DEBUG_STRING_QUOTE_DELIMIT "-sh: parsing case b, string delimiter \"'n"
#define DEBUG_STRING_SPACE_DELIMIT "--sh: parsing case c, string delimiter ' '\n"

#define DEBUG_STRING_REMOVING_WHITESPACE "--sh: removing whitespace\n"

#define DEBUG_STRING_FORK_START "--sh: Fork started\n"
#define DEBUG_STRING_NO_PIPE "--sh: Flags=NO_PIPE>\n\n"
#define DEBUG_STRING_PIPE_CONTA "--sh: Flags=PIPE_CONTA>\n\n"
#define DEBUG_STRING_PIPE_CONTB "--sh: Flags=PIPE_CONTB>\n\n"
#define DEBUG_STRING_PIPE_DRAINA "--sh: Flags=PIPE_DRAINA>\n\n"
#define DEBUG_STRING_PIPE_DRAINB "--sh: Flags=PIPE_DRAINB>\n\n"
#define DEBUG_STRING_FORK_END "--sh: ending proc_fork call\n"
#define DEBUG_STRING_PIPE_START "--sh Flags=PIPE_START>\n\n"

#define DEBUG_STRING_COMMAND_OUT_CALL "--sh: calling command_out [%d,%d]\n"
#define DEBUG_STRING_CUR_ARG "--sh: reading in argument %d\n"
#define DEBUG_STRING_CUR_CMD "--sh: reading in command \n"

#define DEBUG_STRING_CONTAIN_CTRL_D "--sh: input contains ^d. Exiting.\n\n"
#define DEBUG_STRING_NO_CONTAIN_CTRL_D "--sh: input does not contain ^d\n"

///////////////////////////////////////////////////////////////////////////////
//// Message output
#define ARGUMENT "Argument %d: %s\n"
#define COMMAND "Command: %s\n"
#define FILE_IO "File IO:\n"
#define INPUT_FILE "Input file: %s\n"
#define LANGUAGE_SELECT "Language selected %s\n"
#define OUTPUT_FILE "Output file: %s\n"

#define EXIT_STRING "exit"
#define PROMPT_STRING "> "
///////////////////////////////////////////////////////////////////////////////

#endif
