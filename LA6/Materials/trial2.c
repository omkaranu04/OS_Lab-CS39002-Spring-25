#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <unistd.h>

int main(int argc, char const *argv[])
{
    int semid;
    int key = 16;
    semid = semget(key, 1, 0666 | IPC_CREAT);
    // semctl(semid, 0, SETVAL, 69);
    int retval = semctl(semid, 0, GETPID, 0);
    // printf("Semaphore Value : %d\n", retval);
    printf("The PID of last process : %d\n", retval);
    printf("Current PID : %d\n", getpid());
    return 0;
}