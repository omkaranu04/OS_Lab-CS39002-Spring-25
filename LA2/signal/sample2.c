#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

void abc(int);
int main()
{
    printf("Process ID = %d\n", getpid());
    signal(SIGINT, abc);
    signal(SIGQUIT, abc);
    for (;;)
        ;
}
void abc(int signo)
{
    printf("You have killed the process with signal ID = %d\n", signo);
    exit(0);
}