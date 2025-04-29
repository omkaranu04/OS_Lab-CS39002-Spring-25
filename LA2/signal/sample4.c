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
        signal(SIGINT, abc);
        printf("child sleeping for 2\n");
        sleep(2);
    }
    else
    {
        sleep(1);
        printf("Parent process\n");
        kill(pid, SIGINT);
        sleep(5);
        printf("Parent exiting\n");
    }
}
void abc()
{
    printf("Signal received by child\n");
}