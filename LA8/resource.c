#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <pthread.h>

// Constants
// For Resources
#define MAX_RESOURCES 20
#define MAX_PROCESSES 100

// For Request Types
#define ADDITIONAL 1
#define RELEASE 2
#define EXIT 3

// struct for requests
typedef struct
{
    int type;
    int thread_id;
    int *req;
} Request;

// Global Variables
int m, n;
Request reqc;
pthread_barrier_t BOS, REQB;
pthread_barrier_t ACK[MAX_PROCESSES];
pthread_cond_t CV[MAX_PROCESSES];
pthread_mutex_t CMTX[MAX_PROCESSES];
int ready_reqs[MAX_PROCESSES];

// Mutexes
pthread_mutex_t rmtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t pmtx = PTHREAD_MUTEX_INITIALIZER;

// Queue Implementation to Queue Requests
typedef struct ReqNode
{
    Request request;
    struct ReqNode *next;
} ReqNode;

typedef struct
{
    ReqNode *front;
    ReqNode *rear;
    int size;
} Queue;

Queue *init_queue();
void enqueue(Queue *queue, Request req);
Request dequeue(Queue *queue);
ReqNode *get_request_node(Queue *queue, int index);
void remove_request(Queue *queue, int thread_id);
void free_queue(Queue *queue);

Queue *init_queue()
{
    Queue *queue = (Queue *)malloc(sizeof(Queue));
    if (queue == NULL)
    {
        perror("Error in allocating memory for queue");
        exit(1);
    }
    queue->front = NULL;
    queue->rear = NULL;
    queue->size = 0;
    return queue;
}
void enqueue(Queue *queue, Request req)
{
    ReqNode *temp = (ReqNode *)malloc(sizeof(ReqNode));
    if (temp == NULL)
    {
        perror("Error in allocating memory for request node");
        exit(1);
    }
    temp->request.type = req.type;
    temp->request.thread_id = req.thread_id;
    temp->request.req = (int *)malloc(sizeof(int) * m);
    if (temp->request.req == NULL)
    {
        perror("Error in allocating memory for request resources");
        exit(1);
    }
    for (int i = 0; i < m; i++)
    {
        temp->request.req[i] = req.req[i];
    }
    temp->next = NULL;
    // Add to queue
    if (queue->rear == NULL)
    {
        queue->front = temp;
        queue->rear = temp;
    }
    else
    {
        queue->rear->next = temp;
        queue->rear = temp;
    }
    queue->size++;
}
Request dequeue(Queue *queue)
{
    Request req;
    req.req = NULL;
    if (queue->front == NULL)
    {
        return req;
    }
    ReqNode *temp = queue->front;
    req = temp->request;
    queue->front = queue->front->next;
    if (queue->front == NULL)
    {
        queue->rear = NULL;
    }
    free(temp);
    queue->size--;
    return req;
}
ReqNode *get_request_node(Queue *queue, int index)
{
    if (index < 0 || index >= queue->size)
        return NULL;
    ReqNode *curr = queue->front;
    int i = 0;
    if (i < index)
    {
        do
        {
            curr = curr->next;
            i++;
        } while (i < index);
    }
    return curr;
}
void remove_request(Queue *queue, int thread_id)
{
    if (queue->front == NULL)
        return;
    if (queue->front->request.thread_id == thread_id)
    {
        ReqNode *temp = queue->front;
        queue->front = queue->front->next;
        if (queue->front == NULL)
            queue->rear = NULL;
        free(temp->request.req);
        free(temp);
        queue->size--;
        return;
    }
    // Search for the node to remove
    ReqNode *curr = queue->front;
    ReqNode *prev = NULL;
    while (curr->request.thread_id != thread_id)
    {
        prev = curr;
        curr = curr->next;
        if (curr == NULL)
            break;
    }
    if (curr != NULL)
    {
        prev->next = curr->next;
        if (curr->next == NULL)
        {
            queue->rear = prev;
        }
        free(curr->request.req);
        free(curr);
        queue->size--;
    }
}
void free_queue(Queue *queue)
{
    ReqNode *curr = queue->front;
    ReqNode *next;
    while (curr != NULL)
    {
        next = curr->next;
        free(curr->request.req);
        free(curr);
        curr = next;
    }
    free(queue);
}

void *MASTER(void *args);
void *PROCESS(void *args);
void print(const char *s, ...);

void print(const char *s, ...)
{
    pthread_mutex_lock(&pmtx);
    va_list args;
    va_start(args, s);
    vprintf(s, args);
    va_end(args);
    fflush(stdout);
    pthread_mutex_unlock(&pmtx);
}

int bankers(int *available, int **max_need, int **alloc, int *live, Request *req)
{
    for (int i = 0; i < m; i++)
    {
        if (req->req[i] > available[i])
            return -1;
    }
#ifndef _DLAVOID
    return 1;
#endif

    // loading current system state
    int *temp_avail = (int *)malloc(sizeof(int) * m);
    int **temp_alloc = (int **)malloc(sizeof(int *) * n);
    int **need = (int **)malloc(sizeof(int *) * n);

    if (temp_avail == NULL || temp_alloc == NULL || need == NULL)
    {
        perror("Error in allocating memory for temp resources");
        exit(1);
    }

    for (int i = 0; i < n; i++)
    {
        temp_alloc[i] = (int *)malloc(sizeof(int) * m);
        need[i] = (int *)malloc(sizeof(int) * m);

        if (temp_alloc[i] == NULL || need[i] == NULL)
        {
            perror("Error in allocating memory for temp resources");
            exit(1);
        }

        for (int j = 0; j < m; j++)
        {
            temp_alloc[i][j] = alloc[i][j];
            need[i][j] = max_need[i][j] - alloc[i][j];
        }
    }

    for (int i = 0; i < m; i++)
    {
        temp_avail[i] = available[i];
    }

    // simulate allocation
    for (int i = 0; i < m; i++)
    {
        temp_avail[i] -= req->req[i];
        temp_alloc[req->thread_id][i] += req->req[i];
        need[req->thread_id][i] -= req->req[i];
    }

    int *finish = (int *)calloc(n, sizeof(int));
    int *work = (int *)malloc(m * sizeof(int));

    if (finish == NULL || work == NULL)
    {
        perror("Memory allocation failed in banker function\n");
        exit(1);
    }

    // mark any tasks as finished, and copy available resources to work
    for (int i = 0; i < n; i++)
    {
        if (live[i] == 0)
            finish[i] = 1;
    }
    for (int i = 0; i < m; i++)
    {
        work[i] = temp_avail[i];
    }
    int p = 1;
    while (p)
    {
        p = 0;
        for (int i = 0; i < n; i++)
        {
            if (!finish[i])
            {
                int possible = 1;
                int j = 0;
                do
                {
                    if (need[i][j] > work[j])
                    {
                        possible = 0;
                        break;
                    }
                    j++;
                } while (j < m);

                if (possible)
                {
                    int j = 0;
                    do
                    {
                        work[j] += temp_alloc[i][j];
                        j++;
                    } while (j < m);
                    finish[i] = 1;
                    p = 1;
                }
            }
        }
    }

    // check if all processes have finished
    int all = 1;
    for (int i = 0; i < n; i++)
    {
        if (!finish[i] && live[i])
        {
            all = 0;
            break;
        }
    }

    free(temp_avail);
    free(finish);
    free(work);

    for (int i = 0; i < n; i++)
    {
        free(temp_alloc[i]);
        free(need[i]);
    }

    return all;
}

void *MASTER(void *args)
{
    int *avail = (int *)malloc(m * sizeof(int));
    int **max_need = (int **)malloc(n * sizeof(int *));
    int **alloc = (int **)malloc(n * sizeof(int *));
    int *live = (int *)malloc(n * sizeof(int));

    if (avail == NULL || max_need == NULL || alloc == NULL || live == NULL)
    {
        perror("Memory allocation failed in MASTER\n");
        exit(1);
    }

    for (int i = 0; i < n; i++)
    {
        max_need[i] = (int *)malloc(m * sizeof(int));
        alloc[i] = (int *)malloc(m * sizeof(int));
        if (max_need[i] == NULL || alloc[i] == NULL)
        {
            perror("Memory allocation failed in MASTER\n");
            exit(1);
        }
        live[i] = 1;
    }

    FILE *fp = fopen("input/system.txt", "r");
    if (fp == NULL)
    {
        perror("Error in opening system.txt");
        exit(1);
    }
    int waste;
    fscanf(fp, "%d", &waste);
    fscanf(fp, "%d", &waste);

    for (int i = 0; i < m; i++)
    {
        fscanf(fp, "%d", &avail[i]);
    }
    fclose(fp);

    // read max for each process i.e. each thread
    for (int i = 0; i < n; i++)
    {
        char filename[64];
        sprintf(filename, "input/thread%02d.txt", i);
        fp = fopen(filename, "r");
        if (fp == NULL)
        {
            perror("Error in opening thread file");
            exit(1);
        }
        int j = 0;
        do
        {
            fscanf(fp, "%d", &max_need[i][j]);
            j++;
        } while (j < m);
        fclose(fp);
    }

    // init request queue
    Queue *queue = init_queue();
    Request local_req;
    local_req.req = (int *)malloc(m * sizeof(int));
    if (local_req.req == NULL)
    {
        perror("Memory allocation failed in MASTER\n");
        exit(1);
    }

    pthread_barrier_wait(&BOS);

    int thread_cnt = n;

    while (thread_cnt)
    {
        pthread_barrier_wait(&REQB);

        // copy request
        local_req.type = reqc.type;
        local_req.thread_id = reqc.thread_id;
        for (int i = 0; i < m; i++)
        {
            local_req.req[i] = reqc.req[i];
        }

        if (local_req.type == EXIT)
            print("Master thread releases resources of thread %d\n", local_req.thread_id);
        else if (local_req.type == ADDITIONAL)
            print("Master thread stores resource request of thread %d\n", local_req.thread_id);

        pthread_barrier_wait(&ACK[local_req.thread_id]);

        if (local_req.type == EXIT)
        {
            // release resources
            for (int i = 0; i < m; i++)
            {
                avail[i] += alloc[local_req.thread_id][i];
                alloc[local_req.thread_id][i] = 0;
            }
            live[local_req.thread_id] = 0;
            thread_cnt--;
        }
        if (local_req.type == RELEASE)
        {
            int i = 0;
            do
            {
                avail[i] -= local_req.req[i];
                alloc[local_req.thread_id][i] += local_req.req[i];
                i++;
            } while (i < m);
        }
        else if (local_req.type == ADDITIONAL)
        {
            // release first
            for (int i = 0; i < m; i++)
            {
                if (local_req.req[i] < 0)
                {
                    avail[i] += abs(local_req.req[i]);
                    alloc[local_req.thread_id][i] -= abs(local_req.req[i]);
                    local_req.req[i] = 0;
                }
            }
            enqueue(queue, local_req);
        }

        if (local_req.type != RELEASE)
        {
            print("\t\tWaiting Threads : ");
            ReqNode *curr = queue->front;
            while (curr != NULL)
            {
                print("%d ", curr->request.thread_id);
                curr = curr->next;
            }
            print("\n");
        }

        if (local_req.type == EXIT)
        {
            print("\t\tAlive Threads : Threads Left : %d ", thread_cnt);
            for (int i = 0; i < n; i++)
            {
                if (live[i])
                    print("%d ", i);
            }
            print("\n");

            print("Available Resources : ");
            for (int i = 0; i < m; i++)
            {
                print("%d ", avail[i]);
            }
            print("\n");

            if (thread_cnt == 0)
                break;
        }

        print("Master Thread tries to grant pending requests\n");

        ReqNode *curr = queue->front;
        int idx = 0;

        while (curr != NULL)
        {
            Request *rrr = &curr->request;
            int proceed = bankers(avail, max_need, alloc, live, rrr);

            if (proceed == -1)
            {
                print("\t+++ Insufficient resources to grant request of thread %d\n", rrr->thread_id);
                idx++;
                curr = curr->next;
            }
            else if (proceed == 0)
            {
                print("\t+++ Deadlock detected, cannot grant request of thread %d\n", rrr->thread_id);
                idx++;
                curr = curr->next;
            }
            else
            {
                print("Master thread grants resource request for thread %d\n", rrr->thread_id);

                int j = 0;
                do
                {
                    avail[j] -= rrr->req[j];
                    alloc[rrr->thread_id][j] += rrr->req[j];
                    j++;
                } while (j < m);

                ReqNode *to_remove = curr;
                int thread_to_signal = rrr->thread_id;

                curr = curr->next;

                remove_request(queue, thread_to_signal);

                pthread_mutex_lock(&CMTX[thread_to_signal]);
                ready_reqs[thread_to_signal] = 1;
                pthread_cond_signal(&CV[thread_to_signal]);
                pthread_mutex_unlock(&CMTX[thread_to_signal]);
            }
        }

        print("\t\tWaiting Threads : ");
        curr = queue->front;
        while (curr != NULL)
        {
            print("%d ", curr->request.thread_id);
            curr = curr->next;
        }
        print("\n");
    }

    free_queue(queue);
    free(local_req.req);
    free(avail);
    free(live);

    for (int i = 0; i < n; i++)
    {
        free(max_need[i]);
        free(alloc[i]);
    }
    free(max_need);
    free(alloc);

    return NULL;
}

void *PROCESS(void *args)
{
    int tid = *(int *)args;
    char filename[64];
    sprintf(filename, "input/thread%02d.txt", tid);

    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
    {
        perror("Error in opening thread file");
        exit(1);
    }

    // skip the first line
    for (int i = 0; i < m; i++)
    {
        int x;
        fscanf(fp, "%d", &x);
    }

    print("\tThread %d Born\n", tid);
    pthread_barrier_wait(&BOS);

    int delay = 0;
    int *req_res = (int *)malloc(sizeof(int) * m);
    if (req_res == NULL)
    {
        perror("Error in allocating memory for request resources");
        exit(1);
    }

    while (1)
    {
        fscanf(fp, "%d", &delay);
        usleep(delay * 100000);

        char rtype;
        fscanf(fp, " %c", &rtype);

        if (rtype == 'Q')
            break;

        int type = RELEASE;
        for (int i = 0; i < m; i++)
        {
            fscanf(fp, "%d", &req_res[i]);
            if (req_res[i] > 0)
            {
                type = ADDITIONAL;
            }
        }

        pthread_mutex_lock(&rmtx);

        reqc.type = type;
        reqc.thread_id = tid;
        for (int i = 0; i < m; i++)
            reqc.req[i] = req_res[i];

        if (type == ADDITIONAL)
            print("\tThread %d sends resource request: type = ADDITIONAL\n", tid);
        else
            print("\tThread %d sends resource request: type = RELEASE\n", tid);

        pthread_barrier_wait(&REQB);
        pthread_barrier_wait(&ACK[tid]);

        pthread_mutex_unlock(&rmtx);

        if (type == ADDITIONAL)
        {
            pthread_mutex_lock(&CMTX[tid]);
            while (!ready_reqs[tid])
                pthread_cond_wait(&CV[tid], &CMTX[tid]);
            ready_reqs[tid] = 0;
            pthread_mutex_unlock(&CMTX[tid]);

            print("\tThread %d is granted its last resource request\n", tid);
        }
        else
            print("\tThread %d is done with its resource, release request\n", tid);
    }

    pthread_mutex_lock(&rmtx);
    reqc.type = EXIT;
    reqc.thread_id = tid;
    pthread_barrier_wait(&REQB);
    pthread_barrier_wait(&ACK[tid]);
    pthread_mutex_unlock(&rmtx);

    print("\tThread %d going to quit\n", tid);

    free(req_res);
    fclose(fp);
    return NULL;
}

int main(int argc, char const *argv[])
{
    setvbuf(stdout, NULL, _IONBF, 0); // disable output buffering

    FILE *fp = fopen("input/system.txt", "r");
    if (fp == NULL)
    {
        perror("Error in opening system.txt");
        exit(1);
    }
    fscanf(fp, "%d", &m);
    fscanf(fp, "%d", &n);
    fclose(fp);

    reqc.req = (int *)malloc(sizeof(int) * m);
    if (reqc.req == NULL)
    {
        perror("Error in allocating memory for request resources");
        exit(1);
    }

    pthread_barrier_init(&BOS, NULL, n + 1);
    pthread_barrier_init(&REQB, NULL, 2);

    for (int i = 0; i < n; i++)
    {
        pthread_barrier_init(&ACK[i], NULL, 2);
        pthread_cond_init(&CV[i], NULL);
        pthread_mutex_init(&CMTX[i], NULL);
        ready_reqs[i] = 0;
    }

    pthread_t master;
    pthread_t *threads = (pthread_t *)malloc(sizeof(pthread_t) * n);
    int *thread_ids = (int *)malloc(sizeof(int) * n);
    if (threads == NULL || thread_ids == NULL)
    {
        perror("Error in allocating memory for thread arrays");
        exit(1);
    }

    if (pthread_create(&master, NULL, MASTER, NULL) != 0)
    {
        perror("Error in creating master thread");
        exit(1);
    }

    for (int i = 0; i < n; i++)
    {
        thread_ids[i] = i;
        if (pthread_create(&threads[i], NULL, PROCESS, &thread_ids[i]) != 0)
        {
            perror("Error in creating process thread");
            exit(1);
        }
    }

    pthread_join(master, NULL);
    for (int i = 0; i < n; i++)
    {
        pthread_join(threads[i], NULL);
    }

    // Free allocated memory and destroy
    free(reqc.req);
    free(threads);
    free(thread_ids);

    pthread_barrier_destroy(&BOS);
    pthread_barrier_destroy(&REQB);
    for (int i = 0; i < n; i++)
    {
        pthread_barrier_destroy(&ACK[i]);
        pthread_cond_destroy(&CV[i]);
        pthread_mutex_destroy(&CMTX[i]);
    }

    return 0;
}