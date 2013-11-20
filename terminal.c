/* This code acts as a terminal, in spirit.
 * It will parse the input text and display out the parsed text
 *
 * Supports, escape characters, strings
 * Author:David Tran
 * Version 0.5.0
 */

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<stdbool.h>

#define DEBUG 0
#define debug if (DEBUG)

#define BUFFER_SIZE 256

///////////////////////////////////////////////////////////////////////////////
//Function Prototypes

//Main Shell Function
int shell(void);

//Command Manipulation Functions
int command_out( char*, int, int, bool* );

//Support String Manipulation Functions
void check_ctrl_d( char*, int );
void purge_string( char*, int );
char remove_whitespace( char*, int*, int* );
///////////////////////////////////////////////////////////////////////////////
int main(void) {
  shell();
  return 0;
}
///////////////////////////////////////////////////////////////////////////////
int shell(void) {

  //This is the main function that will run the shell

  static enum FLAG_MODE {
    NO_PIPE   = 0, //No pipe
    PIPE_OUT  = 1, //Send output to file
    PIPE_RE   = 2, //Pipe to another program
    PIPE_IN   = 3  //Take in input
  }flag_mode;
  flag_mode = NO_PIPE;

  //Stores input and related states
  char input_buffer[BUFFER_SIZE];
  purge_string(input_buffer,BUFFER_SIZE);
  static char current_char;
  static int previous_end, current_pos;
  static bool is_command = true;

  //Loop
  while(1) {

    //Ask for input
    printf(">"); //Prompt
    fgets(input_buffer,sizeof(input_buffer),stdin);
    debug printf("You entered>%s\n",input_buffer);

    //Set the starting position for string parsing
    current_pos = 0;
    previous_end = -1;

    //Check for Ctrl+D
    check_ctrl_d(input_buffer,BUFFER_SIZE);

    //This outer loop will jump to the next argument
    while( (current_char = input_buffer[current_pos]) ) {

      //Remove leading white space
      current_char = remove_whitespace( input_buffer, &previous_end, &current_pos );

      //Break into 6 base cases
      //We first try to parse for a pipe BEFORE we parse the next input.
      //We will process pipes BEFORE we process the input, so we will know if
      //we have an input file or output file

      if ( current_char == '\n' ) {
        debug printf( "End of input\n" );
        //Check flags that are missing and DON'T RUN proc if they are on
        break;
      }

      if ( current_char == '|' ) {
        printf( "\npipe symbol\n\n" );
        is_command = true;
        previous_end += 1;
        current_pos  += 1;
        current_char = remove_whitespace( input_buffer, &previous_end, &current_pos );
        flag_mode = PIPE_RE;
        //Execute 'left' command BUT set up pipe before
      }

      if ( current_char == '<' ) {
        printf( "\npipe symbol\n\n" );
        flag_mode = PIPE_IN;
        previous_end += 1;
        current_pos  += 1;
        current_char = remove_whitespace( input_buffer, &previous_end, &current_pos );

        //Set flag, kill whitespace and go to next character
      }

      if ( current_char == '>' ) {
        printf( "\npipe symbol\n\n" );
        flag_mode = PIPE_OUT;
        previous_end += 1;
        current_pos  += 1;
        current_char = remove_whitespace( input_buffer, &previous_end, &current_pos );

        //Set flag, kill whitespace and go to next character
      }

      //Parse Input
      //Case A Start with '
      if ( current_char == '\'' ) {
        previous_end +=1;
        current_pos += 1;
        while( (current_char = input_buffer[current_pos]) != '\'' ) {
          if ( current_char == '\0' ) {
            printf( "Unexpected end of input" );
            exit ( -1 );
          }
          else if ( current_char == '\\' )
            current_pos += 1;
          current_pos += 1;
        }
        previous_end = command_out(input_buffer,previous_end,current_pos,&is_command);
      }

      //Case B start with "
      else if ( current_char == '\"' ) {
        previous_end +=1;
        current_pos +=1;
        while( (current_char = input_buffer[current_pos]) != '\"' ) {
          if ( current_char == '\0' ) {
            printf( "Unexpected end of input" );
            exit ( -1 );
          }
          else if ( current_char == '\\' )
            current_pos += 1;
          current_pos += 1;
        }
        previous_end = command_out(input_buffer,previous_end,current_pos,&is_command);
      }

      //Case C string
      else{
        while( (current_char = input_buffer[current_pos]) != ' ' ) {
          if ( current_char == '\0' || current_char == '\n' ||
               current_char == '\"' || current_char == '\'' )
            break;
          else if ( current_char == '\\' )
            current_pos += 1;
          current_pos += 1;
        }
        previous_end = command_out(input_buffer,previous_end,current_pos,&is_command);
      }

      current_pos +=1;
    }//End of current argument

    //Execute the "last" command currently in the cmd array
    is_command = true;
    purge_string(input_buffer,BUFFER_SIZE);
  }//End of Loop

  //End
  return 0;
}
///////////////////////////////////////////////////////////////////////////////
int command_out( char* input_buffer, int previous_end, int current_pos,
  bool *is_command ) {
  //Parses the input string and adds it into the cmd array

  static unsigned int argument_count = 0;
  static char temp_buffer[256];

  if ( *is_command ) {
    strncpy(temp_buffer,input_buffer+previous_end+1,current_pos-previous_end-1);
    temp_buffer[current_pos-previous_end-1] = '\0';
    printf( "command: %s\n",  temp_buffer );

    *is_command = false;
    argument_count = 0;
  }
  else {
    strncpy(temp_buffer,input_buffer+previous_end+1,current_pos-previous_end-1 );
    temp_buffer[current_pos-previous_end-1] = '\0';
    printf( "argument %u: %s\n", ++argument_count, temp_buffer);

  }
  return current_pos;
}
///////////////////////////////////////////////////////////////////////////////
void check_ctrl_d( char* input_buffer, int length ){
  static int i;
  //Checks for Ctrl+D in the input

  for( i = 0 ; i < length ; i++ ){
    if ( !input_buffer[i] ){
      printf ("^D detected in input. Exiting.\n");
      exit ( 0 );
    }
    if ( input_buffer[i] == '\n' ){
      debug printf( "Good input\n" );
      break;
    }
  }
}
///////////////////////////////////////////////////////////////////////////////
void purge_string( char* buffer, int length ) {
  //Fills the string with '\0'

  static int i;
  for( i = 0 ; i < length ; i++ )
    buffer[i] = '\0';
}
///////////////////////////////////////////////////////////////////////////////
char remove_whitespace( char* input_buffer, int* previous_end,
    int* current_pos) {
  //Filters leading whitespace for the next string
  static char current_char;
  if ( (current_char = input_buffer[*current_pos]) == ' ' ) {
    do{
      debug printf("Removing whitespace\n");
      (*current_pos) +=1;
      (*previous_end) +=1;
    }while ( (current_char = input_buffer[*current_pos]) == ' ' );
  }
  return current_char;
}
///////////////////////////////////////////////////////////////////////////////

