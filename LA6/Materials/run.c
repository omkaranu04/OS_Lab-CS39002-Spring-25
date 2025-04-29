#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

int main()
{
    int semid;
    struct sembuf sop;

    semid = semget(20, 1, 0666);
    if (semid == -1)
    {
        perror("semget");
        return 1;
    }

    sop.sem_num = 0;
    sop.sem_op = 0;
    sop.sem_flg = 0;

    printf("Waiting for semaphore to become 0...\n");
    if (semop(semid, &sop, 1) == -1)
    {
        perror("semop");
        return 1;
    }

    printf("Semaphore operation completed successfully\n");
    return 0;
}
