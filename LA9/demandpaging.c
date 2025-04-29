#include <stdio.h>
#include <stdlib.h>

#define PAGE_SIZE 4096
#define ELEMS_PER_PAGE 1024
#define PAGE_TABLE_SIZE 2048
#define ESSENTIAL_PAGES 10
#define TOTAL_MEMORY_MB 64
#define OS_RESERVED_MB 16
#define TOTAL_FRAMES_USEABLE 12288

__uint32_t accesses = 0;
__uint32_t faults = 0;
__uint32_t swaps = 0;
__uint32_t deg_multiprog = __INT8_MAX__;
__uint32_t active_processes = 0;

// struct for process
typedef struct
{
    int id, s, m, curr_search_cnt, *search_indices;
    unsigned short PageTable[PAGE_TABLE_SIZE];
} Process;

// Queue Implementation
typedef struct Node
{
    Process *proc;
    struct Node *next;
} Node;
typedef struct
{
    Node *front, *rear;
} Queue;
void enqueue(Queue *q, Process *p)
{
    Node *new = (Node *)malloc(sizeof(Node));
    new->proc = p;
    new->next = NULL;
    if (q->rear == NULL)
        q->front = q->rear = new;
    else
    {
        q->rear->next = new;
        q->rear = new;
    }
}
Process *dequeue(Queue *q)
{
    if (q->front == NULL)
        return NULL;
    Node *temp = q->front;
    Process *p = temp->proc;
    q->front = q->front->next;
    if (q->front == NULL)
        q->rear = NULL;
    free(temp);
    return p;
}

// required queues
Queue ready_queue = {NULL, NULL};
Queue swapped_queue = {NULL, NULL};
Queue free_frames = {NULL, NULL};
// store all the processes
Process **processes;

int BS(Process *p, int search_idx)
{
    int L = 0, R = p->s - 1;

#ifdef VERBOSE
    printf("\tSearch %d by Process %d\n", p->curr_search_cnt + 1, p->id);
    fflush(stdout);
#endif

    while (L < R)
    {
        int M = (L + R) >> 1;
        int page_num = ESSENTIAL_PAGES + (M >> 10);
        accesses++;

        if (!(p->PageTable[page_num] & (1 << 15)))
        {
            /*
                if that page is invalid in the logical memory,
                that means we need ot fetch that page
            */
            faults++;
            if (free_frames.front != NULL)
            {
                unsigned short frame = (unsigned short)(long)dequeue(&free_frames);
                p->PageTable[page_num] = (frame | (1 << 15));
            }
            else
            {
                active_processes--;
                printf("+++ Swapping out process %3d [%u active processes]\n", p->id, active_processes);
                fflush(stdout);
                for (int i = 0; i < PAGE_TABLE_SIZE; i++)
                {
                    if (p->PageTable[i] & (1 << 15)) // if that page is valid release that
                    {
                        enqueue(&free_frames, (Process *)(long)(p->PageTable[i] & ~(1 << 15)));
                        p->PageTable[i] = 0;
                    }
                }
                enqueue(&swapped_queue, p);
                swaps++;
                deg_multiprog = (deg_multiprog < active_processes) ? deg_multiprog : active_processes;
                return 1; // failed to search, so need to fetch the page
            }
        }
        if (search_idx <= M)
            R = M;
        else
            L = M + 1;
    }
    return 0; // successfully searched
}

int main(int argc, char const *argv[])
{
    int n, m;

    // initialise the data ------
    FILE *fp = fopen("search.txt", "r");
    if (fp == NULL)
    {
        printf("File not found\n");
        exit(1);
    }
    fscanf(fp, "%d %d", &n, &m);

    // put all frames in free_frames
    for (int i = 0; i < TOTAL_FRAMES_USEABLE; i++)
    {
        enqueue(&free_frames, (Process *)(long)i);
    }

    processes = (Process **)malloc(n * sizeof(Process *));
    for (int i = 0; i < n; i++)
    {
        processes[i] = (Process *)malloc(sizeof(Process));
        fscanf(fp, "%d", &processes[i]->s);
        processes[i]->id = i;
        processes[i]->m = m;
        processes[i]->curr_search_cnt = 0;
        processes[i]->search_indices = (int *)malloc(m * sizeof(int));

        // read all the indices meant for BS
        for (int j = 0; j < m; j++)
        {
            fscanf(fp, "%d", &processes[i]->search_indices[j]);
        }

        // initialise the page table of the process
        for (int j = 0; j < PAGE_TABLE_SIZE; j++)
        {
            processes[i]->PageTable[j] = 0;
        }

        // give all the processes the 10 essential processes
        for (int j = 0; j < ESSENTIAL_PAGES; j++)
        {
            // remove the free frame from the free_frames queue
            unsigned short frame = (unsigned short)(long)dequeue(&free_frames);
            // set the frame in the page table
            processes[i]->PageTable[j] = (frame | (1 << 15));
        }

        enqueue(&ready_queue, processes[i]);
        active_processes++;
    }
    fclose(fp); // reading done

    // simulation starts here
    printf("+++ Simulation data read from file\n");
    fflush(stdout);
    printf("+++ Kernel data initialized\n");
    fflush(stdout);
    do
    {
        int flag = 0;
        if (ready_queue.front)
        {
            Process *p = dequeue(&ready_queue);
            int search_idx = p->search_indices[p->curr_search_cnt];
            if (BS(p, search_idx) == 0) // successful BS completion
            {
                p->curr_search_cnt++;
                if (p->curr_search_cnt == m)
                {
                    // terminate the process, all searches completed
                    for (int i = 0; i < PAGE_TABLE_SIZE; i++)
                    {
                        if (p->PageTable[i] & (1 << 15))
                            enqueue(&free_frames, (Process *)(long)(p->PageTable[i] & ~(1 << 15)));
                    }
                    active_processes--;
                    free(p->search_indices);
                    free(p);

                    flag = 1; // mark that a process has been terminated
                }
                else
                    enqueue(&ready_queue, p);
            }
        }

        if (flag && swapped_queue.front)
        {
            Process *p = dequeue(&swapped_queue);
            enqueue(&ready_queue, p);
            active_processes++;
            printf("+++ Swapping in process %3d [%u active processes]\n", p->id, active_processes);
            fflush(stdout);
        }
    } while (ready_queue.front || swapped_queue.front);

    printf("+++ Page access summary\n");
    printf("\tTotal number of page access = %u\n", accesses);
    printf("\tTotal number of page faults = %u\n", faults);
    printf("\tTotal number of swaps = %u\n", swaps);
    printf("\tDegree of multiprogramming = %u\n", deg_multiprog);
    fflush(stdout);

    return 0;
}