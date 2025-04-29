#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
void abc();

int main()
{
    int pid = fork();
    if (pid == 0)
    {
        printf("child sleeping for 2\n");
        sleep(2);
    }
    else
    {
        signal(SIGCHLD, abc);
        sleep(3);
        printf("Parent process\n");
        sleep(5);
        printf("Parent exiting\n");
    }
}
void abc()
{
    printf("Child terminated\n");
}