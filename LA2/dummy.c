#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>

void handle_exit(int sig_no)
{
    // if SIGNINT is received, then exit the process
    exit(0);
}

int main(int argc, char const *argv[])
{
    // setting signal handler
    signal(SIGINT, handle_exit);
    // wait till you receive a signal
    while (1)
        pause();
    return 0;
}