#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <stdlib.h>

struct databuf
{
    int nread;
    char buf[1000];
};

int main()
{
    struct databuf *ptr;
    int pid;

    // Create shared memory with correct size
    key_t key = 2000; // Generate a unique key
    int shmid = shmget(key, 1024, IPC_CREAT | 0666);
    if (shmid == -1)
    {
        perror("shmget failed");
        exit(1);
    }

    // Attach shared memory
    ptr = (struct databuf *)shmat(shmid, NULL, 0);
    if (ptr == (void *)-1)
    {
        perror("shmat failed");
        return 1;
    }

    printf("shmid=%d\n", shmid);

    pid = fork();
    if (pid == -1)
    {
        perror("fork failed");
        return 1;
    }
    else if (pid == 0)
    {
        // Child process
        // Read from standard input (up to 1000 bytes)
        ptr->nread = read(0, ptr->buf, sizeof(ptr->buf));
        if (ptr->nread < 0)
        {
            perror("read failed");
            exit(1);
        }
        exit(0);
    }
    else
    {
        // Parent process
        wait(NULL); // Parent waits for child
        printf("parent\n");

        // Write only the number of bytes read by the child process
        write(1, ptr->buf, ptr->nread);

        // Detach and remove shared memory
        if (shmdt(ptr) == -1)
        {
            perror("shmdt failed");
            return 1;
        }
        // if (shmctl(shmid, IPC_RMID, NULL) == -1)
        // {
        //     perror("shmctl failed");
        //     return 1;
        // }
    }

    return 0;
}
