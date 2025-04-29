#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

int shmid;

void custom_handler(int signum)
{
    printf("Interrupt signal received. Terminating ...\n");
    shmctl(shmid, IPC_RMID, NULL);
    exit(0);
}

int main(int argc, char const *argv[])
{

    // if the user presses ctrl + C in the terminal make sure that we clear and free the shared memory
    signal(SIGINT, custom_handler);

    int n = 10;
    if (argc > 1)
    {
        n = atoi(argv[1]);
        if (n <= 0 || n > 100)
        {
            printf("Number of followers must be between 1 and 100\n");
            return 1;
        }
    }
    // create shared memory segment
    key_t key = ftok("/", 16);
    if (key == -1)
    {
        perror("ftok error");
        return 1;
    }
    shmid = shmget(key, (4 + n) * sizeof(int), IPC_CREAT | IPC_EXCL | 0666);
    if (shmid == -1)
    {
        perror("shmget error");
        return 1;
    }
    int *M = (int *)shmat(shmid, 0, 0);
    if (M == (int *)-1)
    {
        perror("shmat error");
        shmctl(shmid, IPC_RMID, NULL); // remove if error
        return 1;
    }

    M[0] = n; // tot followers
    M[1] = 0; // curr followers
    M[2] = 0; // turn indicator

    printf("Waiting for the followers to join ... \n");

    while (M[1] < n)
        ;

    int hash_table[1000] = {0};
    int sum_count = 0;

    srand(time(NULL)); // random seed

    while (1)
    {
        M[2] = 1;
        while (M[2] != 0)
            ;

        M[3] = rand() % 99 + 1;

        int sum = M[3];
        for (int i = 1; i <= n; i++)
        {
            sum += M[3 + i];
        }
        printf("%d", M[3]);
        for (int i = 1; i <= n; i++)
        {
            printf(" + %d", M[3 + i]);
        }
        printf(" = %d\n", sum);

        int idx = sum % 1000;
        while (hash_table[idx] != 0 && hash_table[idx] != sum)
        {
            idx = (idx + 1) % 1000;
        }
        if (hash_table[idx] == sum)
        {
            M[2] = -1; // found duplicate
            break;
        }
        hash_table[idx] = sum;
        sum_count++;
    }
    while (M[2] != 0)
        ;

    printf("Duplicate sum found. Terminating ...\n");

    shmdt(M);
    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}