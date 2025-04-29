#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <string.h>

#define MINUTE_SCALE 100000 // 100 ms per simulated minute

#define COOKS 2       // max number of cooks
#define WAITERS 5     // max number of waiters
#define CUSTOMERS 200 // max number of customers
#define TIMECLOSE 240 // time to close 3 pm

// INDICES IN SHARED MEMORY
#define TIME 0         // time
#define EMPTY_TABLES 1 // number of empty tables
#define WAITER_NEXT 2  // next waiter to serve
#define COOK_ORDERS 3  // number of orders in queue for cook

// for cook queue
#define COOK_QUEUE_FRONT 4
#define COOK_QUEUE_BACK 5
#define WORKING_COOKS 6

// 100 - 299 waiter U
#define FR_U 10    // marked by cook for ready order
#define PO_U 11    // used by customers for how many are waiting to place order
#define F_U 12     // marks the start of the queue
#define B_U 13     // marks the curr end of the queue
#define LEAVE_U 14 // indicate to leave to waiter U

// 300 - 499 waiter V
#define FR_V 15
#define PO_V 16
#define F_V 17
#define B_V 18
#define LEAVE_V 19

// 500 - 699 waiter W
#define FR_W 20
#define PO_W 21
#define F_W 22
#define B_W 23
#define LEAVE_W 24

// 700 - 899 waiter X
#define FR_X 25
#define PO_X 26
#define F_X 27
#define B_X 28
#define LEAVE_X 29

// 900 - 1099 waiter Y
#define FR_Y 30
#define PO_Y 31
#define F_Y 32
#define B_Y 33
#define LEAVE_Y 34

void init_SM()
{
    key_t key = ftok(".", 13);
    int shmid = shmget(key, 2048 * sizeof(int), 0777 | IPC_CREAT);
    if (shmid == -1)
    {
        perror("init_SM : shmget failed");
        exit(1);
    }

    int *shm = (int *)shmat(shmid, NULL, 0);
    if (shm == (int *)-1)
    {
        perror("init_SM : shmat failed");
        exit(1);
    }

    for (int i = 0; i < 2048; i++)
    {
        shm[i] = 0;
    }

    shm[TIME] = 0;          // init time to 0
    shm[EMPTY_TABLES] = 10; // init empty tables to 10
    shm[WAITER_NEXT] = 0;   // init waiter next to 0
    shm[COOK_ORDERS] = 0;   // init cook orders to 0

    shm[COOK_QUEUE_FRONT] = 1100;
    shm[COOK_QUEUE_BACK] = 1100;
    shm[WORKING_COOKS] = 2;

    shm[FR_U] = 0;
    shm[PO_U] = 0;
    shm[F_U] = 100;
    shm[B_U] = 100;

    shm[FR_V] = 0;
    shm[PO_V] = 0;
    shm[F_V] = 300;
    shm[B_V] = 300;

    shm[FR_W] = 0;
    shm[PO_W] = 0;
    shm[F_W] = 500;
    shm[B_W] = 500;

    shm[FR_X] = 0;
    shm[PO_X] = 0;
    shm[F_X] = 700;
    shm[B_X] = 700;

    shm[FR_Y] = 0;
    shm[PO_Y] = 0;
    shm[F_Y] = 900;
    shm[B_Y] = 900;

    shmdt(shm);
}

int *attach_shared_memory()
{
    key_t key = ftok(".", 13);
    int shmid = shmget(key, 2048 * sizeof(int), 0777);
    if (shmid == -1)
    {
        perror("attach_shared_memory : shmget failed");
        exit(1);
    }

    int *shm = (int *)shmat(shmid, NULL, 0);
    if (shm == (int *)-1)
    {
        perror("attach_shared_memory : shmat failed");
        exit(1);
    }

    return shm;
}

void delete_SM()
{
    key_t key = ftok(".", 13);
    int shmid = shmget(key, 2048 * sizeof(int), 0777);
    if (shmid == -1)
    {
        perror("delete_SM : shmget failed");
        exit(1);
    }

    if (shmctl(shmid, IPC_RMID, NULL) == -1)
    {
        perror("delete_SM : shmctl failed");
        exit(1);
    }
}

void init_semaphores()
{
    key_t key_mutex = ftok(".", 'A');
    key_t key_cook = ftok(".", 'B');
    key_t key_waiter = ftok(".", 'C');
    key_t key_customer = ftok(".", 'D');

    int sem_mutex = semget(key_mutex, 1, 0777 | IPC_CREAT);
    int sem_cook = semget(key_cook, 1, 0777 | IPC_CREAT);
    int sem_waiter = semget(key_waiter, WAITERS, 0777 | IPC_CREAT);
    int sem_customer = semget(key_customer, CUSTOMERS, 0777 | IPC_CREAT);
    if (sem_customer == -1 || sem_waiter == -1 || sem_cook == -1 || sem_mutex == -1)
    {
        perror("init semaphores : semget failed");
        exit(1);
    }

    semctl(sem_mutex, 0, SETVAL, 1);
    semctl(sem_cook, 0, SETVAL, 0);
    for (int i = 0; i < WAITERS; i++)
    {
        semctl(sem_waiter, i, SETVAL, 0);
    }
    for (int i = 0; i < CUSTOMERS; i++)
    {
        semctl(sem_customer, i, SETVAL, 0);
    }
}

void delete_semaphores()
{
    key_t key_mutex = ftok(".", 'A');
    key_t key_cook = ftok(".", 'B');
    key_t key_waiter = ftok(".", 'C');
    key_t key_customer = ftok(".", 'D');

    int sem_mutex = semget(key_mutex, 1, 0777);
    int sem_cook = semget(key_cook, 1, 0777);
    int sem_waiter = semget(key_waiter, WAITERS, 0777);
    int sem_customer = semget(key_customer, CUSTOMERS, 0777);

    if (semctl(sem_mutex, 0, IPC_RMID, NULL) == -1)
    {
        perror("delete_semaphores : semctl mutex failed");
        exit(1);
    }
    if (semctl(sem_cook, 0, IPC_RMID, NULL) == -1)
    {
        perror("delete_semaphores : semctl cook failed");
        exit(1);
    }
    if (semctl(sem_waiter, 0, IPC_RMID, NULL) == -1)
    {
        perror("delete_semaphores : semctl waiter failed");
        exit(1);
    }
    if (semctl(sem_customer, 0, IPC_RMID, NULL) == -1)
    {
        perror("delete_semaphores : semctl customer failed");
        exit(1);
    }
}

void WAIT(int semid, int sem_num)
{
    struct sembuf WAIT;
    WAIT.sem_num = sem_num;
    WAIT.sem_op = -1;
    WAIT.sem_flg = 0;
    if (semop(semid, &WAIT, 1) == -1)
    {
        perror("WAIT : semop failed");
        exit(1);
    }
}

void SIGNAL(int semid, int sem_num)
{
    struct sembuf SIGNAL;
    SIGNAL.sem_num = sem_num;
    SIGNAL.sem_op = 1;
    SIGNAL.sem_flg = 0;
    if (semop(semid, &SIGNAL, 1) == -1)
    {
        perror("SIGNAL : semop failed");
        exit(1);
    }
}

void handler(int signum)
{
    if (signum == SIGSEGV)
    {
        printf("Segmentation fault\n");
    }
    delete_SM();
    delete_semaphores();
    exit(0);
}

int *shm;
int sem_mutex, sem_cook, sem_waiter, sem_customer;
key_t key_mutex, key_cook, key_waiter, key_customer;

// COOK PRINTING FUNCTIONS
void print_cook_ready(int time, int cook)
{
    int hr = (time / 60 + 11) % 12;
    if (hr == 0)
        hr = 12;
    int min = time % 60;
    char m[3];
    strcpy(m, (time / 60 + 11) < 12 ? "am" : "pm");
    char cook_name = (cook == 0) ? 'C' : 'D';
    if (cook_name == 'C')
    {
        printf("[%02d:%02d %s] Cook %c: is ready\n", hr, min, m, cook_name);
    }
    else
    {
        printf("[%02d:%02d %s] \tCook %c: is ready\n", hr, min, m, cook_name);
    }
}
void print_preparing_order_stmt(int time, int cook, int waiter_id, int customer_id, int customer_count)
{
    int hr = (time / 60 + 11) % 12;
    if (hr == 0)
        hr = 12;
    int min = time % 60;
    char m[3];
    strcpy(m, (time / 60 + 11) < 12 ? "am" : "pm");
    char cook_name = (cook == 0) ? 'C' : 'D';
    char waiter_name = 'U' + waiter_id;
    if (cook_name == 'C')
    {
        printf("[%02d:%02d %s] Cook %c: Preparing order (Waiter %c, Customer %d, Count %d)\n", hr, min, m, cook_name, waiter_name, customer_id, customer_count);
    }
    else
    {
        printf("[%02d:%02d %s] \tCook %c: Preparing order (Waiter %c, Customer %d, Count %d)\n", hr, min, m, cook_name, waiter_name, customer_id, customer_count);
    }
}
void print_prepared_order_stmt(int time, int cook, int waiter_id, int customer_id, int customer_count)
{
    int hr = (time / 60 + 11) % 12;
    if (hr == 0)
        hr = 12;
    int min = time % 60;
    char m[3];
    strcpy(m, (time / 60 + 11) < 12 ? "am" : "pm");
    char cook_name = (cook == 0) ? 'C' : 'D';
    char waiter_name = 'U' + waiter_id;
    if (cook_name == 'C')
    {
        printf("[%02d:%02d %s] Cook %c: Prepared order (Waiter %c, Customer %d, Count %d)\n", hr, min, m, cook_name, waiter_name, customer_id, customer_count);
    }
    else
    {
        printf("[%02d:%02d %s] \tCook %c: Prepared order (Waiter %c, Customer %d, Count %d)\n", hr, min, m, cook_name, waiter_name, customer_id, customer_count);
    }
}
void print_cook_leaving(int time, int cook)
{
    int hr = (time / 60 + 11) % 12;
    if (hr == 0)
        hr = 12;
    int min = time % 60;
    char m[3];
    strcpy(m, (time / 60 + 11) < 12 ? "am" : "pm");
    char cook_name = (cook == 0) ? 'C' : 'D';
    if (cook_name == 'C')
    {
        printf("[%02d:%02d %s] Cook %c: Leaving\n", hr, min, m, cook_name);
    }
    else
    {
        printf("[%02d:%02d %s] \tCook %c: Leaving\n", hr, min, m, cook_name);
    }
}

void cmain(int cook)
{
    key_mutex = ftok(".", 'A');
    key_cook = ftok(".", 'B');
    key_waiter = ftok(".", 'C');
    key_customer = ftok(".", 'D');

    sem_mutex = semget(key_mutex, 1, 0777);
    sem_cook = semget(key_cook, 1, 0777);
    sem_waiter = semget(key_waiter, WAITERS, 0777);
    sem_customer = semget(key_customer, CUSTOMERS, 0777);

    shm = attach_shared_memory();

    WAIT(sem_mutex, 0);
    print_cook_ready(shm[TIME], cook);
    SIGNAL(sem_mutex, 0);

    while (1)
    {
        WAIT(sem_cook, 0);
        WAIT(sem_mutex, 0);

        int curr_time = shm[TIME];

        if (curr_time > TIMECLOSE && shm[COOK_ORDERS] == 0)
        {
            print_cook_leaving(curr_time, cook);
            shm[WORKING_COOKS]--;
            if (shm[WORKING_COOKS] == 0)
            {
                for (int i = 0; i < WAITERS; i++)
                {
                    if (i == 0)
                        shm[LEAVE_U] = 1;
                    if (i == 1)
                        shm[LEAVE_V] = 1;
                    if (i == 2)
                        shm[LEAVE_W] = 1;
                    if (i == 3)
                        shm[LEAVE_X] = 1;
                    if (i == 4)
                        shm[LEAVE_Y] = 1;
                    SIGNAL(sem_waiter, i);
                }
            }
            SIGNAL(sem_mutex, 0);
            break;
        }

        // take the order in front of the queue to process
        int waiter_id, customer_id, customer_count;
        int index = shm[COOK_QUEUE_FRONT];
        waiter_id = shm[index];
        customer_id = shm[index + 1];
        customer_count = shm[index + 2];

        // move the queue to the front
        shm[COOK_QUEUE_FRONT] += 3;
        shm[COOK_ORDERS]--;

        SIGNAL(sem_mutex, 0);

        print_preparing_order_stmt(curr_time, cook, waiter_id, customer_id, customer_count);

        // prepare the order
        int cooking_time = 5 * customer_count;
        usleep(MINUTE_SCALE * cooking_time);

        WAIT(sem_mutex, 0);
        int updated_time = curr_time + cooking_time;

        if (updated_time >= shm[TIME])
            shm[TIME] = updated_time;

        print_prepared_order_stmt(shm[TIME], cook, waiter_id, customer_id, customer_count);

        // signal the waiter that the food is ready
        if (waiter_id == 0)
            shm[FR_U] = customer_id;
        if (waiter_id == 1)
            shm[FR_V] = customer_id;
        if (waiter_id == 2)
            shm[FR_W] = customer_id;
        if (waiter_id == 3)
            shm[FR_X] = customer_id;
        if (waiter_id == 4)
            shm[FR_Y] = customer_id;
        SIGNAL(sem_waiter, waiter_id);

        if (shm[TIME] > TIMECLOSE && shm[COOK_ORDERS] == 0)
        {
            print_cook_leaving(shm[TIME], cook);
            shm[WORKING_COOKS]--;
            if (shm[WORKING_COOKS] == 0)
            {
                for (int i = 0; i < WAITERS; i++)
                {
                    if (i == 0)
                        shm[LEAVE_U] = 1;
                    if (i == 1)
                        shm[LEAVE_V] = 1;
                    if (i == 2)
                        shm[LEAVE_W] = 1;
                    if (i == 3)
                        shm[LEAVE_X] = 1;
                    if (i == 4)
                        shm[LEAVE_Y] = 1;
                    SIGNAL(sem_waiter, i);
                }
            }
            else
            {
                SIGNAL(sem_cook, 0);
            }
            SIGNAL(sem_mutex, 0);
            break;
        }
        SIGNAL(sem_mutex, 0);
    }
    shmdt(shm);
    exit(0);
}

int main(int argc, char const *argv[])
{
    init_SM();
    init_semaphores();

    // fork 2 cook processes
    for (int i = 0; i < COOKS; i++)
    {
        if (fork() == 0)
        {
            cmain(i);
            exit(0);
        }
    }

    signal(SIGINT, handler);
    signal(SIGSEGV, handler);

    // wait for all children to finish
    for (int i = 0; i < COOKS; i++)
    {
        wait(NULL);
    }

    return 0;
}