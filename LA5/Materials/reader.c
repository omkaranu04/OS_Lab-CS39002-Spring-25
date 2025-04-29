#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

int main()
{
    int shmid, f, key = 3, i, pid;
    char *ptr;
    shmid = shmget((key_t)key, 100, IPC_CREAT | 0666);
    ptr = (char *)shmat(shmid, NULL, 0);
    printf("shmid=%d ptr=%s\n", shmid, ptr);
    printf("\nstr %s\n", ptr);
}