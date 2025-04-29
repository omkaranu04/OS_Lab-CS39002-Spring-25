#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main()
{
    int pid = fork();
    if (pid == 0)
    {
        // int pid2 = fork();
        // if (pid2 == 0)
        // {
        //     printf("Child12: %d\n", getpgid(0));
        // }
        // else
        // {
        //     printf("Child1: %d\n", getpgid(0));
        // }
        fork();
        printf("Child1: %d\n", getpgid(0));
    }
    else
    {
        // int pid2 = fork();
        // if (pid2 == 0)
        // {
        //     printf("Child2: %d\n", getpgid(0));
        // }
        // else
        // {
        //     printf("Parent: %d\n", getpgid(0));
        // }
        fork();
        printf("Parent: %d\n", getpgid(0));
    }
}