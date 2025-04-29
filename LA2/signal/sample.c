#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

void abc();

int main()
{
    signal(SIGINT, abc);
    for (;;)
        ;
}

void abc()
{
    printf("You have pressed Ctrl+C\n");
    exit(0);
}