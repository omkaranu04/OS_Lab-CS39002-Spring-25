#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>

void process_follower(int id, int *M)
{
    printf("follower %d joins\n", id);
    srand(time(NULL) ^ (id * 232323));

    while (1)
    {
        while (M[2] != id && M[2] != -id)
            ;
        if (M[2] == -id)
        {
            printf("\tfollower %d leaves\n", id);
            if (id == M[0])
            {
                M[2] = 0;
            }
            else
            {
                M[2] = -(id + 1);
            }
            break;
        }
        M[3 + id] = rand() % 9 + 1;
        if (id == M[0])
        {
            M[2] = 0;
        }
        else
        {
            M[2] = id + 1;
        }
    }
}
int main(int argc, char const *argv[])
{
    int nf = 1;
    if (argc > 1)
    {
        nf = atoi(argv[1]);
        if (nf <= 0)
        {
            printf("Number of followers must be positive\n");
            return 1;
        }
    }

    key_t key = ftok("/", 16);
    if (key == -1)
    {
        perror("ftok error");
        return 1;
    }
    int shmid = shmget(key, 0, 0666);
    if (shmid == -1)
    {
        perror("shmget error: Leader not running");
        return 1;
    }
    int *M = (int *)shmat(shmid, 0, 0);
    if (M == (int *)-1)
    {
        perror("shmat error");
        return 1;
    }

    pid_t *children = malloc(nf * sizeof(pid_t));
    int creat_f = 0;

    for (int i = 0; i < nf; i++)
    {
        int foll_n = M[1] + 1;
        if (foll_n > M[0])
        {
            printf("Follower error: %d followers have already joined\n", M[0]);
            continue;
        }
        M[1] = foll_n;
        // usleep(1000);

        pid_t pid = fork();
        if (pid == 0)
        {
            // child process
            free(children);
            process_follower(foll_n, M);
            shmdt(M);
            exit(0); // terminate
        }
        else if (pid > 0)
        {
            children[creat_f] = pid;
            creat_f++;
        }
        else
        {
            perror("fork error");
        }
    }

    for (int i = 0; i < creat_f; i++)
    {
        waitpid(children[i], NULL, 0);
    }

    free(children);
    shmdt(M);
    return 0;
}