/* This code creates a terminal for the user.
 * Programs that are called by the terminal are handled
 * via "fork", "pipe" and "exec" so that the terminal
 * lives when the command is executed
 *
 * Author:David Tran
 * Version 1.1
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
#define debug if (DEBUG)

#define BUFFER_SIZE 1024
#define ARG_COUNT 21

enum pipe_flag {
  NO_PIPE     = 0, //No pipe
  PIPE_START  = 2, //Send into pipe (always pipea)
  PIPE_CONTA  = 4, //Draw from pipea and write to pipeb
  PIPE_DRAINA = 5, //Drain the pipea
  PIPE_CONTB  = 6, //Draw from pipeb and write to pipea
  PIPE_DRAINB = 7  //Drain from pipeb
};
///////////////////////////////////////////////////////////////////////////////
//Global error strings
#define UNEXPECTED_EOL "-sh: syntax error near unexpected token `newline'\n"

#define PFD_OPEN_ERROR "-sh: can't create internal pipes"

#define PFD_CLOSE_ERROR "-sh: can't close internal pipes"

#define STDIN_CLOSE_ERROR "-sh: can't redirect stdin"

#define STDIN_OPEN_ERROR "-sh: can't redirect stdin to a file"

#define STDOUT_CLOSE_ERROR "-sh: can't redirect stdout"

#define STDOUT_OPEN_ERROR "-sh: can't redirect stdout to a file"

#define FORK_FAIL "sh: can't fork program"

///////////////////////////////////////////////////////////////////////////////
//Function Prototypes

//Main Shell Function
int shell(void);

//Support String Manipulation Functions
void purge_string( char*, int );
char remove_whitespace( char*, int*, int* );
void check_ctrl_d( char[], int );

//Memory Management of char *[]
void null_run_array(char *[], int);
void free_run_array(char *[], int);

//Debug String Print
void show_state( char* [], int );
void show_io( char* []);

//Command Manipulation Functions
int command_out( char*, int, int, bool*, char*[], unsigned int* );
int modify_fin_fout( char*, int, int, bool , char*[] );

//Fork Function and support
void proc_fork( int* , int*,  char*[], char*[] , int, enum pipe_flag* );
void syserror( const char * );

///////////////////////////////////////////////////////////////////////////////

//Main
int main(void) {
  shell();
  return 0;
}

///////////////////////////////////////////////////////////////////////////////
int shell(void) {

  //This is the main function that will run the shell

  //Remember that output is 1 and input is 0

  //Stores the current pipe mode we are in
  static enum pipe_flag flag_mode;
  flag_mode = NO_PIPE;

  static bool set_file_input = false;
  static bool set_file_output = false;

  //Stores input
  static char input_buffer[BUFFER_SIZE];
  purge_string(input_buffer,BUFFER_SIZE);

  static char current_char;
  static int previous_end, current_pos;
  static bool is_command = true;

  //Command Run String Buffer
  unsigned int args_count = 0;
  char *run_buffer_array[ARG_COUNT];
  null_run_array(run_buffer_array,ARG_COUNT);
  char *io_pipe_array[2];
  null_run_array(io_pipe_array,2);

  // io_pipe_array
  // [0] will contain the input file  or 0 if there is none
  // [1] will contain the output file or 0 if there is none
  
  // run_buffer_array
  // [0..] will contain the args for argv

  // Creating pipes
  int pfda[2];
  if ( pipe(pfda) == -1 ) {
    syserror( PFD_OPEN_ERROR );
  }
  int pfdb[2];
  if ( pipe(pfdb) == -1 ) {
    syserror( PFD_OPEN_ERROR );
  }

  //Shell Loop
  while(1) {

    //Ask for input
    printf(">"); //Prompt
    fgets(input_buffer,sizeof(input_buffer),stdin);
    debug printf("--sh: input received > %s\n",input_buffer);

    //Set the starting position for string parsing
    current_pos = 0;
    previous_end = -1;

    //Ctrl-D Check
    check_ctrl_d( input_buffer, BUFFER_SIZE );

    //This outer loop will jump to the next argument
    while( (current_char = input_buffer[current_pos]) ) {

      //Remove leading white space
      current_char = remove_whitespace( input_buffer,
        &previous_end, &current_pos );

      //Break into the 6 base cases
      //We first try to parse for a pipe BEFORE we parse the next input.
      //We will process pipes BEFORE we process the input, so we will know if
      //we have an input file or output file

      if ( current_char == '\n' ) {
        debug printf( "--sh: 'newline' found\n" );
        //Check flags that are missing // No need
        break;
      }

      if ( current_char == '|' ) {
        debug printf( "\n--sh: pipe symbol '|' located\n\n" );
        previous_end += 1;
        current_pos  += 1;
        current_char = remove_whitespace( input_buffer,
          &previous_end, &current_pos );

        if ( flag_mode == NO_PIPE )
          flag_mode = PIPE_START;

        proc_fork(pfda, pfdb, run_buffer_array, io_pipe_array, args_count, &flag_mode);
        is_command = true;
      }

      else if ( current_char == '<' ) {
        debug printf( "\n--sh: pipe symbol '<' located\n\n" );
        //flag_mode = PIPE_IN;
        previous_end += 1;
        current_pos  += 1;
        current_char = remove_whitespace( input_buffer,
          &previous_end, &current_pos );

        //If we hit an end of line before we get the filename, assume bad input
        if ( current_char == '\n' ){
          printf( UNEXPECTED_EOL );
          break;
        }

        set_file_input = true;
      }

      else if ( current_char == '>' ) {
        debug printf( "\n--sh: pipe symbol '>' located\n\n" );
        //flag_mode = PIPE_OUT;
        previous_end += 1;
        current_pos  += 1;
        current_char = remove_whitespace( input_buffer,
          &previous_end, &current_pos );

        //If we hit an end of line before we get the filename, assume bad input
        if ( current_char == '\n' ){
          printf( UNEXPECTED_EOL );
          break;
        }

        set_file_output = true;
      }

      //Parse Input
      //Case A Start with '
      if ( current_char == '\'' ) {
        debug printf( "--sh: parsing case a, string delimiter '\n" );
        previous_end +=1;
        current_pos += 1;

        while( (current_char = input_buffer[current_pos]) != '\'' ) {
          if ( current_char == '\0' ) {
            printf( UNEXPECTED_EOL );
            break;
            //exit ( -1 );
          }
          else if ( current_char == '\\' )
            current_pos += 1;
          current_pos += 1;
        }

        //Check if we have a pipe before this string
        if ( !set_file_input && !set_file_output ){
          previous_end = command_out(input_buffer,previous_end,
            current_pos,&is_command,run_buffer_array,&args_count);
        }
        else{
          previous_end = modify_fin_fout(input_buffer,previous_end,
            current_pos,set_file_input,io_pipe_array);
          set_file_input = set_file_output = false;
        }
      }

      //Case B start with "
      else if ( current_char == '\"' ) {
        debug printf( "-sh: parsing case b, string delimiter \"'n" );
        previous_end +=1;
        current_pos +=1;

        while( (current_char = input_buffer[current_pos]) != '\"' ) {
          if ( current_char == '\0' ) {
            printf( UNEXPECTED_EOL );
            break;
            //exit ( -1 );
          }
          else if ( current_char == '\\' )
            current_pos += 1;
          current_pos += 1;
        }

        //Check if we have a pipe before this string
        if ( !set_file_input && !set_file_output ){
          previous_end = command_out(input_buffer,previous_end,
            current_pos,&is_command,run_buffer_array,&args_count);
        }
        else{
          previous_end = modify_fin_fout(input_buffer,previous_end,
            current_pos,set_file_input,io_pipe_array);
          set_file_input = set_file_output = false;
        }
      }

      //Case C string
      else{
        debug printf( "--sh: parsing case c, string delimiter ' '\n" );
        while( (current_char = input_buffer[current_pos]) != ' ' ) {
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

        //Check if we have a pipe before this string
        if ( !set_file_input && !set_file_output ){
          previous_end = command_out(input_buffer,previous_end,
            current_pos,&is_command,run_buffer_array,&args_count);
        }
        else{
          previous_end = modify_fin_fout(input_buffer,previous_end,
            current_pos,set_file_input,io_pipe_array);
          set_file_input = set_file_output = false;
        }
      }

      current_pos +=1;
      debug printf( "--sh: current position %d\n", current_pos );

    }//End of current argument

    //Execute the "last" command currently in the cmd array
    //Set the right end pipe
    if ( flag_mode == PIPE_CONTA || flag_mode == PIPE_START )
      flag_mode = PIPE_DRAINA;
    else if ( flag_mode == PIPE_CONTB )
      flag_mode = PIPE_DRAINB;

    proc_fork(pfda, pfdb, run_buffer_array, io_pipe_array, args_count, &flag_mode);
    is_command = true;
    purge_string(input_buffer,BUFFER_SIZE);

  }//End of Loop

  //Close File Descriptors
  if ( close(pfda[0]) == -1 || close(pfda[1]) == -1 ||
      close(pfdb[0]) == -1 || close(pfdb[1]) == -1 )
    syserror( "--sh: exiting shell. Can't close internal pipes" );

  //End
  return 0;
}

///////////////////////////////////////////////////////////////////////////////
int modify_fin_fout( char* input_buffer, int previous_end,
    int current_pos, bool mode, char* io_pipe_array[]) {
  //Adds the file name, due to a pipe character in front,
  //into the cmd array

  if ( mode ){
    io_pipe_array[0] = (char *) malloc(current_pos-previous_end);
    strncpy(io_pipe_array[0], //Input File
        input_buffer+previous_end+1,current_pos-previous_end);
    io_pipe_array[0][current_pos-previous_end-1] = '\0';
  }
  else{
    io_pipe_array[1] = (char *) malloc(current_pos-previous_end);
    strncpy(io_pipe_array[1], //Output File
        input_buffer+previous_end+1,current_pos-previous_end);
    io_pipe_array[1][current_pos-previous_end-1] = '\0';
  }

  return current_pos;
}

///////////////////////////////////////////////////////////////////////////////
int command_out( char* input_buffer, int previous_end,
    int current_pos, bool *is_command, char* run_buffer_array[],
    unsigned int* args ) {
  //Parses the input string and adds it into the cmd array

  static unsigned int args_count = 0;

  /*
     debug printf( "--sh:calling command_out [%d,%d]\n" ,
     previous_end, current_pos );
   */

  if ( *is_command ) {
    //debug printf( "--sh: reading in command \n" );
    //Clear Run Array
    //free_run_array(run_buffer_array,ARG_COUNT);
    args_count = 0;
    run_buffer_array[args_count] = (char *) malloc(current_pos-previous_end-1);

    strncpy(run_buffer_array[args_count],
        input_buffer+previous_end+1,current_pos-previous_end-1);
    run_buffer_array[args_count][current_pos-previous_end-1] = '\0';
    *is_command = false;
  }
  else {
    args_count+=1;
    run_buffer_array[args_count] = (char *) malloc(current_pos-previous_end-1);

    //debug printf( "--sh:reading in argument %d\n", args_count );
    strncpy(run_buffer_array[args_count],
        input_buffer+previous_end+1,current_pos-previous_end-1 );
    run_buffer_array[args_count][current_pos-previous_end-1] = '\0';
  }

  *args = args_count;

  return current_pos;
}

///////////////////////////////////////////////////////////////////////////////
char remove_whitespace( char* input_buffer, int* previous_end,
    int* current_pos) {
  //Filters leading whitespace for the next string

  static char current_char;

  if ( (current_char = input_buffer[*current_pos]) == ' ' ) {
    do{
      //debug printf("--sh: removing whitespace\n");
      (*current_pos) +=1;
      (*previous_end) +=1;
    }while ( (current_char = input_buffer[*current_pos]) == ' ' );
  }

  return current_char;
}

///////////////////////////////////////////////////////////////////////////////
void purge_string( char* buffer, int length ) {
  //Empties the string

  static int i;
  for( i = 0 ; i < length ; i++ )
    buffer[i] = '\0';

}

///////////////////////////////////////////////////////////////////////////////
void proc_fork( int* pfda, int* pfdb, char* run_buffer_array [],
    char* io_pipe_array [], int arg_count, enum pipe_flag* _flags) {
  //Executes the fork.exec and pipe commands

  static char concat_string_buffer[BUFFER_SIZE*2];

  int pid,status;
  debug show_state( run_buffer_array, arg_count );
  debug printf( "File IO:\n" );
  debug show_io( io_pipe_array );

  status = 0;

  debug printf("--sh: Fork started\n");

  //Setting up the pipes correctly in the call
  //No pipes in the call
  if ( (*_flags) == NO_PIPE ) {
    debug printf ( "--sh: Flags=NO_PIPE>\n\n" );
    switch ( pid = fork() ) {
      case -1:
        syserror( FORK_FAIL );
        break;
      case  0:
        //Stdin
        if ( io_pipe_array[0] != NULL ) {
          if ( close( 0 ) == -1 )
            syserror( STDIN_CLOSE_ERROR );
          status = open(io_pipe_array[0],  O_RDONLY | O_CREAT);
          if ( status < 0 )
            syserror( STDIN_OPEN_ERROR );
          dup(status);
        }

        //Stdout
        if ( io_pipe_array[1] != NULL ) {
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
        sprintf( concat_string_buffer ,
          "-sh: %s: command not found", run_buffer_array[0]);
        syserror( concat_string_buffer );
        break;
    }
  }
  //First pipe
  else if ( (*_flags) == PIPE_START ) {
    debug printf ( "--sh Flags=PIPE_START>\n\n" );
    switch ( pid = fork() ) {
      case -1:
        syserror( FORK_FAIL );
        break;
      case  0:
        //Stdin
        if ( io_pipe_array[0] != NULL ) {
          if ( close( 0 ) == -1 )
            syserror( STDIN_CLOSE_ERROR );
          status = open(io_pipe_array[0],O_RDONLY | O_CREAT);
          if ( status < 0 )
            syserror( STDIN_OPEN_ERROR );
          dup(status);
        }

        //Stdout
        if ( close ( 1 ) == -1 )
          syserror( STDOUT_CLOSE_ERROR );
        dup(pfda[1]);

        if ( close( pfda[0] ) == -1 || close( pfda[1] ) == -1 ||
            close( pfdb[0] ) == -1 || close( pfdb[1] ) == -1 )
          syserror( PFD_CLOSE_ERROR );

        execvp( run_buffer_array[0], (char** ) run_buffer_array );
        sprintf( concat_string_buffer ,
          "-sh: %s: command not found", run_buffer_array[0]);
        syserror( concat_string_buffer );
        break;
    }
  }
  //nth pipe
  else if ( (*_flags) == PIPE_CONTA ) {
    debug printf ( "--sh: Flags=PIPE_CONTA>\n\n" );
    switch ( pid = fork() ) {
      case -1:
        syserror( FORK_FAIL );
        break;
      case  0:
        //Stdin
        if ( close ( 0 ) == -1 )
          syserror( STDIN_CLOSE_ERROR );
        dup(pfda[0]);

        //Stdout
        if ( close ( 1 ) == -1 )
          syserror( STDOUT_CLOSE_ERROR );
        dup(pfdb[1]);

        if ( close( pfda[0] ) == -1 || close( pfda[1] ) == -1 ||
            close( pfdb[0] ) == -1 || close( pfdb[1] ) == -1 )
          syserror( PFD_CLOSE_ERROR );

        execvp( run_buffer_array[0], (char** ) run_buffer_array );
        sprintf( concat_string_buffer ,
          "-sh: %s: command not found", run_buffer_array[0]);
        syserror( concat_string_buffer );
        break;
    }
  }
  //nth pipe
  else if ( (*_flags) == PIPE_CONTB ) {
    debug printf ( "--sh: Flags=PIPE_COTNB>\n\n" );
    switch ( pid = fork() ) {
      case -1:
        syserror( FORK_FAIL );
        break;
      case  0:
        //Stdin
        if ( close ( 0 ) == -1 )
          syserror( STDIN_CLOSE_ERROR );
        dup(pfdb[0]);

        //Stdout
        if ( close ( 1 ) == -1 )
          syserror( STDOUT_CLOSE_ERROR );
        dup(pfda[1]);

        if ( close( pfda[0] ) == -1 || close( pfda[1] ) == -1 ||
            close( pfdb[0] ) == -1 || close( pfdb[1] ) == -1 )
          syserror( PFD_CLOSE_ERROR );

        execvp( run_buffer_array[0], (char** ) run_buffer_array );
        sprintf( concat_string_buffer ,
          "-sh: %s: command not found", run_buffer_array[0]);
        syserror( concat_string_buffer );
        break;
    }
  }
  //last pipe case a
  else if ( (*_flags) == PIPE_DRAINA ) {
    debug printf ( "--sh: Flags=PIPE_DRAINA>\n\n" );
    switch ( pid = fork() ) {
      case -1:
        syserror( FORK_FAIL );
        break;
      case  0:
        //Stdin
        if ( close ( 0 ) == -1 )
          syserror( STDIN_CLOSE_ERROR );
        dup(pfda[0]);

        //Stdout
        if ( io_pipe_array[1] != NULL ) {
          if ( close( 1 ) == -1 )
            syserror( STDOUT_CLOSE_ERROR );
          status = open(io_pipe_array[1],O_WRONLY | O_CREAT);
          if ( status < 0 )
            syserror( STDOUT_OPEN_ERROR );
          dup(status);
        }

        if ( close( pfda[0] ) == -1 || close( pfda[1] ) == -1 ||
            close( pfdb[0] ) == -1 || close( pfdb[1] ) == -1 )
          syserror( PFD_CLOSE_ERROR );

        execvp( run_buffer_array[0], (char** ) run_buffer_array );
        sprintf( concat_string_buffer ,
          "-sh: %s: command not found", run_buffer_array[0]);
        syserror( concat_string_buffer );
        break;
    }
  }
  //last pipe case b
  else if ( (*_flags) == PIPE_DRAINB ) {
    debug printf ( "--sh: Flags=PIPE_DRAINB>\n\n" );
    switch ( pid = fork() ) {
      case -1:
        syserror( FORK_FAIL );
        break;
      case  0:
        //Stdin
        if ( close ( 0 ) == -1 )
          syserror( STDIN_CLOSE_ERROR );
        dup(pfdb[0]);

        //Stdout
        if ( io_pipe_array[1] != NULL ) {
          if ( close( 1 ) == -1 )
            syserror( STDOUT_CLOSE_ERROR );
          status = open(io_pipe_array[1],O_WRONLY | O_CREAT);
          if ( status < 0 )
            syserror( STDOUT_OPEN_ERROR );
          dup(status);
        }
        if ( close( pfda[0] ) == -1 || close( pfda[1] ) == -1 ||
            close( pfdb[0] ) == -1 || close( pfdb[1] ) == -1 )
          syserror( PFD_CLOSE_ERROR );

        execvp( run_buffer_array[0], (char** ) run_buffer_array );
        sprintf( concat_string_buffer ,
          "-sh: %s: command not found", run_buffer_array[0]);
        syserror( concat_string_buffer );
        break;
    }
  }

  //Close the "used" pipe
  if ( (*_flags) == PIPE_DRAINA || (*_flags) == PIPE_CONTA ){
    if ( close(pfda[0]) == -1 || close(pfda[1]) == -1 )
      syserror( "-sh: Parent can't close internal pipe a" );
  }
  else if ( (*_flags) == PIPE_DRAINB || (*_flags) == PIPE_CONTB )
    if ( close(pfdb[0]) == -1 || close(pfdb[1]) == -1 )
      syserror( "-sh: Parent can't close internal pipe b" );

  //Parent Waiting
  while ( (pid = wait( (int *) 0 ) ) != -1 )
    ;

  //Switch pipe states and make a new pipe
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

  //Purge Run Buffer
  free_run_array(run_buffer_array,ARG_COUNT);
  free_run_array(io_pipe_array,2);
  debug printf("--sh: ending proc_fork call\n" );
}

///////////////////////////////////////////////////////////////////////////////
void syserror(const char *s) {
  //System error call
  extern int errno;

  fprintf( stderr, "%s\n" , s );
  fprintf( stderr, " (%s)\n", strerror(errno) );
  exit( 1 );
}

///////////////////////////////////////////////////////////////////////////////
void null_run_array(char * run_buffer_array[], int size){
  //Sets every element of a char* array to null
  static int i;
  for( i = 0 ; i < size ; i++ ){
    run_buffer_array[i] = NULL;
  }
}

///////////////////////////////////////////////////////////////////////////////
void free_run_array(char * run_buffer_array[], int size){
  //Frees every element of a char* [] and then sets them to null
  static int i;
  for( i = 0 ; i < size ; i++ ){
    free(run_buffer_array[i]);
    run_buffer_array[i] = NULL;
  }
}

///////////////////////////////////////////////////////////////////////////////
void show_state( char* run_buffer_array[], int args_count ) {
  static int i;
  //Prints everything in the cmd array

  printf( "Command: %s\n" , run_buffer_array[0] );
  for( i = 0 ; i < args_count ; i++ )
    printf( "Argument %d: %s\n" , i, run_buffer_array[1+i] );

}

///////////////////////////////////////////////////////////////////////////////
void show_io( char* io_pipe_array[]){
  printf("Input file is %s\n", io_pipe_array[0]);
  printf("Output file is %s\n", io_pipe_array[1]);
}

///////////////////////////////////////////////////////////////////////////////
void check_ctrl_d( char input_buffer[], int length ){
  static int i;
  //Checks for Ctrl+D in the input

  for( i = 0 ; i < length ; i++ ){
    if ( !input_buffer[i] ){
      debug printf ("--sh: input contains ^d. Exiting.\n" );
      printf( "\n" );
      exit ( 0 );
    }
    if ( input_buffer[i] == '\n' ){
      debug printf( "--sh: input does not contain ^d\n" );
      break;
    }
  }

}

///////////////////////////////////////////////////////////////////////////////

