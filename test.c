#include <stdio.h>
#include <stdlib.h>

#define MAX_PLAYERS 10
struct Player 
{
    //char * name;
    int age;
};

struct Player * players[MAX_PLAYERS];

int main()
{
    int i;
    for(i = 0; i < MAX_PLAYERS; i++)
    {
        players[i] = malloc(sizeof(struct Player));
    }
    for(i = 0; i < MAX_PLAYERS; i++)
    {
        players[i]->age = 10;
    }
    for(i = 0; i < MAX_PLAYERS; i++)
    {
        printf("%d\n", players[i]->age);
    }
    return 0;
}