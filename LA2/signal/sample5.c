#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

void abc();
void def();
int pid;

int main()
{
    pid = fork();
    if (pid == 0)
    {
        signal(SIGUSR2, abc);
        sleep(1);
        printf("Hello Parent\n");
        kill(getppid(), SIGUSR1);
        sleep(4);
    }
    else
    {
        signal(SIGUSR1, def);
        sleep(5);
    }
}
void abc()
{
    // sleep(2);
    printf("Bye Parent\n");
}
void def()
{
    printf("Hello Child\n");
    kill(pid, SIGUSR2);
    // mindicator 
}