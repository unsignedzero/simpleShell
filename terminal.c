/* This code creates a terminal for the user.
 * Programs that are called by the terminal are handled
 * via fork, pipe and exec so that the terminal
 * lives when the command is executed. As this requires low-level system
 * functions, this might not work properly on Windows.
 *
 * Author:David Tran
 * Version: 1.2.0
 */

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<stdbool.h>

#include<unistd.h>
#include<sys/wait.h>
#include<fcntl.h>
#include<errno.h>

#define DEBUG 0
#define debug if (DEBUG&1)
#define debug_verbose if (DEBUG&2)

#define BUFFER_SIZE 1024
#define ARG_COUNT 21

enum pipe_flag{
  NO_PIPE     = 0, // No pipe
  PIPE_START  = 2, // Send into pipe (always pipea)
  PIPE_CONTA  = 4, // Draw from pipea and write to pipeb
  PIPE_DRAINA = 5, // Drain the pipea
  PIPE_CONTB  = 6, // Draw from pipeb and write to pipea
  PIPE_DRAINB = 7  // Drain from pipeb
};
///////////////////////////////////////////////////////////////////////////////
// Global error strings
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

// Global debug strings
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

// Message output
#define ARGUMENT "Argument %d: %s\n"
#define COMMAND "Command: %s\n"
#define FILE_IO "File IO:\n"
#define INPUT_FILE "Input file: %s\n"
#define OUTPUT_FILE "Output file: %s\n"

#define EXIT_STRING "exit"
#define PROMPT_STRING "> "
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Function Prototypes

// Main Shell Function
int shell(void);

// Fork Function and support
void proc_fork( int* , int*,  char*[], char*[] , int, enum pipe_flag* );
void syserror( const char * );

// Command Manipulation Functions
int command_out( char*, int, int, bool*, char*[], unsigned int* );
int modify_fin_fout( char*, int, int, bool , char*[] );

// Support String Manipulation Functions
void check_ctrl_d( char[], int );
void purge_string( char*, int );
char remove_whitespace( char*, int*, int* );

// Memory Management of char *[]
void free_run_array(char *[], int);
void null_run_array(char *[], int);

// Debug String Print
void show_state( char* [], int );
void show_io( char* []);

///////////////////////////////////////////////////////////////////////////////
int main(void){
  shell();
  return 0;
}
///////////////////////////////////////////////////////////////////////////////
int shell(void){
  // This is the main function that will run the shell
  // Remember that output is 1 and input is 0

  static enum pipe_flag flag_mode;
  flag_mode = NO_PIPE;

  static bool set_file_input = false;
  static bool set_file_output = false;

  // Stores input
  static char input_buffer[BUFFER_SIZE];
  purge_string(input_buffer, BUFFER_SIZE);

  static char current_char;
  static int previous_end, current_pos;
  static bool is_command = true;

  // Command Run String Buffer
  unsigned int args_count = 0;
  char *run_buffer_array[ARG_COUNT];
  null_run_array(run_buffer_array, ARG_COUNT);
  char *io_pipe_array[2];
  null_run_array(io_pipe_array,2);

  // io_pipe_array
  // [0] will contain the input file  or 0 if there is none
  // [1] will contain the output file or 0 if there is none

  // run_buffer_array
  // [0..] will contain the args for argv

  // Creating pipes
  int pfda[2];
  int pfdb[2];

  if ( pipe(pfda) == -1 ){
    syserror( PFD_OPEN_ERROR );
  }
  if ( pipe(pfdb) == -1 ){
    syserror( PFD_OPEN_ERROR );
  }

  // Shell Loop
  while(1){

    // Ask for input
    printf( PROMPT_STRING );
    fgets(input_buffer, sizeof(input_buffer), stdin);
    debug printf( DEBUG_STRING_CUR_INPUT, input_buffer);

    if ( input_buffer[0] == '\n' )
      continue;

    // Set the starting position for string parsing
    current_pos = 0;
    previous_end = -1;

    check_ctrl_d( input_buffer, BUFFER_SIZE );

    // This outer loop will jump to the next argument
    while( (current_char = input_buffer[current_pos]) ){

      current_char = remove_whitespace( input_buffer,
        &previous_end, &current_pos );

      /* Break into the 6 base cases
       * We first try to parse for a pipe BEFORE we parse the next input.
       * We will process pipes BEFORE we process the input, so we will know if
       * we have an input file or output file
       */

      if ( current_char == '\n' ){
        debug printf( DEBUG_STRING_NEWLINE_FOUND );
        // Check flags that are missing // No need
        break;
      }

      if ( current_char == '|' ){
        debug printf( DEBUG_STRING_PIPE_FOUND );
        previous_end += 1;
        current_pos  += 1;
        current_char = remove_whitespace( input_buffer,
          &previous_end, &current_pos );

        if ( flag_mode == NO_PIPE )
          flag_mode = PIPE_START;

        proc_fork(pfda, pfdb, run_buffer_array, io_pipe_array, args_count, &flag_mode);
        is_command = true;
      }

      else if ( current_char == '<' ){
        debug printf( DEBUG_STRING_LESS_FOUND );
        previous_end += 1;
        current_pos  += 1;
        current_char = remove_whitespace( input_buffer,
          &previous_end, &current_pos );

        // If we hit an end of line before we get the filename, assume bad input
        if ( current_char == '\n' ){
          printf( UNEXPECTED_EOL );
          break;
        }

        set_file_input = true;
      }

      else if ( current_char == '>' ){
        debug printf( DEBUG_STRING_MORE_FOUND );
        previous_end += 1;
        current_pos  += 1;
        current_char = remove_whitespace( input_buffer,
          &previous_end, &current_pos );

        // If we hit an end of line before we get the filename, assume bad input
        if ( current_char == '\n' ){
          printf( UNEXPECTED_EOL );
          break;
        }

        set_file_output = true;
      }

      // Parse Input
      // Case A Start with '
      if ( current_char == '\'' ){
        debug printf( DEBUG_STRING_NEWLINE_DELIMIT );
        previous_end +=1;
        current_pos += 1;

        while( (current_char = input_buffer[current_pos]) != '\'' ){
          if ( current_char == '\0' ){
            printf( UNEXPECTED_EOL );
            break;
          }
          // Ignore escaped character
          else if ( current_char == '\\' ){
            current_pos += 1;
          }
          current_pos += 1;
        }

        // Check if we have a pipe before this string
        if ( !set_file_input && !set_file_output ){
          previous_end = command_out(input_buffer, previous_end,
            current_pos,&is_command, run_buffer_array,&args_count);
        }
        else{
          previous_end = modify_fin_fout(input_buffer, previous_end,
            current_pos, set_file_input, io_pipe_array);
          set_file_input = set_file_output = false;
        }
      }

      // Case B start with "
      else if ( current_char == '\"' ){
        debug printf( DEBUG_STRING_QUOTE_DELIMIT );
        previous_end +=1;
        current_pos +=1;

        while( (current_char = input_buffer[current_pos]) != '\"' ){
          if ( current_char == '\0' ){
            printf( UNEXPECTED_EOL );
            break;
          }
          else if ( current_char == '\\' )
            current_pos += 1;
          current_pos += 1;
        }

        // Check if we have a pipe before this string
        if ( !set_file_input && !set_file_output ){
          previous_end = command_out(input_buffer, previous_end,
            current_pos,&is_command, run_buffer_array,&args_count);
        }
        else{
          previous_end = modify_fin_fout(input_buffer, previous_end,
            current_pos, set_file_input, io_pipe_array);
          set_file_input = set_file_output = false;
        }
      }

      // Case C string
      else{
        debug printf( DEBUG_STRING_SPACE_DELIMIT );
        while( (current_char = input_buffer[current_pos]) != ' ' ){
          if ( current_char == '\0' || current_char == '\n' ||
               current_char == '\"' || current_char == '\'' ||
               current_char == '<'  || current_char == '>'  ||
               current_char == '|'
               )
            break;
          else if ( current_char == '\\' )
            current_pos += 1;
          current_pos += 1;
        }

        // Check if we have a pipe before this string
        if ( !set_file_input && !set_file_output ){
          previous_end = command_out(input_buffer, previous_end,
            current_pos,&is_command, run_buffer_array,&args_count);
        }
        else{
          previous_end = modify_fin_fout(input_buffer, previous_end,
            current_pos, set_file_input, io_pipe_array);
          set_file_input = set_file_output = false;
        }
      }

      current_pos +=1;
      debug printf( DEBUG_STRING_CUR_POS, current_pos );

    }// End of current argument

    // Set the right end pipe to the right terminal state
    // and start last command in queue
    if ( flag_mode == PIPE_CONTA || flag_mode == PIPE_START )
      flag_mode = PIPE_DRAINA;
    else if ( flag_mode == PIPE_CONTB )
      flag_mode = PIPE_DRAINB;

    proc_fork(pfda, pfdb, run_buffer_array, io_pipe_array, args_count, &flag_mode);
    is_command = true;
    purge_string(input_buffer, BUFFER_SIZE);

  }// End of Shell Loop

  // Close File Descriptors for parent
  if ( close(pfda[0]) == -1 || close(pfda[1]) == -1 ||
      close(pfdb[0]) == -1 || close(pfdb[1]) == -1 )
    syserror( CLOSE_PIPE_FAIL );

  // End
  return 0;
}
///////////////////////////////////////////////////////////////////////////////
void proc_fork( int* pfda, int* pfdb, char* run_buffer_array [],
    char* io_pipe_array [], int arg_count, enum pipe_flag* _flags){
  // Executes the fork.exec and pipe commands

  static char concat_string_buffer[BUFFER_SIZE*2];

  int pid, status;
  debug show_state( run_buffer_array, arg_count );
  debug printf( FILE_IO );
  debug show_io( io_pipe_array );

  status = 0;

  if ( strcmp(EXIT_STRING, run_buffer_array[0]) == 0 )
    exit(0);

  debug printf( DEBUG_STRING_FORK_START );

  // Setting up the pipes correctly in the call
  // No pipes in the call
  if ( (*_flags) == NO_PIPE ){
    debug printf ( DEBUG_STRING_NO_PIPE );
    switch ( pid = fork() ){
      case -1:
        syserror( FORK_FAIL );
        break;
      case  0:
        // Stdin
        if ( io_pipe_array[0] != NULL ){
          if ( close( 0 ) == -1 )
            syserror( STDIN_CLOSE_ERROR );
          status = open(io_pipe_array[0],  O_RDONLY | O_CREAT);
          if ( status < 0 )
            syserror( STDIN_OPEN_ERROR );
          dup(status);
        }

        // Stdout
        if ( io_pipe_array[1] != NULL ){
          if ( close( 1 ) == -1 )
            syserror( STDOUT_CLOSE_ERROR );
          status = open(io_pipe_array[1],  O_WRONLY | O_CREAT);
          if ( status < 0 )
            syserror( STDIN_OPEN_ERROR );
          dup(status);
        }

        if ( close( pfda[0] ) == -1 || close( pfda[1] ) == -1 ||
            close( pfdb[0] ) == -1 || close( pfdb[1] ) == -1 )
          syserror( PFD_CLOSE_ERROR );

        execvp( run_buffer_array[0], (char** ) run_buffer_array );
        sprintf( concat_string_buffer, COMMAND_NOT_FOUND, run_buffer_array[0]);
        syserror( concat_string_buffer );
        break;
    }
  }
  // First pipe
  else if ( (*_flags) == PIPE_START ){
    debug printf ( DEBUG_STRING_PIPE_START );
    switch ( pid = fork() ){
      case -1:
        syserror( FORK_FAIL );
        break;
      case  0:
        // Stdin
        if ( io_pipe_array[0] != NULL ){
          if ( close( 0 ) == -1 )
            syserror( STDIN_CLOSE_ERROR );
          status = open(io_pipe_array[0], O_RDONLY | O_CREAT);
          if ( status < 0 )
            syserror( STDIN_OPEN_ERROR );
          dup(status);
        }

        // Stdout
        if ( close( 1 ) == -1 )
          syserror( STDOUT_CLOSE_ERROR );
        dup(pfda[1]);

        if ( close( pfda[0] ) == -1 || close( pfda[1] ) == -1 ||
            close( pfdb[0] ) == -1 || close( pfdb[1] ) == -1 )
          syserror( PFD_CLOSE_ERROR );

        execvp( run_buffer_array[0], (char** ) run_buffer_array );
        sprintf( concat_string_buffer, COMMAND_NOT_FOUND, run_buffer_array[0]);
        syserror( concat_string_buffer );
        break;
    }
  }
  // nth pipe case a
  else if ( (*_flags) == PIPE_CONTA ){
    debug printf ( DEBUG_STRING_PIPE_CONTA );
    switch ( pid = fork() ){
      case -1:
        syserror( FORK_FAIL );
        break;
      case  0:
        // Stdin
        if ( close( 0 ) == -1 )
          syserror( STDIN_CLOSE_ERROR );
        dup(pfda[0]);

        // Stdout
        if ( close( 1 ) == -1 )
          syserror( STDOUT_CLOSE_ERROR );
        dup(pfdb[1]);

        if ( close( pfda[0] ) == -1 || close( pfda[1] ) == -1 ||
            close( pfdb[0] ) == -1 || close( pfdb[1] ) == -1 )
          syserror( PFD_CLOSE_ERROR );

        execvp( run_buffer_array[0], (char** ) run_buffer_array );
        sprintf( concat_string_buffer, COMMAND_NOT_FOUND, run_buffer_array[0]);
        syserror( concat_string_buffer );
        break;
    }
  }
  // nth pipe case b
  else if ( (*_flags) == PIPE_CONTB ){
    debug printf ( DEBUG_STRING_PIPE_CONTB );
    switch ( pid = fork() ){
      case -1:
        syserror( FORK_FAIL );
        break;
      case  0:
        // Stdin
        if ( close( 0 ) == -1 )
          syserror( STDIN_CLOSE_ERROR );
        dup(pfdb[0]);

        // Stdout
        if ( close( 1 ) == -1 )
          syserror( STDOUT_CLOSE_ERROR );
        dup(pfda[1]);

        if ( close( pfda[0] ) == -1 || close( pfda[1] ) == -1 ||
            close( pfdb[0] ) == -1 || close( pfdb[1] ) == -1 )
          syserror( PFD_CLOSE_ERROR );

        execvp( run_buffer_array[0], (char** ) run_buffer_array );
        sprintf( concat_string_buffer, COMMAND_NOT_FOUND, run_buffer_array[0]);
        syserror( concat_string_buffer );
        break;
    }
  }
  // last pipe case a
  else if ( (*_flags) == PIPE_DRAINA ){
    debug printf ( DEBUG_STRING_PIPE_DRAINA );
    switch ( pid = fork() ){
      case -1:
        syserror( FORK_FAIL );
        break;
      case  0:
        // Stdin
        if ( close( 0 ) == -1 )
          syserror( STDIN_CLOSE_ERROR );
        dup(pfda[0]);

        // Stdout
        if ( io_pipe_array[1] != NULL ){
          if ( close( 1 ) == -1 )
            syserror( STDOUT_CLOSE_ERROR );
          status = open(io_pipe_array[1], O_WRONLY | O_CREAT);
          if ( status < 0 )
            syserror( STDOUT_OPEN_ERROR );
          dup(status);
        }

        if ( close( pfda[0] ) == -1 || close( pfda[1] ) == -1 ||
            close( pfdb[0] ) == -1 || close( pfdb[1] ) == -1 )
          syserror( PFD_CLOSE_ERROR );

        execvp( run_buffer_array[0], (char** ) run_buffer_array );
        sprintf( concat_string_buffer, COMMAND_NOT_FOUND, run_buffer_array[0]);
        syserror( concat_string_buffer );
        break;
    }
  }
  // last pipe case b
  else if ( (*_flags) == PIPE_DRAINB ){
    debug printf ( DEBUG_STRING_PIPE_DRAINB );
    switch ( pid = fork() ){
      case -1:
        syserror( FORK_FAIL );
        break;
      case  0:
        // Stdin
        if ( close( 0 ) == -1 )
          syserror( STDIN_CLOSE_ERROR );
        dup(pfdb[0]);

        // Stdout
        if ( io_pipe_array[1] != NULL ){
          if ( close( 1 ) == -1 )
            syserror( STDOUT_CLOSE_ERROR );
          status = open(io_pipe_array[1], O_WRONLY | O_CREAT);
          if ( status < 0 )
            syserror( STDOUT_OPEN_ERROR );
          dup(status);
        }
        if ( close( pfda[0] ) == -1 || close( pfda[1] ) == -1 ||
            close( pfdb[0] ) == -1 || close( pfdb[1] ) == -1 )
          syserror( PFD_CLOSE_ERROR );

        execvp( run_buffer_array[0], (char** ) run_buffer_array );
        sprintf( concat_string_buffer, COMMAND_NOT_FOUND, run_buffer_array[0]);
        syserror( concat_string_buffer );
        break;
    }
  }

  // Close the used pipe from the parent
  if ( (*_flags) == PIPE_DRAINA || (*_flags) == PIPE_CONTA ){
    if ( close(pfda[0]) == -1 || close(pfda[1]) == -1 )
      syserror( PFD_A_CLOSE_FAIL );
  }
  else if ( (*_flags) == PIPE_DRAINB || (*_flags) == PIPE_CONTB )
    if ( close(pfdb[0]) == -1 || close(pfdb[1]) == -1 )
      syserror( PFD_B_CLOSE_FAIL );

  // Parent Waiting
  while ( (pid = wait( (int *) 0 ) ) != -1 )
    ;

  // Switch pipe states and make a new pipe
  if ( (*_flags) == PIPE_START )
    (*_flags) = PIPE_CONTA;
  else if ( (*_flags) == PIPE_CONTA ){
    (*_flags) = PIPE_CONTB;
    if ( pipe(pfda) == -1 )
      syserror( PFD_OPEN_ERROR );
  }
  else if ( (*_flags) == PIPE_CONTB ){
    (*_flags) = PIPE_CONTA;
    if ( pipe(pfdb) == -1 )
      syserror( PFD_OPEN_ERROR );
  }
  else if ( (*_flags) == PIPE_DRAINA){
    (*_flags) = NO_PIPE;
    if ( pipe(pfda) == -1 )
      syserror( PFD_OPEN_ERROR );
  }
  else if ( (*_flags) == PIPE_DRAINB){
    (*_flags) = NO_PIPE;
    if ( pipe(pfdb) == -1 )
      syserror( PFD_OPEN_ERROR );
  }

  free_run_array(run_buffer_array, ARG_COUNT);
  free_run_array(io_pipe_array,2);
  debug printf( DEBUG_STRING_FORK_END );
}
///////////////////////////////////////////////////////////////////////////////
void syserror(const char *s){
  // System error call
  extern int errno;

  fprintf( stderr, "%s\n", s );
  fprintf( stderr, " (%s)\n", strerror(errno) );
  exit( 1 );
}
///////////////////////////////////////////////////////////////////////////////
int command_out( char* input_buffer, int previous_end,
    int current_pos, bool *is_command, char* run_buffer_array[],
    unsigned int* args ){
  // Parses the input string and adds it into the cmd array
  static unsigned int args_count = 0;

  debug_verbose printf(
    DEBUG_STRING_COMMAND_OUT_CALL, previous_end, current_pos );

  if ( *is_command ){
    debug_verbose printf( DEBUG_STRING_CUR_CMD );
    // free_run_array(run_buffer_array, ARG_COUNT);
    args_count = 0;
    run_buffer_array[args_count] = (char *) malloc(current_pos-previous_end-1);

    strncpy(run_buffer_array[args_count],
        input_buffer+previous_end+1, current_pos-previous_end-1);
    run_buffer_array[args_count][current_pos-previous_end-1] = '\0';
    *is_command = false;
  }
  else{
    args_count+=1;
    debug_verbose printf( DEBUG_STRING_CUR_ARG, args_count );

    run_buffer_array[args_count] = (char *) malloc(current_pos-previous_end-1);

    strncpy(run_buffer_array[args_count],
        input_buffer+previous_end+1, current_pos-previous_end-1 );
    run_buffer_array[args_count][current_pos-previous_end-1] = '\0';
  }

  *args = args_count;

  return current_pos;
}
///////////////////////////////////////////////////////////////////////////////
int modify_fin_fout( char* input_buffer, int previous_end,
    int current_pos, bool mode, char* io_pipe_array[]){
  // Adds the file name, due to a pipe character in front,
  // into the cmd array

  if ( mode ){
    io_pipe_array[0] = (char *) malloc(current_pos-previous_end);
    strncpy(io_pipe_array[0], // Input File
        input_buffer+previous_end+1, current_pos-previous_end);
    io_pipe_array[0][current_pos-previous_end-1] = '\0';
  }
  else{
    io_pipe_array[1] = (char *) malloc(current_pos-previous_end);
    strncpy(io_pipe_array[1], // Output File
        input_buffer+previous_end+1, current_pos-previous_end);
    io_pipe_array[1][current_pos-previous_end-1] = '\0';
  }

  return current_pos;
}
///////////////////////////////////////////////////////////////////////////////
void check_ctrl_d( char input_buffer[], int length ){
  // Checks for Ctrl+D in the input
  static int i;

  for( i = 0 ; i < length ; i++ ){
    if ( !input_buffer[i] ){
      debug printf ( DEBUG_STRING_CONTAIN_CTRL_D );
      exit( 0 );
    }
    if ( input_buffer[i] == '\n' ){
      debug printf( DEBUG_STRING_NO_CONTAIN_CTRL_D );
      break;
    }
  }
}
///////////////////////////////////////////////////////////////////////////////
void purge_string( char* buffer, int length ){
  // Empties the string
  static int i;


  for( i = 0 ; i < length ; i++ )
    buffer[i] = '\0';
}
///////////////////////////////////////////////////////////////////////////////
char remove_whitespace( char* input_buffer, int* previous_end,
    int* current_pos){
  // Filters leading whitespace for the next string

  static char current_char;

  if ( (current_char = input_buffer[*current_pos]) == ' ' ){
    do{
      debug_verbose printf( DEBUG_STRING_REMOVING_WHITESPACE );
      (*current_pos) +=1;
      (*previous_end) +=1;
    } while ( (current_char = input_buffer[*current_pos]) == ' ' );
  }

  return current_char;
}
///////////////////////////////////////////////////////////////////////////////
void free_run_array(char * run_buffer_array[], int size){
  // Frees every element of a char* [] and then sets them to null
  static int i;

  for( i = 0 ; i < size ; i++ ){
    free(run_buffer_array[i]);
    run_buffer_array[i] = NULL;
  }
}
///////////////////////////////////////////////////////////////////////////////
void null_run_array(char * run_buffer_array[], int size){
  // Sets every element of a char* array to null
  static int i;

  for( i = 0 ; i < size ; i++ ){
    run_buffer_array[i] = NULL;
  }
}
///////////////////////////////////////////////////////////////////////////////
void show_io( char* io_pipe_array[]){
  // Prints the value in the io pipe array
  printf( INPUT_FILE, io_pipe_array[0]);
  printf( OUTPUT_FILE, io_pipe_array[1]);
}
///////////////////////////////////////////////////////////////////////////////
void show_state( char* run_buffer_array[], int args_count ){
  // Prints everything in the cmd array
  static int i;

  printf( COMMAND , run_buffer_array[0] );
  for( i = 0 ; i < args_count ; i++ )
    printf( ARGUMENT , i, run_buffer_array[1+i] );

}
///////////////////////////////////////////////////////////////////////////////

