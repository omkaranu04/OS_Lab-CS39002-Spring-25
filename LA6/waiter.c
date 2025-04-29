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

// WAITER PRINTING FUNCTIONS
void print_waiter_ready(int time, int waiter)
{
    int hr = (time / 60 + 11) % 12;
    if (hr == 0)
        hr = 12;
    int min = time % 60;
    char m[3];
    strcpy(m, (time / 60 + 11) < 12 ? "am" : "pm");
    char waiter_name = 'U' + waiter;
    printf("[%02d:%02d %s] %*sWaiter %c is ready\n", hr, min, m, waiter * 2, "", waiter_name);
}
void print_waiter_leaving(int time, int waiter)
{
    int hr = (time / 60 + 11) % 12;
    if (hr == 0)
        hr = 12;
    int min = time % 60;
    char m[3];
    strcpy(m, (time / 60 + 11) < 12 ? "am" : "pm");
    char waiter_name = 'U' + waiter;
    printf("[%02d:%02d %s] %*sWaiter %c leaving (no more customer to serve)\n", hr, min, m, waiter * 2, "", waiter_name);
}
void print_waiter_serving(int time, int waiter, int customer_id)
{
    int hr = (time / 60 + 11) % 12;
    if (hr == 0)
        hr = 12;
    int min = time % 60;
    char m[3];
    strcpy(m, (time / 60 + 11) < 12 ? "am" : "pm");
    char waiter_name = 'U' + waiter;
    printf("[%02d:%02d %s] %*sWaiter %c: Serving food to Customer %d\n", hr, min, m, waiter * 2, "", waiter_name, customer_id);
}
void printing_waiter_placing_order(int time, int waiter, int customer_id, int customer_count)
{
    int hr = (time / 60 + 11) % 12;
    if (hr == 0)
        hr = 12;
    int min = time % 60;
    char m[3];
    strcpy(m, (time / 60 + 11) < 12 ? "am" : "pm");
    char waiter_name = 'U' + waiter;
    printf("[%02d:%02d %s] %*sWaiter %c: Placing order for Customer %d (Count = %d)\n", hr, min, m, waiter * 2, "", waiter_name, customer_id, customer_count);
}
void extract(int waiter, int *waiter_fr, int *waiter_po, int *waiter_leave)
{
    if (waiter == 0)
    {
        *waiter_fr = shm[FR_U];
        *waiter_po = shm[PO_U];
        *waiter_leave = shm[LEAVE_U];
    }
    else if (waiter == 1)
    {
        *waiter_fr = shm[FR_V];
        *waiter_po = shm[PO_V];
        *waiter_leave = shm[LEAVE_V];
    }
    else if (waiter == 2)
    {
        *waiter_fr = shm[FR_W];
        *waiter_po = shm[PO_W];
        *waiter_leave = shm[LEAVE_W];
    }
    else if (waiter == 3)
    {
        *waiter_fr = shm[FR_X];
        *waiter_po = shm[PO_X];
        *waiter_leave = shm[LEAVE_X];
    }
    else if (waiter == 4)
    {
        *waiter_fr = shm[FR_Y];
        *waiter_po = shm[PO_Y];
        *waiter_leave = shm[LEAVE_Y];
    }
}
void place_order(int waiter, int *customer_id, int *customer_count)
{
    if (waiter == 0)
    {
        int index = shm[F_U]; // take from the front of the queue
        *customer_id = shm[index];
        *customer_count = shm[index + 1];
        shm[F_U] += 2; // move the queue front pointer
    }
    else if (waiter == 1)
    {
        int index = shm[F_V];
        *customer_id = shm[index];
        *customer_count = shm[index + 1];
        shm[F_V] += 2;
    }
    else if (waiter == 2)
    {
        int index = shm[F_W];
        *customer_id = shm[index];
        *customer_count = shm[index + 1];
        shm[F_W] += 2;
    }
    else if (waiter == 3)
    {
        int index = shm[F_X];
        *customer_id = shm[index];
        *customer_count = shm[index + 1];
        shm[F_X] += 2;
    }
    else if (waiter == 4)
    {
        int index = shm[F_Y];
        *customer_id = shm[index];
        *customer_count = shm[index + 1];
        shm[F_Y] += 2;
    }
}
void wmain(int waiter)
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
    print_waiter_ready(shm[TIME], waiter);
    SIGNAL(sem_mutex, 0);

    while (1)
    {
        WAIT(sem_waiter, waiter);
        WAIT(sem_mutex, 0);

        int waiter_fr, waiter_po, waiter_leave;
        extract(waiter, &waiter_fr, &waiter_po, &waiter_leave);

        if (waiter_po == 0 && waiter_leave == 1)
        {
            print_waiter_leaving(shm[TIME], waiter);
            SIGNAL(sem_mutex, 0);
            break;
        }

        if (waiter_fr != 0)
        {
            print_waiter_serving(shm[TIME], waiter, waiter_fr);
            if (waiter == 0)
            {
                shm[FR_U] = 0;
                shm[PO_U]--;
            }
            else if (waiter == 1)
            {
                shm[FR_V] = 0;
                shm[PO_V]--;
            }
            else if (waiter == 2)
            {
                shm[FR_W] = 0;
                shm[PO_W]--;
            }
            else if (waiter == 3)
            {
                shm[FR_X] = 0;
                shm[PO_X]--;
            }
            else if (waiter == 4)
            {
                shm[FR_Y] = 0;
                shm[PO_Y]--;
            }
            SIGNAL(sem_customer, waiter_fr);
            SIGNAL(sem_mutex, 0);
        }
        else
        {
            int customer_id, customer_count;

            place_order(waiter, &customer_id, &customer_count);

            int curr_time = shm[TIME];
            SIGNAL(sem_mutex, 0);

            usleep(MINUTE_SCALE * 1);

            WAIT(sem_mutex, 0);
            int updated_time = curr_time + 1;

            if (updated_time >= shm[TIME])
                shm[TIME] = updated_time;

            printing_waiter_placing_order(shm[TIME], waiter, customer_id, customer_count);

            // add the order to the cooking queue
            int index = shm[COOK_QUEUE_BACK];
            shm[index] = waiter;
            shm[index + 1] = customer_id;
            shm[index + 2] = customer_count;
            shm[COOK_QUEUE_BACK] += 3;
            shm[COOK_ORDERS]++;

            SIGNAL(sem_customer, customer_id);
            SIGNAL(sem_cook, 0);
            SIGNAL(sem_mutex, 0);
        }
    }
    shmdt(shm);
}

int main(int argc, char const *argv[])
{
    // fork all the waiters
    for (int i = 0; i < WAITERS; i++)
    {
        if (fork() == 0)
        {
            wmain(i);
            exit(0);
        }
    }

    // wait for all children to finish
    for (int i = 0; i < WAITERS; i++)
    {
        wait(NULL);
    }

    return 0;
}