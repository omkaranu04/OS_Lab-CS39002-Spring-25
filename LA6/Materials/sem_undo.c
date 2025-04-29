#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/wait.h>

union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

int main()
{
    int semid, pid;
    struct sembuf sops;
    union semun arg;

    // Create a semaphore
    semid = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
    if (semid == -1)
    {
        perror("semget");
        exit(1);
    }

    // Initialize semaphore value to 1
    arg.val = 1;
    if (semctl(semid, 0, SETVAL, arg) == -1)
    {
        perror("semctl");
        exit(1);
    }

    pid = fork();
    if (pid < 0)
    {
        perror("fork");
        exit(1);
    }
    else if (pid == 0)
    { // Child process
        printf("Child: Attempting to acquire semaphore\n");
        sops.sem_num = 0;
        sops.sem_op = -1; // Decrement by 1 (acquire)
        sops.sem_flg = SEM_UNDO;

        if (semop(semid, &sops, 1) == -1)
        {
            perror("semop in child");
            exit(1);
        }

        printf("Child: Acquired semaphore\n");
        sleep(5); // Simulate some work
        printf("Child: Exiting without releasing semaphore\n");
        exit(0);
    }
    else
    {             // Parent process
        sleep(1); // Give child time to acquire semaphore
        printf("Parent: Attempting to acquire semaphore\n");
        sops.sem_num = 0;
        sops.sem_op = -1; // Decrement by 1 (acquire)
        sops.sem_flg = SEM_UNDO;

        if (semop(semid, &sops, 1) == -1)
        {
            perror("semop in parent");
        }
        else
        {
            printf("Parent: Acquired semaphore\n");
        }

        printf("Parent: Waiting for child to exit\n");
        wait(NULL);

        // Check semaphore value
        int semval = semctl(semid, 0, GETVAL);
        printf("Parent: Semaphore value after child exit: %d\n", semval);

        // Clean up
        semctl(semid, 0, IPC_RMID);
    }

    return 0;
}
