#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <unistd.h>

union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf;
};

int main(int argc, char const *argv[])
{
    int semid;
    int key = ftok("/", 69);
    if (key == -1)
    {
        perror("ftok");
        exit(1);
    }

    ushort val[5] = {1, 2, 3, 4, 5};
    ushort retval[5];

    semid = semget(key, 5, 0666 | IPC_CREAT);
    if (semid == -1)
    {
        perror("semget");
        exit(1);
    }

    union semun arg;
    arg.array = val;
    if (semctl(semid, 0, SETALL, arg) == -1)
    {
        perror("semctl SETALL");
        exit(1);
    }

    arg.array = retval;
    if (semctl(semid, 0, GETALL, retval))
    {
        perror("semctl GETALL");
        exit(1);
    }

    for (int i = 0; i < 5; i++)
    {
        printf("Setval : %d Retval : %d\n", val[i], retval[i]);
    }
    printf("PID : %d\n", getpid());

    semctl(semid, 0, IPC_RMID, 0);
    return 0;
}