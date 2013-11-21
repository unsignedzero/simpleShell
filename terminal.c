/* This code creates a terminal for the user.
 * Programs that are called by the terminal are handled
 * via fork, pipe and exec so that the terminal
 * lives when the command is executed. As this requires low-level system
 * functions, this might not work properly on Windows.
 *
 * Author:David Tran
 * Version: 1.3.0
 */

#ifndef TERMINAL_C
#define TERMINAL_C

#ifndef TERMINAL_H
 #include "terminal.h"
 #ifndef TERMINAL_H
  #error "Missing header file for terminal"
 #endif
#endif

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<stdbool.h>

#include<errno.h>
#include<fcntl.h>
//#include<unistd.h>
///////////////////////////////////////////////////////////////////////////////
int main(void){
  return shell();
}
///////////////////////////////////////////////////////////////////////////////
int shell(void){
  // This is the main function that will run the shell
  // Remember that output is 1 and input is 0

  debug printf( LANGUAGE_SELECT, TERMLANG );

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
  // Executes the fork exec and pipe commands

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
void check_ctrl_d( char* input_buffer, int length ){
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

#endif // TERMINAL_C
