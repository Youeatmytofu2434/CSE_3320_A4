#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PLAYERS 10
struct Player 
{
    //char * name;
    int age;
};

struct Player * players[MAX_PLAYERS];

int main()
{
    int array[10];
    //memset( array, 0, sizeof(array) );
    int i;
    for( i = 0; i < 10; i++)
    {
        array[i] = i + 1;
    }
    for( i = 0; i < 10; i++)
    {
        printf("Hello: %d\n", array[i]);
    }
    return 0;
}