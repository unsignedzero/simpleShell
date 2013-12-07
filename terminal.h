/* This code creates a terminal for the user.
 * Programs that are called by the terminal are handled
 * via fork, pipe and exec so that the terminal
 * lives when the command is executed. As this requires low-level system
 * functions, this might not work properly on Windows.
 *
 * Author:David Tran
 * Version: 1.4.0
 * Last Modified: 12-06-2013
 */
#ifndef TERMINAL_H
#define TERMINAL_H

#ifndef DEBUG
 #define DEBUG 0
#endif
/* Specifics what debug information will be printed.
 * DEBUG &
 *       1 Prints out the cases shell hits and the current position of the
 *           cursor after it is moved as well as the pipes symbols detected.
 *           Will print out the run_buffer_array array before forking and
 *           the pipe mode.
 *       2 Prints the cursor positions that are set for command_out when
 *           cutting out the string
 *       4 Prints the buffer input and what function is executed. Used mainly
 *           for batch mode debugging. Prompt will look like the line is
 *           typed in.
 */

#define BUFFER_SIZE 1024
#define ARG_COUNT 21

#include<stdbool.h>

enum pipe_flag{
  NO_PIPE     = 0, // No pipe
  PIPE_START  = 2, // Send into pipe (always strt with pipea)
  PIPE_CONTA  = 4, // Draw from pipea and write to pipeb
  PIPE_DRAINA = 5, // Drain the pipea
  PIPE_CONTB  = 6, // Draw from pipeb and write to pipea
  PIPE_DRAINB = 7  // Drain from pipeb
};

///////////////////////////////////////////////////////////////////////////////
//// Main shell thread
int shell(void);
/* Processes the user input and passes the input to proc_fork to fork the
 * given command and its arguments. proc_fork is invoked each time a
 * pipe symbol (|, < and >) is spotted and once more for the final command.
 */

///////////////////////////////////////////////////////////////////////////////
//// Fork Function and support
void proc_fork( int* pfda, int* pfdb, char* run_buffer_array [],
    char* io_pipe_array [], int arg_count, enum pipe_flag* _flags);
/* This function processes the parsed array of run_buffer_array and creates
 * the fork as needed while updating the pipe status.
 *
 * pfda, pfdb are pipes that are created elsewhere that will be used, if
 *   the user uses pipes
 * run_buffer_array contains what will be argv for the child program invoked.
 *   The zeroth entry in the array is the name and the rest are args. The last
 *   entry MUST BE NULL.
 * io_pipe_array is an array containing file names that will be used for
 *   input or input. Mainly applies to the first and last command in the pipe
 *   sequence
 * arg_count denotes the number of entries contained in run_buffer_array. This
 *   is only used for debugging purposes as run_buffer_array should contain
 *   NULL as its last entry in the array.
 * pipe_flag denotes the current state of the system and how this function
 *   will ultimately work.
 */

void syserror(const char *s);
/* Should be only invoked from a child thread. Signifies that there is
 * an error and it should abort execution.
 *
 * s is an error message string that will be displayed before the child exits.
 */

///////////////////////////////////////////////////////////////////////////////
//// Command Manipulation Functions
int command_out( char* input_buffer, int previous_end,
    int current_pos, bool *is_command, char* run_buffer_array[],
    unsigned int* args );
/* This command "cuts" out the argument in the input_buffer, allocates the
 * correct amount of memory and loads it in run_buffer_array
 *
 * This assumes previous_end < current_pos and both are valid positions in
 *   input_buffer.
 * run_buffer_array must be allocated already if we are adding a new element
 *   args is the current number of non-null elements in run_buffer_array and
 *   must be less than ARG_COUNT - 1. Last element must be null.
 *
 * input_buffer is the input buffer that will be parsed
 * previous_end is the start position that will be cut
 * current_pos is the end position that will be cut
 * is_command states if the current string cut is a command that should
 *   create a new run_buffer_array or add to the end of the current one
 * run_buffer_array is the array that will receive the new string
 * args will contain the new args_count once the operation is completed
 *
 * This will current the new current_pos if it needs to be changed
 */

int modify_fin_fout( char* input_buffer, int previous_end,
    int current_pos, bool mode, char* io_pipe_array[]);
/* Redirects either the input or output to a file.
 *
 * This assumes previous_end < current_pos and both are valid positions in
 *   input_buffer.
 * Does not check if io_pipe_array already contains a value in the new field
 *   before allocating.
 *
 * input_buffer is the current input buffer
 * previous_end like command_out is the start string position that will be cut
 * current_pos is the end string position that will be cut
 * mode states if it is the input (true) or output (false) that will be modified
 * io_pipe_array is an array holding the current input/output file that will
 * be used
 *
 */

///////////////////////////////////////////////////////////////////////////////
//// Support String Manipulation Functions
void check_ctrl_d( char* input_buffer, int length );
/* Checks if the input_buffer contains a ^d, escape signal.
 *
 * length < BUFFER_SIZE
 *
 * input_buffer is the buffer to be checked.
 * length is the length of the buffer passed.
 */

void purge_string( char* buffer, int length );
/* Writes a string with null characters.
 *
 * length should be the length of buffer
 *
 * buffer is the buffer it will write \0 to
 * length is the length of said buffer
 */

char remove_whitespace( char* input_buffer, int* previous_end,
    int* current_pos);
/* Reads all whitespace and returns the first non-whitespace character
 * it sees.
 *
 * input_buffer is the buffer that will be processed
 * previous_end, current_pos are values of the current cursor position that
 *   will be moved.
 *
 * Returns the first non-whitespace character it finds. This may be '\0'
 */

///////////////////////////////////////////////////////////////////////////////
//// Memory Management of char *[]
void free_run_array(char * run_buffer_array[], int size);
/* Frees all elements in run_buffer_array and sets it to null
 *
 * run_buffer_array is the array that will be emptied
 * size should be length of run_buffer_array
 */

void null_run_array(char *[], int);
/* Nulls all elements in run_buffer_array. Does NOT free the array
 *
 * run_buffer_array is the array that will be nulled
 * size should be length of run_buffer_array
 */

void show_io( char* io_pipe_array[]);
/* Prints the current state of io_pipe_array.
 *
 * io_pipe_array must contain two elements.
 *
 * io_pipe_array the array that will be printed
 */

void show_state( char* run_buffer_array[], int args_count );
/* Prints the current state of the run_buffer_array
 *
 * args_count is the number of non-null elements in run_buffer_array and should
 *   be less than ARG_COUNT - 1.
 *
 * run_buffer_array is the array that will be printed
 * args_count is the number of element + 1, that should be printed.
 */
///////////////////////////////////////////////////////////////////////////////
#define debug if (DEBUG&1)
#define debug_batch if (DEBUG&4)
#define debug_verbose if (DEBUG&2)
///////////////////////////////////////////////////////////////////////////////

#ifndef TERMLANG_H
 #include "termlang.h"
 #ifndef TERMLANG_H
  #error "No language loaded"
 #endif
#endif

#endif // TERMINAL_H
