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

    /*
    int index = 0;
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

        inode_array_ptr[inode_idx]->blocks[index] = block_index;
        used_blocks[block_index]=1;

        clearerr(ifp);
        copy_size   -=BLOCK_SIZE;
        offset      +=BLOCK_SIZE;
        index       ++;
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
        inode_array_ptr[inode_idx]->blocks[index] = block_index;
        used_blocks[block_index]=1;
        

        fseek(ifp, offset, SEEK_SET);
        int bytes=fread(data_blocks[block_index], copy_size, 1, ifp);
    }
    */
    
    int index = 0;
    int bytes = 0;
    while( copy_size > 0 )
    {
        fseek( ifp, bytes, SEEK_SET);
        if( copy_size < BLOCK_SIZE )
        {
            bytes = copy_size;
        }
        else
        {
            bytes = BLOCK_SIZE;
        }
        int temp = fread(data_blocks[block_index], bytes, 1, ifp);
        if( temp == 0 && !feof( ifp ) )
        {
            printf("An error occured reading from the input file.\n");
            return;
        }

        // Clear EOF flag
        clearerr( ifp );

        // save the blocks of data
        inode_array_ptr[inode_idx]->blocks[index] = block_index;
        //printf("Block Index: %d\n", block_index);
        //printf("Inode Index: %d\n", inode_idx);
        //printf("Index: %d\n", index);
        used_blocks[block_index] = 1;
        copy_size               -= bytes;
        offset                  += bytes;
        block_index              = findFreeBlock();
        index                   ++;
    }
    
    printf( "Size of %s: %d bytes\n", filename, inode_array_ptr[inode_idx]->size );
    printf( "Data Stored:\n" );
    for( int i = 130; i < NUM_BLOCKS; i++ )
    {
        if( used_blocks[i] == 1)
        {
            printf( "%d: %d bytes\n", i, strlen( data_blocks[i] ) );
            printf( "%s\n", data_blocks[i] );
        }

    }
    
    fclose( ifp );
    return;
}

void list()
{
    //flag variable
    int noFilesFound = 1;
    
    //initialization
    int currentSize;
    time_t currentDate;

    int i=0;
    /*
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
            int temp_size = strlen( currentName );
            memset( currentName, 0, temp_size );
            
        }
    }
    */

    for( i = 0; i < NUM_FILES; i++ )
    {
        if( directory_ptr[i].valid == 1 
            && inode_array_ptr[i]->hiddenAttrib != 1 )
        { 
            //char * currentName; 
            noFilesFound = 0;
            currentSize = inode_array_ptr[i]->size;
            currentDate = inode_array_ptr[i]->date;
            int temp_fileSize = strlen( directory_ptr[i].name );
            //currentName = (char*)malloc( temp_fileSize );
            char * currentName = directory_ptr[i].name;
            strncpy( currentName, directory_ptr[i].name, temp_fileSize );
            printf( "%d %s %s \n", currentSize, directory_ptr[i].name, ctime( &currentDate ) );
            //free(currentName);
        }
    }


    if(noFilesFound==1)
    {
        //happens when no files are found
        printf("list: No files found.\n");
    }
    
    return;
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
        printf("undel: Cannot find the file.\n");
    }
}

void attrib(char * attribInput, char * filename)
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
                        printf("Error: Invalid second token. You must use '+'/'-' followed by EITHER 'h' or 'r'. Example: \"+h\"\n");
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
                        printf("Error: Invalid second token. You must use '+'/'-' followed by EITHER 'h' or 'r'. Example: \"+h\"\n");
                    }
                }
                else
                {
                    printf("Error: Invalid first token. You must use '+'/'-' followed by EITHER 'h' or 'r'. Example: \"+h\"\n");
                }
                break;
            }
        }
    }
    if(noFilesFound==1)
    {
        //happens when no files are found
        printf("attrib: File not found.\n");
    }
}

void get( char * filename, char * output_fileName )
{
    // Case no files in the directory
    if( directory_ptr[0].valid == 0 )
    {
        printf( "get error: No file in dir!\n" );
        return;
    }

    int i;
    char * temp_fileName = filename;
    char * destination_file = output_fileName;
    
    int file_idx = isFileExist( temp_fileName );
    if( file_idx == -1 )
    {
        printf( "get error: %s not found!\n", temp_fileName );
        return;
    }

    // Found the file in FS
    //printf( "%s found!\n", temp_fileName );

    // Copy the file to local directory using old filename
    //printf("file index: %d\n", file_idx);
    //printf("Dir inode index: %d\n", directory_ptr[file_idx].inode_idx);
    //printf("Data: %s\n", data_blocks[file_idx + 130]);

    /*
    for( i = 0; i < MAX_BLOCKS_PER_FILE; i++ )
    {
        if( inode_array_ptr[i]->valid == 1)
        {
            printf("%d block:", i);
            for( int j = 0; j < MAX_BLOCKS_PER_FILE; j++)
                if( inode_array_ptr[i]->blocks[j] != 0)
                    printf("%d\n", inode_array_ptr[i]->blocks[j]);
        }
    }  
    */
    
    // Check if file exists in local directory
    // if the file we're trying to write to exists in local directory
    // then remove it before creating a new one
    struct stat buf;
    int status = stat( destination_file, &buf );
    if( status != -1 )
    {
        int _ = remove( destination_file );
    }

    
    int offset = 0;
    int copy_size = inode_array_ptr[file_idx]->size;
    FILE * ofp = fopen( destination_file, "w" );
    while( copy_size > 0 )
    {
        // If the remaining number of bytes we need to copy is less than BLOCK_SIZE then
        // only copy the amount that remains. If we copied BLOCK_SIZE number of bytes we'd
        // end up with garbage at the end of the file.
        int remainding_bytes;
        if( copy_size < BLOCK_SIZE )
        {
            remainding_bytes = copy_size;
        }
        else
        {
            remainding_bytes = BLOCK_SIZE;
        }
        
        int temp = 0;
        for( i = 0; i < MAX_BLOCKS_PER_FILE; i++ )
        {
            int data_location = inode_array_ptr[file_idx]->blocks[i];
            if( data_location != 0 && used_blocks[data_location] == 1)
            {
                // Write num_bytes number of bytes from our data array into our output file.
                temp = fwrite( data_blocks[data_location], remainding_bytes, 1, ofp );
            }
        }
        
        // Warns the users if an unexpected error occured while 
        // trying to write to a file.
        if(temp == 0 && !feof( ofp ) )
        {
            printf("An error occured writing to output file.\n");
            break;
        }

        // Clear EOF Flag
        clearerr( ofp );

        // Reduce the amount of bytes remaining to copy, increase the offset into the file
        // and increment the block_index to move us to the next data block.
        copy_size -= BLOCK_SIZE;
        offset    += BLOCK_SIZE;

        // Since we've copied from the point pointed to by our current file pointer, increment
        // offset number of bytes so we will be ready to copy to the next area of our output file.
        fseek( ofp, remainding_bytes, SEEK_SET );
    }
    
    
    fclose(ofp);
    
    return;
}

void createfs(char * filename)
{
    FILE *ofp;

    /*
    //creates a new name to create a file
    char * newFsname;
    char * dotBinExtension;
    newFsname=(char *)malloc(strlen(fsname)+4);
    dotBinExtension=(char *)malloc(4);
    strncpy(dotBinExtension,".bin",4);
    strncpy(newFsname,fsname,strlen(fsname));
    strncat(newFsname,dotBinExtension,4);
    free(dotBinExtension);
    //memory allocated with dotBin is no longer needed
    */

    ofp = fopen( filename, "wb" );
    int offset = 0;
    //creates a new file for writing
    fseek( ofp, offset, SEEK_SET );
    //iterates through inode list to physically write everything from there to a binary file
    int i = 0;
    
    fwrite(directory_ptr, sizeof(struct directory_entry), 1, ofp);

    for( i = 0; i < NUM_INODES; i++ )
    {
        fwrite(inode_array_ptr[i], sizeof(struct inode), 1, ofp);
    }
    
    for( i = 130; i < NUM_BLOCKS; i++ )
    {    
        //responsible for writing all blocks on the directory of mfs into disk
        fwrite(data_blocks[i], BLOCK_SIZE, 1, ofp);
    }


    fclose(ofp);
    //free(newFsname);
    //memory for newFsname is no longer needed
    return;
}

void open( char * filename)
{
    //TODO: FInd out if file is not found/exists

    
    //creates a new file for writing
    FILE *ofp;
    ofp = fopen( filename, "rb" );
    int offset = 0;
    //finds the beginning of the file
    //fseek( ofp, 0, SEEK_SET );
    fseek( ofp, offset, SEEK_SET );
    //iterates through inode list to physically write everything from there to a binary file
    int i=0;

    fread(directory_ptr, sizeof(struct directory_entry), 1, ofp);
    

    for(i=0; i<NUM_INODES; i++)
    {
        fread(inode_array_ptr[i], sizeof(struct inode), 1, ofp);
    }

    for(i=130; i<NUM_BLOCKS; i++)
    {   
        fread(data_blocks[i], BLOCK_SIZE, 1, ofp);
    }
    
    fclose(ofp);
    return;
}

void savefs(char * filename)
{
    FILE *ofp;
    ofp = fopen( filename, "wb" );
    int offset = 0;
    fseek( ofp, offset, SEEK_SET );
    //iterates through inode list to physically write everything from there to a binary file
    int i=0;

    fwrite(directory_ptr, sizeof(struct directory_entry), 1, ofp);

    for(i=0; i<NUM_INODES; i++)
    {
        //responsible for writing all inodes into disk
        fwrite(inode_array_ptr[i], sizeof(struct inode), 1, ofp);
    }
    
    for(i=130; i<NUM_BLOCKS; i++)
    {    
        //responsible for writing all blocks on the directory of mfs into disk
        fwrite(data_blocks[i], BLOCK_SIZE, 1, ofp);
    }
    
    fclose(ofp);
    return;
}

int main()
{
    /* Might need to put this in the createfs or open*/
    directory_ptr = malloc( NUM_FILES * sizeof(struct directory_entry));
    for(int i = 0; i < NUM_INODES; i++)
        inode_array_ptr[i] = malloc( sizeof( struct inode ) );
    /* Might need to put this in the createfs or open*/
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
            // not providing output file
            else if( token[2] == NULL ) 
            {
                get( token[1], token[1] );
            }
            else
            {
                get( token[1], token[2] );
            }
        }
        else if( strcmp( token[0] , "del") == 0 )
        {
            if( token[1] == NULL )
            {
                printf("Error: del <filename>\n");
            }
            else
            {
                //printf("%s\n", token[1] );
                del( token[1] );
            }
        }
        else if( strcmp( token[0] , "undel") == 0 )
        {
            if( token[1] == NULL )
            {
                printf("Error: undel <filename>\n");
            }
            else
            {
                //printf("%s\n", token[1] );
                undel( token[1] );
            }
        }
        else if( strcmp( token[0] , "attrib") == 0 )
        {
            //TODO: Error handling on no arguments later.
            attrib(token[1],token[2]);
        }
        else if( strcmp( token[0] , "createfs") == 0 )
        {
            //TODO: Error handling on no file name later
            createfs(token[1]);
        }
        else if( strcmp( token[0] , "open") == 0 )
        {
            //TODO: Error handling on no file name later
            open(token[1]);
        }
        
        else if( strcmp( token[0] , "savefs") == 0 )
        {
            savefs(token[1]);
        }
        /*
        
        
        else if( strcmp( token[0] , "close") == 0 )
        
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