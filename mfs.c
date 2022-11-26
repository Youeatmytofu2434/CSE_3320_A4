// The MIT License (MIT)
// 
// Copyright (c) 2016, 2017 Trevor Bakker 
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <time.h>

#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255    // The maximum command-line size

#define MAX_NUM_ARGUMENTS 5     // Mav shell only supports five arguments

#define NUM_BLOCKS 4226
#define BLOCK_SIZE 8192
#define NUM_FILES 128
#define NUM_INODES 128
#define MAX_BLOCKS_PER_FILE 32

unsigned char data_blocks[NUM_BLOCKS][BLOCK_SIZE];
         int  used_blocks[NUM_BLOCKS];

struct directory_entry
{
    char * name;
    int valid;
    int inode_idx;
};

struct inode
{
    time_t date;
    int valid;
    int size;
    int blocks[MAX_BLOCKS_PER_FILE];
};

struct directory_entry * directory_ptr;
struct inode * inode_array_ptr[NUM_INODES];

int findFreeDirectoryEntry( )
{
    int i;
    int retval=-1;
    for(i=0; i<128; i++)
    {
        if(directory_ptr[i].valid ==0)
        {
            retval=i;
            break;
        }
    }
    return retval;
}

int findFreeInode( )
{
    int i;
    int retval=-1;
    for(i=0; i<128; i++)
    {
        if(inode_array_ptr[i]->valid==0)
        {
            retval=i;
            break;
        }
    }
}

int findFreeBlock()
{
    int i=0;
    int retval=-1;
    for(i=130; i<4226; i++)
    {
        if(used_blocks[i]==0)
        {
            retval=i;
            break;
        }
    }
    return retval;
}

int df()
{
    int count=0;
    int i=0;
    for(i=130; i<NUM_BLOCKS; i++)
    {
        if(used_blocks[i]==0)
        {
            count++;
        }
    }
    return count*BLOCK_SIZE;
}

void put(char * filename)
{
    struct stat buf;
    
    //does the file exist?
    int status = stat(filename, &buf);
    
    //if the file doesnt exists, bails out of function
    if(status==-1)
    {
        printf("Error: File not found.\n");
        return;
    }

    if(buf.st_size>df( ) )
    {
        printf("Error: Not enough room for file.\n");
        return;
    }

    int dir_idx = findFreeDirectoryEntry( );
    if(dir_idx==-1)
    {
        printf("Error: Not enough room for file.\n");
        return;
    }

    directory_ptr[dir_idx].valid=1;
    directory_ptr[dir_idx].name = (char *)malloc(strlen(filename));
    strncpy(directory_ptr[dir_idx].name,filename, strlen(filename));

    int inode_idx=findFreeInode( );
    if(inode_idx==-1)
    {
        printf("Error: No free inodes.\n");
        return;
    }

    directory_ptr[dir_idx].inode_idx=inode_idx;
    inode_array_ptr[inode_idx]->size=buf.st_size;
    inode_array_ptr[inode_idx]->date=time(NULL);
    inode_array_ptr[inode_idx]->valid=1;

    FILE * ifp = fopen(filename, "r");

    int copy_size=buf.st_size;
    int offset=0;

    int block_index=findFreeBlock( );
    if(block_index==-1)
    {
        printf("Error: Can't find free node blocks.\n");
        //TODO: Clean up a bunch of directory and inode stuff
        //This error means that there is a bug in the code and the files are messed up somehow
        return;
    }

    

    

    while(copy_size>=BLOCK_SIZE)
    {
        fseek(ifp, offset, SEEK_SET);

        int bytes=fread(data_blocks[block_index], BLOCK_SIZE, 1, ifp);

        if(bytes==0 && !feof(ifp) )
        {
            printf("An error occured reading from the input file.\n");
            return;
        }


        used_blocks[block_index]=1;
        //inode_array_ptr[inode_idx]->blocks[findFreeInode]

        clearerr(ifp);
        copy_size   -=BLOCK_SIZE;
        offset      +=BLOCK_SIZE;
    }
    //handles the remainder
    if(copy_size>0)
    {
        int block_index=findFreeBlock( );
        if(block_index==-1)
        {
            printf("Error: Can't find free block.\n");
            //cleanup a bunch of directory and inode stuff
            return;
        }

        used_blocks[block_index]=1;

        // inode_array_ptr[inode_idx]->blocks[findFreeInode()]


        fseek(ifp, offset, SEEK_SET);
        int bytes=fread(data_blocks[block_index], copy_size, 1, ifp);
    }

    return;
}

int main()
{

  char * cmd_str = (char*) malloc( MAX_COMMAND_SIZE );

  while( 1 )
  {
    // Print out the mfs prompt
    printf ("mfs> ");

    // Read the command from the commandline.  The
    // maximum command that will be read is MAX_COMMAND_SIZE
    // This while command will wait here until the user
    // inputs something since fgets returns NULL when there
    // is no input
    while( !fgets (cmd_str, MAX_COMMAND_SIZE, stdin) );

    /* Parse input */
    char *token[MAX_NUM_ARGUMENTS];

    int   token_count = 0;                                 
                                                           
    // Pointer to point to the token
    // parsed by strsep
    char *arg_ptr;                                         
                                                           
    char *working_str  = strdup( cmd_str );                

    // we are going to move the working_str pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end
    char *working_root = working_str;

    // Tokenize the input stringswith whitespace used as the delimiter
    while ( ( (arg_ptr = strsep(&working_str, WHITESPACE ) ) != NULL) && 
              (token_count<MAX_NUM_ARGUMENTS))
    {
      token[token_count] = strndup( arg_ptr, MAX_COMMAND_SIZE );
      if( strlen( token[token_count] ) == 0 )
      {
        token[token_count] = NULL;
      }
        token_count++;
    }

    // Now print the tokenized input as a debug check
    // \TODO Remove this code and replace with your shell functionality

    int token_index  = 0;
    for( token_index = 0; token_index < token_count; token_index ++ ) 
    {
      printf("token[%d] = %s\n", token_index, token[token_index] );  
    }

    free( working_root );

  }
  return 0;
}