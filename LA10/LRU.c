#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define PAGE_SIZE 4096
#define ELEMS_PER_PAGE 1024
#define PAGE_TABLE_SIZE 2048
#define ESSENTIAL_PAGES 10
#define TOTAL_MEMORY_MB 64
#define OS_RESERVED_MB 16
#define TOTAL_FRAMES_USEABLE 12288
#define NFFMIN 1000

// Statistics
__uint32_t accesses = 0;
__uint32_t faults = 0;
__uint32_t replacements = 0;
__uint32_t attempt1 = 0;
__uint32_t attempt2 = 0;
__uint32_t attempt3 = 0;
__uint32_t attempt4 = 0;
__uint32_t active_processes = 0;

typedef struct
{
    int frame_no, last_owner, pg_no;
} FrameEntry;

FrameEntry FFLIST[TOTAL_FRAMES_USEABLE];
int NFF = TOTAL_FRAMES_USEABLE;

typedef struct
{
    unsigned short entry, history;
} PageTableEntry;

typedef struct
{
    int id, s, m, curr_search_cnt, *search_indices;
    PageTableEntry PageTable[PAGE_TABLE_SIZE];
} Process;
Process **processes;

typedef struct
{
    __uint32_t accesses, faults, replacements;
    __uint32_t attempt1, attempt2, attempt3, attempt4;
} ProcessStats;
ProcessStats *process_stats;

// Ready Queue Implementation
typedef struct Node
{
    Process *process;
    struct Node *next;
} Node;
typedef struct
{
    Node *front, *rear;
} Queue;
Queue ready_queue = {NULL, NULL};
void enqueue(Queue *q, Process *p)
{
    Node *new_node = (Node *)malloc(sizeof(Node));
    new_node->process = p;
    new_node->next = NULL;
    if (q->rear == NULL)
        q->front = q->rear = new_node;
    else
    {
        q->rear->next = new_node;
        q->rear = new_node;
    }
}
Process *dequeue(Queue *q)
{
    if (q->front == NULL)
        return NULL;
    Node *temp = q->front;
    Process *p = temp->process;
    q->front = q->front->next;
    if (q->front == NULL)
        q->rear = NULL;
    free(temp);
    return p;
}

int page_replacement_algo(Process *p, int page_num)
{
    int victim_page = -1;
    unsigned short min_hist = 0xFFFF;
    for (int i = ESSENTIAL_PAGES; i < PAGE_TABLE_SIZE; i++)
    {
        if ((p->PageTable[i].entry & (1 << 15)) && (p->PageTable[i].history < min_hist))
        {
            min_hist = p->PageTable[i].history;
            victim_page = i;
        }
    }
    if (victim_page == -1) // ideally should not happen
        return -1;

    int victim_frame = p->PageTable[victim_page].entry & 0x3FFF; // get the frame of victim page

    // now we will make attempts to find a frame
    int replacement_frame = -1;
    int found_index = -1;
    int attempt = 0;

    // Attempt 1: Find frame with same process and page
    for (int i = NFF - 1; i >= 0; i--)
    {
        if (FFLIST[i].last_owner == p->id && FFLIST[i].pg_no == page_num)
        {
            replacement_frame = FFLIST[i].frame_no;
            found_index = i;
            attempt = 1;
            break;
        }
    }

    // Attempt 2: Find frame with no owner
    if (replacement_frame == -1)
    {
        for (int i = NFF - 1; i >= 0; i--)
        {
            if (FFLIST[i].last_owner == -1)
            {
                replacement_frame = FFLIST[i].frame_no;
                found_index = i;
                attempt = 2;
                break;
            }
        }
    }

    // Attempt 3: Find frame with same process
    if (replacement_frame == -1)
    {
        for (int i = NFF - 1; i >= 0; i--)
        {
            if (FFLIST[i].last_owner == p->id)
            {
                replacement_frame = FFLIST[i].frame_no;
                found_index = i;
                attempt = 3;
                break;
            }
        }
    }

    // Attempt 4: Pick a random frame, the first one
    if (replacement_frame == -1)
    {
        srand(42);
        int random_index = rand() % NFF;
        // int random_index = 0;
        replacement_frame = FFLIST[random_index].frame_no;
        found_index = random_index;
        attempt = 4;
    }

    for (int j = found_index; j < NFF - 1; j++)
    {
        FFLIST[j] = FFLIST[j + 1];
    }
    NFF--;

    // Add the victim frame to FFLIST
    FFLIST[NFF].frame_no = victim_frame;
    FFLIST[NFF].last_owner = p->id;
    FFLIST[NFF].pg_no = victim_page;
    NFF++;

    // update the page table
    p->PageTable[victim_page].entry &= ~(1 << 15); // clear the valid bit

#ifdef VERBOSE
    printf("    Fault on Page %4d: To replace Page %d at Frame %d [history = %d]\n",
           page_num, victim_page, victim_frame, min_hist);
    switch (attempt)
    {
    case 1:
        printf("        Attempt 1: Page found in free frame %d\n", replacement_frame);
        break;
    case 2:
        printf("        Attempt 2: Free frame %d owned by no process found\n", replacement_frame);
        break;
    case 3:
        printf("        Attempt 3: Own page %d found in free frame %d\n", page_num, replacement_frame);
        break;
    case 4:
        printf("        Attempt 4: Free frame %d owned by Process %d chosen\n", replacement_frame, p->id);
        break;
    }
#endif

    switch (attempt)
    {
    case 1:
        attempt1++;
        process_stats[p->id].attempt1++;
        break;
    case 2:
        attempt2++;
        process_stats[p->id].attempt2++;
        break;
    case 3:
        attempt3++;
        process_stats[p->id].attempt3++;
        break;
    case 4:
        attempt4++;
        process_stats[p->id].attempt4++;
        break;
    }

    replacements++;
    process_stats[p->id].replacements++;
    return replacement_frame;
}

// get a frame from the FFLIST
int get_frame(Process *p, int page_num, int flag)
{
    if (NFF > NFFMIN)
    {
        int frame = FFLIST[0].frame_no;

#ifdef VERBOSE
        if (flag == 1)
            printf("    Fault on Page %4d: Free frame %d found\n", page_num, frame);
#endif

        // remove the page from FFLIST
        for (int i = 0; i < NFF - 1; i++)
        {
            FFLIST[i] = FFLIST[i + 1];
        }
        NFF--;
        return frame;
    }
    else
    {
        return page_replacement_algo(p, page_num);
    }
}

// finish the process
void finish_process(Process *p)
{
    for (int i = 0; i < PAGE_TABLE_SIZE; i++)
    {
        if (p->PageTable[i].entry & (1 << 15))
        {
            int frame = p->PageTable[i].entry & 0x3FFF;
            FFLIST[NFF].frame_no = frame;
            FFLIST[NFF].last_owner = -1;
            FFLIST[NFF].pg_no = -1;
            NFF++;
        }
    }
    active_processes--;
    free(p->search_indices);
    free(p);
}

int BS(Process *p, int search_idx)
{
    int L = 0, R = p->s - 1;
#ifdef VERBOSE
    printf("+++ Process %d: Search %d\n", p->id, p->curr_search_cnt + 1);
#endif

    while (L < R)
    {
        int M = (L + R) >> 1;
        int page_num = ESSENTIAL_PAGES + (M >> 10);
        accesses++;
        process_stats[p->id].accesses++;

        if (!(p->PageTable[page_num].entry & (1 << 15))) // invalid, page fault
        {
            faults++;
            process_stats[p->id].faults++;
            int frame = get_frame(p, page_num, 1);
            if (frame == -1)
                return 1;
            p->PageTable[page_num].entry = frame | (1 << 15) | (1 << 14);
            p->PageTable[page_num].history = 0xFFFF;
        }
        else
        {
            p->PageTable[page_num].entry |= (1 << 14);
        }

        if (search_idx <= M)
            R = M;
        else
            L = M + 1;
    }
    for (int i = 0; i < PAGE_TABLE_SIZE; i++)
    {
        if (p->PageTable[i].entry & (1 << 15)) // valid page
        {
            p->PageTable[i].history = (p->PageTable[i].history >> 1) |
                                      ((p->PageTable[i].entry & (1 << 14)) ? 0x8000 : 0);
            p->PageTable[i].entry &= ~(1 << 14);
        }
    }
    return 0;
}

int main(int argc, char const *argv[])
{
    int n, m;
    FILE *fp = fopen("search.txt", "r");
    if (fp == NULL)
    {
        printf("File not found\n");
        exit(1);
    }
    fscanf(fp, "%d %d", &n, &m);

    // Initialize FFLIST
    for (int i = 0; i < TOTAL_FRAMES_USEABLE; i++)
    {
        FFLIST[i].frame_no = i;
        FFLIST[i].last_owner = -1;
        FFLIST[i].pg_no = -1;
    }
    NFF = TOTAL_FRAMES_USEABLE;

    process_stats = (ProcessStats *)calloc(n, sizeof(ProcessStats));
    processes = (Process **)malloc(n * sizeof(Process *));

    for (int i = 0; i < n; i++)
    {
        processes[i] = (Process *)malloc(sizeof(Process));
        fscanf(fp, "%d", &processes[i]->s);
        processes[i]->id = i;
        processes[i]->m = m;
        processes[i]->curr_search_cnt = 0;
        processes[i]->search_indices = (int *)malloc(m * sizeof(int));
        for (int j = 0; j < m; j++)
        {
            fscanf(fp, "%d", &processes[i]->search_indices[j]);
        }
        for (int j = 0; j < PAGE_TABLE_SIZE; j++)
        {
            processes[i]->PageTable[j].entry = 0;
            processes[i]->PageTable[j].history = 0;
        }

        // Essential Pages
        for (int j = 0; j < ESSENTIAL_PAGES; j++)
        {
            int frame = get_frame(processes[i], j, 0);
            processes[i]->PageTable[j].entry = frame | (1 << 15);
            processes[i]->PageTable[j].history = 0xFFFF; // essential page history as mentioned
        }

        enqueue(&ready_queue, processes[i]);
        active_processes++;
    }
    fclose(fp);

    // Simulation starts here
    // printf("+++ Simulation data read from file\n");
    // printf("+++ Kernel data initialized\n");

    while (ready_queue.front)
    {
        Process *p = dequeue(&ready_queue);
        int search_idx = p->search_indices[p->curr_search_cnt];
        if (BS(p, search_idx) == 0)
        {
            p->curr_search_cnt++;
            if (p->curr_search_cnt == p->m)
                finish_process(p);
            else
                enqueue(&ready_queue, p);
        }
    }

    // // print statistics per page
    // printf("+++ Page access summary\n");
    // printf(" PID Accesses \t Faults \t\t Replacements\t\tAttempts\n");
    int total_accesses = 0;
    int total_faults = 0;
    int total_replacements = 0;
    int total_attempt1 = 0;
    int total_attempt2 = 0;
    int total_attempt3 = 0;
    int total_attempt4 = 0;

    for (int i = 0; i < n; i++)
    {
        printf(" %-3d %8u %6u (%5.2f%%) %6u (%5.2f%%) %6u + %3u + %3u + %3u (%5.2f%% + %5.2f%% + %5.2f%% + %5.2f%%)\n",
               i,
               process_stats[i].accesses,
               process_stats[i].faults,
               (float)process_stats[i].faults * 100 / process_stats[i].accesses,
               process_stats[i].replacements,
               (float)process_stats[i].replacements * 100 / process_stats[i].accesses,
               process_stats[i].attempt1,
               process_stats[i].attempt2,
               process_stats[i].attempt3,
               process_stats[i].attempt4,
               process_stats[i].replacements > 0 ? (float)process_stats[i].attempt1 * 100 / process_stats[i].replacements : 0.0,
               process_stats[i].replacements > 0 ? (float)process_stats[i].attempt2 * 100 / process_stats[i].replacements : 0.0,
               process_stats[i].replacements > 0 ? (float)process_stats[i].attempt3 * 100 / process_stats[i].replacements : 0.0,
               process_stats[i].replacements > 0 ? (float)process_stats[i].attempt4 * 100 / process_stats[i].replacements : 0.0);

        total_accesses += process_stats[i].accesses;
        total_faults += process_stats[i].faults;
        total_replacements += process_stats[i].replacements;
        total_attempt1 += process_stats[i].attempt1;
        total_attempt2 += process_stats[i].attempt2;
        total_attempt3 += process_stats[i].attempt3;
        total_attempt4 += process_stats[i].attempt4;
    }

    printf("\nTotal %8d %6d (%5.2f%%) %6d (%5.2f%%) %6d + %3d + %3d + %d (%5.2f%% + %5.2f%% + %5.2f%% + %5.2f%%)\n",
           total_accesses,
           total_faults,
           (float)total_faults * 100 / total_accesses,
           total_replacements,
           (float)total_replacements * 100 / total_accesses,
           total_attempt1,
           total_attempt2,
           total_attempt3,
           total_attempt4,
           total_replacements > 0 ? (float)total_attempt1 * 100 / total_replacements : 0.0,
           total_replacements > 0 ? (float)total_attempt2 * 100 / total_replacements : 0.0,
           total_replacements > 0 ? (float)total_attempt3 * 100 / total_replacements : 0.0,
           total_replacements > 0 ? (float)total_attempt4 * 100 / total_replacements : 0.0);
    return 0;
}