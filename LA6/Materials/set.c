#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

int main()
{
    int val, semid;

    printf("Enter the initial semaphore value: ");
    scanf("%d", &val);

    semid = semget(20, 1, IPC_CREAT | 0666);
    if (semid == -1)
    {
        perror("semget");
        return 1;
    }

    if (semctl(semid, 0, SETVAL, val) == -1)
    {
        perror("semctl");
        return 1;
    }

    printf("Semaphore initialized with value: %d\n", val);
    return 0;
}
