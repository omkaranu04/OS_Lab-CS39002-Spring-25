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

// CUSTOMER PRINTING FUNCTIONS
void print_customer_arrival(int time, int customer_id, int customer_count)
{
    int hr = (time / 60 + 11) % 12;
    if (hr == 0)
        hr = 12;
    int min = time % 60;
    char m[3];
    strcpy(m, (time / 60 + 11) < 12 ? "am" : "pm");
    printf("[%02d:%02d %s] Customer %d arrives (count = %d)\n", hr, min, m, customer_id, customer_count);
}
void print_customer_leaves_late(int time, int customer_id)
{
    int hr = (time / 60 + 11) % 12;
    if (hr == 0)
        hr = 12;
    int min = time % 60;
    char m[3];
    strcpy(m, (time / 60 + 11) < 12 ? "am" : "pm");
    printf("[%02d:%02d %s] \t\t\t\tCustomer %d leaves (late arrival)\n", hr, min, m, customer_id);
}
void print_customer_leaves_table(int time, int customer_id)
{
    int hr = (time / 60 + 11) % 12;
    if (hr == 0)
        hr = 12;
    int min = time % 60;
    char m[3];
    strcpy(m, (time / 60 + 11) < 12 ? "am" : "pm");
    printf("[%02d:%02d %s] \t\t\t\tCustomer %d leaves (no empty table)\n", hr, min, m, customer_id);
}
void print_finish_eating(int time, int customer_id)
{
    int hr = (time / 60 + 11) % 12;
    if (hr == 0)
        hr = 12;
    int min = time % 60;
    char m[3];
    strcpy(m, (time / 60 + 11) < 12 ? "am" : "pm");
    printf("[%02d:%02d %s] \t\t\tCustomer %d finishes eating and leaves\n", hr, min, m, customer_id);
}
void print_order_placed(int time, int customer_id, int waiter_id)
{
    int hr = (time / 60 + 11) % 12;
    if (hr == 0)
        hr = 12;
    int min = time % 60;
    char m[3];
    strcpy(m, (time / 60 + 11) < 12 ? "am" : "pm");
    char waiter_name = 'U' + waiter_id;
    printf("[%02d:%02d %s] \tCustomer %d: Order placed to Waiter %c\n", hr, min, m, customer_id, waiter_name);
}
void print_gets_food(int time, int customer_id, int wait_time)
{
    int hr = (time / 60 + 11) % 12;
    if (hr == 0)
        hr = 12;
    int min = time % 60;
    char m[3];
    strcpy(m, (time / 60 + 11) < 12 ? "am" : "pm");
    printf("[%02d:%02d %s] \t\t\tCustomer %d gets food [Waiting time = %d]\n", hr, min, m, customer_id, wait_time);
}

void simulate_eating(int customer_id, int waiter_to_serve, int arrival_time)
{
    WAIT(sem_customer, customer_id);
    WAIT(sem_mutex, 0);
    int curr_time = shm[TIME];

    print_order_placed(curr_time, customer_id, waiter_to_serve);
    SIGNAL(sem_mutex, 0);

    WAIT(sem_customer, customer_id);
    WAIT(sem_mutex, 0);
    int served_time = shm[TIME];

    print_gets_food(served_time, customer_id, served_time - arrival_time);
    SIGNAL(sem_mutex, 0);

    usleep(MINUTE_SCALE * 30);

    WAIT(sem_mutex, 0);
    shm[EMPTY_TABLES]++; // customer leaves, frees up a table
    int updated_time = served_time + 30;

    shm[TIME] = updated_time;

    print_finish_eating(shm[TIME], customer_id);
    SIGNAL(sem_mutex, 0);
}

void add_to_waiter_queue(int waiter_to_serve, int customer_id, int customer_count)
{
    if (waiter_to_serve == 0)
    {
        int index = shm[B_U];
        shm[index] = customer_id;
        shm[index + 1] = customer_count;
        shm[B_U] += 2;
        shm[PO_U]++;
    }
    else if (waiter_to_serve == 1)
    {
        int index = shm[B_V];
        shm[index] = customer_id;
        shm[index + 1] = customer_count;
        shm[B_V] += 2;
        shm[PO_V]++;
    }
    else if (waiter_to_serve == 2)
    {
        int index = shm[B_W];
        shm[index] = customer_id;
        shm[index + 1] = customer_count;
        shm[B_W] += 2;
        shm[PO_W]++;
    }
    else if (waiter_to_serve == 3)
    {
        int index = shm[B_X];
        shm[index] = customer_id;
        shm[index + 1] = customer_count;
        shm[B_X] += 2;
        shm[PO_X]++;
    }
    else if (waiter_to_serve == 4)
    {
        int index = shm[B_Y];
        shm[index] = customer_id;
        shm[index + 1] = customer_count;
        shm[B_Y] += 2;
        shm[PO_Y]++;
    }
}

void cmain(int customer_id, int customer_count)
{
    WAIT(sem_mutex, 0);
    int curr_time = shm[TIME];
    int arrival_time = shm[TIME];

    print_customer_arrival(curr_time, customer_id, customer_count);

    if (curr_time > TIMECLOSE)
    {
        print_customer_leaves_late(curr_time, customer_id);
        SIGNAL(sem_mutex, 0);
        shmdt(shm);
        return;
    }

    int empty_tables = shm[EMPTY_TABLES];
    if (empty_tables == 0)
    {
        print_customer_leaves_table(curr_time, customer_id);
        SIGNAL(sem_mutex, 0);
        shmdt(shm);
        return;
    }

    shm[EMPTY_TABLES]--;
    int waiter_to_serve = shm[WAITER_NEXT];
    shm[WAITER_NEXT] = (waiter_to_serve + 1) % WAITERS;

    add_to_waiter_queue(waiter_to_serve, customer_id, customer_count);

    SIGNAL(sem_waiter, waiter_to_serve);
    SIGNAL(sem_mutex, 0);

    simulate_eating(customer_id, waiter_to_serve, arrival_time);

    shmdt(shm);
}

int main(int argc, char const *argv[])
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

    sleep(1);

    FILE *fp = fopen("customers.txt", "r");
    if (fp == NULL)
    {
        perror("customer : fopen failed");
        exit(1);
    }

    int customer_id, customer_count, arrival_time;
    int curr_time = 0;

    int count_customers = 0;
    while (1)
    {
        if (fscanf(fp, "%d", &customer_id) == EOF || customer_id == -1)
            break;

        count_customers++;
        fscanf(fp, "%d %d", &arrival_time, &customer_count);

        if (arrival_time > curr_time)
        {
            usleep(MINUTE_SCALE * (arrival_time - curr_time));
            curr_time = arrival_time;
        }

        WAIT(sem_mutex, 0);
        shm[TIME] = (curr_time > shm[TIME]) ? curr_time : shm[TIME];
        SIGNAL(sem_mutex, 0);

        if (fork() == 0)
        {
            cmain(customer_id, customer_count);
            exit(0);
        }
    }

    fclose(fp);

    for (int i = 0; i < count_customers; i++)
    {
        int temp = wait(NULL);
    }

    delete_SM();
    delete_semaphores();

    return 0;
}