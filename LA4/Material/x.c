#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>
#define MAX 20

int main()
{
    int p[2], pid;
    char *msg = "hello";
    char buf[MAX];
    pipe(p);
    pid = fork();
    if (pid == 0)
    {
        sleep(5);
        printf("child exiting\n");
    }
    else
    {
        close(p[1]);
        read(p[0], buf, MAX);
    }
    return 0;
}