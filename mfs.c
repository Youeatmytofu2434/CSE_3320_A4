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
    int hiddenAttrib;
    int readOnlyAttrib;
};

// List of files
struct directory_entry * directory_ptr;

// Metadata about the a particular file
struct inode * inode_array_ptr[NUM_INODES];

int findFreeDirectoryEntry( )
{
    int i;
    int retval = -1;
    for(i = 0; i < 128; i++)
    {
        if(directory_ptr[i].valid == 0)
        {
            retval = i;
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

// Find total free space in the directory
int df()
{
    //each unused block represents free space, and each block represents 8192 bytes of size
    //the program iterates through the memory blocks to determine which blocks are used or unused
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
    inode_array_ptr[inode_idx]->hiddenAttrib=0;
    inode_array_ptr[inode_idx]->readOnlyAttrib=0;

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

        // Read data from file and store them into our FS image
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

void list()
{
    //flag variable
    int noFilesFound = 1;
    //initialization
    int currentSize;
    time_t currentDate;
    char * currentName;

    int i=0;
    for(i=0; i<128; i++)
    {
        //if there is a file that is referenced by the index node
        if(inode_array_ptr[i]->valid==1 && inode_array_ptr[i]->hiddenAttrib!=1)
        {
            //changes flag variable to signify that a file is found
            noFilesFound=0;

            //sets variables to be printed later
            currentSize=inode_array_ptr[i]->size;
            currentDate=inode_array_ptr[i]->date;

            //allocates memory for the name variable to be printed later
            //j is used as a failsafe measure in case we need to locate the inode
            int j=0;
            for(j=0; j<128; j++)
            {
                //makes the name of the file into the variable currentName...
                //...when the directory has found the file with the correct inode ptr
                if(directory_ptr[j].inode_idx==i)
                {
                    currentName=(char *) malloc(strlen(directory_ptr[i].name));
                    strncpy(currentName,directory_ptr[i].name,strlen(directory_ptr[i].name));
                    
                    break;
                }
            }
            

            printf("%d %s %s \n",currentSize,currentName,ctime(&currentDate));
            //frees to prevent the accumulation of garbage
            free(currentName);
        }
    }
    if(noFilesFound==1)
    {
        //happens when no files are found
        printf("list: No files found.\n");
    }
}


void del( char * filename)
{
    //flag variable
    int noFilesFound = 1;

    int i=0;
    for(i=0; i<128; i++)
    {
        //if there is a file that is referenced by the index node
        if(inode_array_ptr[i]->valid==1 && inode_array_ptr[i]->readOnlyAttrib!=1)
        {
            if(strcmp(filename,directory_ptr[i].name)==0)
            {
                noFilesFound=0;
                inode_array_ptr[i]->valid=0;
                printf("File deleted.\n");
                break;
            }
        }
    }
    if(noFilesFound==1)
    {
        //happens when no files are found
        printf("del: File not found.\n");
    }
}

void undel( char * filename)
{
    //flag variable
    int noFilesFound = 1;

    int i=0;
    for(i=0; i<128; i++)
    {
        //if there is a file that is referenced by the index node
        if(inode_array_ptr[i]->valid==1)
        {
            if(strcmp(filename,directory_ptr[i].name)==0)
            {
                noFilesFound=0;
                inode_array_ptr[i]->valid=1;
                printf("File undeleted.\n");
                break;
            }
        }
    }
    if(noFilesFound==1)
    {
        //happens when no files are found
        printf("undel: Can not find the file.\n");
    }
}

void attrib(char * attribInput, char * filename)\
{
    //flag variable
    int noFilesFound = 1;

    int i=0;
    for(i=0; i<128; i++)
    {
        //if there is a file that is referenced by the index node
        if(inode_array_ptr[i]->valid==1)
        {
            if(strcmp(filename,directory_ptr[i].name)==0)
            {
                noFilesFound=0;
                if(attribInput[0]=='+')
                {
                    if(attribInput[1]=='r')
                    {
                        inode_array_ptr[i]->readOnlyAttrib=1;
                    }
                    else if(attribInput[1]=='h')
                    {
                        inode_array_ptr[i]->hiddenAttrib=1;
                    }
                    else
                    {
                        printf("DEBUG ERROR: Bad input\n",attribInput[1]);
                    }
                }
                else if(attribInput[0]=='-')
                {
                    if(attribInput[1]=='r')
                    {
                        inode_array_ptr[i]->readOnlyAttrib=0;
                    }
                    else if(attribInput[1]=='h')
                    {
                        inode_array_ptr[i]->hiddenAttrib=0;
                    }
                    else
                    {
                        printf("DEBUG ERROR: Bad input\n",attribInput[1]);
                    }
                }
                else
                {
                    printf("Error: You must use + or - for the first character.\n");
                }
                break;
            }
        }
    }
    if(noFilesFound==1)
    {
        //happens when no files are found
        printf("del: File not found.\n");
    }
}

int isFileExist( char * filename)
{
    int retval = -1;
    int i;
    // Check if file exists in FS
    for( i = 0; i < NUM_FILES; i++ )
    {
        // avoid segfault
        if ( directory_ptr[i].valid == 1 )
        {
            if( strcmp( directory_ptr[i].name, filename ) == 0 )
            {
                retval = i;
                break;
            }
        }
    }
    return retval;
}

void get( char * filename )
{
    // Case no files in the directory
    if( directory_ptr[0].valid == 0 )
    {
        printf( "get error: No file in dir!\n" );
        return;
    }

    int i;
    char * temp_fileName = filename;
    char * destination_file = "test.txt";
    
    int file_idx = isFileExist( temp_fileName );
    if( file_idx == -1 )
    {
        printf( "get error: %s not found!\n", temp_fileName );
        return;
    }

    // Found the file in FS
    //printf( "%s found!\n", temp_fileName );

    // Copy the file to local directory using old filename
    int offset = 0;
    int copy_size = inode_array_ptr[file_idx]->size;
    printf("file index: %d\n", file_idx);
    printf("Dir inode index: %d\n", directory_ptr[file_idx].inode_idx);
    printf("Data: %s\n", data_blocks[file_idx + 130]);
    
    
    // Check if file exists in local directory
    // if the file we're trying to write to exists in local directory
    // then remove it before creating a new one
    struct stat buf;
    int status = stat( destination_file, &buf );
    if( status != -1 )
    {
        int _ = remove( destination_file );
    }

    FILE * ofp = fopen( destination_file, "w" );
    while( copy_size >= BLOCK_SIZE )
    {
        fseek( ofp, offset, SEEK_SET );
        int bytes = fwrite( data_blocks[file_idx + 130], BLOCK_SIZE, 1, ofp );
        // TODO: find a way to write the contents of a file if it exceeds 
        //       BLOCK_SIZE
        if( bytes == 0 && !feof(ofp) )
        {
            printf("An error occured writing from FS.\n");
            return;
        }
        copy_size -= BLOCK_SIZE;
        offset += BLOCK_SIZE;
    }

    // Handles remainder
    if( copy_size > 0)
    {
        fseek( ofp, offset, SEEK_SET );
        int bytes = fwrite( data_blocks[file_idx + 130], copy_size, 1, ofp );
    }
    fclose(ofp);

    return;
}

int main()
{
  char * cmd_str = (char*) malloc( MAX_COMMAND_SIZE );

  /* Might need to put this in the createfs or open*/
  directory_ptr = malloc( NUM_FILES * sizeof(struct directory_entry));
  for(int i = 0; i < NUM_INODES; i++)
    inode_array_ptr[i] = malloc( sizeof( struct inode ) );
  /* Might need to put this in the createfs or open*/

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

    if( token[0] != NULL )
    {
        if ( strcmp( token[0] , "quit" ) == 0 )
        {
            free( working_root );
            break;
        }
        else if( strcmp( token[0] , "put" ) == 0 )
        {
            if( token[1] == NULL )
            {
                printf("Error: put <filename>\n");
            }
            else
            {
                //printf("%s\n", token[1] );
                put( token[1] );
            }
        }

        else if( strcmp( token[0] , "df") == 0 )
        {
            printf("%d bytes free\n",df());
        }
        else if( strcmp( token[0] , "list") == 0 )
        {
            list();
        }
        else if( strcmp( token[0] , "get" ) == 0 )
        {
            // Prints error if not enough arguments
            if( token[1] == NULL )
            {
                printf("Error: get <filename> (optional)<newfilename>\n");
            }
            else
            {
                get( token[1] );
            }
        }
        else if( strcmp( token[0] , "del") == 0 )
        {
            if( token[1] == NULL )
            {
                printf("Error: put <filename>\n");
            }
            else
            {
                //printf("%s\n", token[1] );
                del( token[1] );
            }
        }
        else if( strcmp( token[0] , "undel") == 0 )\
        {
            if( token[1] == NULL )
            {
                printf("Error: put <filename>\n");
            }
            else
            {
                //printf("%s\n", token[1] );
                undel( token[1] );
            }
        }
        else if( strcmp( token[0] , "attrib") == 0 )
        {
            //TODO: Error handling on no file later.
            attrib(token[1],token[2]);
        }
        /*
        
        
        else if( strcmp( token[0] , "open") == 0 )
        else if( strcmp( token[0] , "close") == 0 )
        else if( strcmp( token[0] , "createfs") == 0 )
        else if( strcmp( token[0] , "savefs") == 0 )
        
        */
    }

    free( working_root );
  }

  /* Might need to put this in the createfs or open*/
  free( directory_ptr );
  for(int i = 0; i < NUM_INODES; i++)
    free( inode_array_ptr[i]);
  /* Might need to put this in the createfs or open*/
  return 0;
}