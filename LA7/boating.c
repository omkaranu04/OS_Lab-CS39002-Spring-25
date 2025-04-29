#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

typedef struct
{
    int value;
    pthread_mutex_t mtx;
    pthread_cond_t cv;
} semaphore;

void P(semaphore *s);
void V(semaphore *s);
void *BoatThread(void *arg);
void *RiderThread(void *arg);
int rand_tm(int min, int max);

int n, m, remaining_visitors;
int *BA, *BC, *BT; // BoatAvailable, BoatCarrying, BoatTime
pthread_barrier_t *BB;

semaphore rider, boat;
pthread_mutex_t bmtx;
pthread_barrier_t EOS;

// wait
void P(semaphore *s)
{
    pthread_mutex_lock(&(s->mtx));
    s->value--;
    if (s->value < 0)
    {
        pthread_cond_wait(&(s->cv), &(s->mtx));
    }
    pthread_mutex_unlock(&(s->mtx));
}

// signal
void V(semaphore *s)
{
    pthread_mutex_lock(&(s->mtx));
    s->value++;
    if (s->value <= 0)
    {
        pthread_cond_signal(&(s->cv));
    }
    pthread_mutex_unlock(&(s->mtx));
}

// produce random number
int rand_tm(int min, int max)
{
    return min + rand() % (max - min + 1);
}

void *BoatThread(void *arg)
{
    int boat_id = *((int *)arg);
    free(arg);

    printf("Boat\t%d\tReady\n", boat_id);

    pthread_mutex_lock(&bmtx);
    pthread_barrier_init(&BB[boat_id], NULL, 2);
    pthread_mutex_unlock(&bmtx);

    while (1)
    {
        P(&boat);

        pthread_mutex_lock(&bmtx);
        BA[boat_id] = 1;  // boat available
        BC[boat_id] = -1; // boat not carrying any visitor
        pthread_mutex_unlock(&bmtx);

        V(&rider); // signal the rider that the boat is available

        pthread_barrier_wait(&BB[boat_id]);

        pthread_mutex_lock(&bmtx);
        int rider_id = BC[boat_id];
        int ride_time = BT[boat_id];
        BA[boat_id] = 0; // boat not available
        pthread_mutex_unlock(&bmtx);

        printf("Boat\t%d\tStart of ride for visitor %d\n", boat_id, rider_id);
        usleep(ride_time * 100000);
        printf("Boat\t%d\tEnd of ride for visitor %d (ride time = %d)\n", boat_id, rider_id, ride_time);
        printf("Visitor\t%d\tLeaving\n", rider_id);

        pthread_mutex_lock(&bmtx);
        remaining_visitors--;
        if (remaining_visitors == 0)
        {
            pthread_mutex_unlock(&bmtx);
            pthread_barrier_wait(&EOS);
            break;
        }
        pthread_mutex_unlock(&bmtx);
    }
    pthread_exit(NULL);
}

void *RiderThread(void *arg)
{
    int rider_id = *((int *)arg);
    free(arg);

    srand(time(NULL) ^ (rider_id << 16)); // setting seed for random number generation

    int vtime = rand_tm(30, 120);
    // int vtime = 10;
    int rtime = rand_tm(15, 60);
    // int rtime = 5;

    printf("Visitor\t%d\tStarts sightseeing for %d minutes\n", rider_id, vtime);
    usleep(vtime * 100000);
    V(&boat);  // signal the boat that the rider is ready to ride
    P(&rider); // wait for boat to be available
    printf("Visitor\t%d\tReady to ride a boat (ride time = %d)\n", rider_id, rtime);

    int boat_index = -1;
    for (int i = 1; i <= m; i++)
    {
        pthread_mutex_lock(&bmtx);
        if (BA[i] == 1 && BC[i] == -1)
        {
            boat_index = i;
            BC[i] = rider_id;
            BT[i] = rtime;
            pthread_mutex_unlock(&bmtx);
            break;
        }
        pthread_mutex_unlock(&bmtx);
    }

    printf("Visitor\t%d\tFinds boat %d\n", rider_id, boat_index);
    pthread_barrier_wait(&BB[boat_index]);
    usleep(rtime * 100000);
    pthread_exit(NULL);
}

int main(int argc, char const *argv[])
{
    if (argc != 3)
    {
        printf("Usage: %s <number of boats> <number of visitors>\n", argv[0]);
        return 1;
    }

    m = atoi(argv[1]); // number of boats
    n = atoi(argv[2]); // number of visitors

    if (m < 5 || m > 10 || n < 20 || n > 100)
    {
        printf("Invalid input 5 <= m <= 10  20 <= n <= 100\n");
        return 1;
    }

    remaining_visitors = n; // number of visitors remaining to be served

    // initializing semaphores
    rider = (semaphore){0, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER};
    boat = (semaphore){0, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER};
    bmtx = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;

    // mallocing memory for arrays
    BA = (int *)malloc((m + 1) * sizeof(int));
    BC = (int *)malloc((m + 1) * sizeof(int));
    BT = (int *)malloc((m + 1) * sizeof(int));
    BB = (pthread_barrier_t *)malloc((m + 1) * sizeof(pthread_barrier_t));

    // initializing boat barriers
    for (int i = 1; i <= m; i++)
    {
        pthread_barrier_init(&BB[i], NULL, 2);
    }

    for (int i = 1; i <= m; i++)
    {
        BA[i] = 0;  // boat not available
        BC[i] = -1; // boat not carrying any visitor
        BT[i] = 0;  // boat time
    }

    // initializing barrier
    pthread_barrier_init(&EOS, NULL, 2);

    // creating boat threads
    pthread_t *boat_threads = (pthread_t *)malloc((m + 1) * sizeof(pthread_t));
    for (int i = 1; i <= m; i++)
    {
        int *boat_id = (int *)malloc(sizeof(int));
        *boat_id = i;
        pthread_create(&boat_threads[i], NULL, BoatThread, boat_id);
    }

    // creating visitor threads
    pthread_t *visitor_threads = (pthread_t *)malloc((n + 1) * sizeof(pthread_t));
    for (int i = 1; i <= n; i++)
    {
        int *visitor_id = (int *)malloc(sizeof(int));
        *visitor_id = i;
        pthread_create(&visitor_threads[i], NULL, RiderThread, visitor_id);
    }

    // waiting on barrier
    pthread_barrier_wait(&EOS);

    for (int i = 1; i <= n; i++)
    {
        pthread_cancel(visitor_threads[i]);
    }
    for (int i = 1; i <= m; i++)
    {
        pthread_cancel(boat_threads[i]);
    }
    for (int i = 1; i <= m; i++)
    {
        pthread_barrier_destroy(&BB[i]);
    }

    pthread_mutex_destroy(&bmtx);
    pthread_mutex_destroy(&(rider.mtx));
    pthread_mutex_destroy(&(boat.mtx));
    pthread_cond_destroy(&(rider.cv));
    pthread_cond_destroy(&(boat.cv));

    pthread_barrier_destroy(&EOS);

    free(BA);
    free(BC);
    free(BT);
    free(BB);
    free(boat_threads);
    free(visitor_threads);

    printf("All visitors served\n");
    return 0;
}