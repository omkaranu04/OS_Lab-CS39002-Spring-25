#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main()
{
    printf("At the top\n");
    pid_t pid = fork();
    if (pid == 0)
    {
        sleep(5);
        printf("Child Process\n");
        // pass a manual status to child
        exit(5);
    }
    else
    {
        printf("Parent Waiting\n");
        int *status;
        waitpid(pid, status, 0);
        printf("Status of child: %d\n", WEXITSTATUS(*status));
    }
    return 0;
}