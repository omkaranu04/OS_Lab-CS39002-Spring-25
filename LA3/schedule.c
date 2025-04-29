#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>

#define MAXB 50
#define MAXP 2000
#define PINF 1000000000

/*
    -- STATES --
    READY       1
    RUNNING     2
    WAITING     3
    FINISHED    4
*/

// helper function
int minimumm(int a, int b)
{
    return a < b ? a : b;
}

// static structure to store processes, with minor updation
typedef struct
{
    int id, arrival_time, num_bursts, curr_burst, rem_time, tot_runtime, tat;
    int cpu_bursts[MAXB], io_bursts[MAXB];
    int state;
} process;

// structure for storing all the events happening while scheduling the processes
typedef struct
{
    int process_id, time, event_type;
} event;

// global variables
process processes[MAXP];
event event_heap[MAXP * MAXB];
int ready_queue[MAXP];
int num_proc, heap_sz = 0, r_frnt = 0, r_rear = 0;

void swap(event *a, event *b)
{
    event temp = *a;
    *a = *b;
    *b = temp;
}

/*
    Comparison based on :
    For multiple events happening at the same time
    (like two processes complete IO and a new process arrives at the same time),
    some tie-breaking policies are to be used. In respect of the joining of a process to the ready queue, the following ordering is to be maintained:
    Arrival (for the first time or after IO completion) < CPU timeout
    Ties are possible even with this ordering. Give precedence to smaller IDs to break the ties. For example,
    suppose that a process with ID j arrives at the same time when a process with ID i completes an IO burst. If
    we have i < j, then the IO-completion event will appear earlier in the event queue than the new-arrival event.
*/
int comp(event a, event b)
{
    if (a.time != b.time)
        return a.time < b.time;
    if ((a.event_type == 1 || a.event_type == 3) && b.event_type == 4)
        return 1;
    if (a.event_type == 4 && (b.event_type == 1 || b.event_type == 3))
        return 0;
    return processes[a.process_id].id < processes[b.process_id].id;
}

// heap functions
void heapify(int idx)
{
    int sm = idx, l = 2 * idx + 1, r = 2 * idx + 2;
    if (l < heap_sz && comp(event_heap[l], event_heap[sm]))
        sm = l;
    if (r < heap_sz && comp(event_heap[r], event_heap[sm]))
        sm = r;
    if (sm != idx)
    {
        swap(&event_heap[sm], &event_heap[idx]);
        heapify(sm);
    }
}

void insert(event eve)
{
    int idx = heap_sz++;
    event_heap[idx] = eve;

    while (idx > 0)
    {
        int par = (idx - 1) / 2;
        if (comp(event_heap[idx], event_heap[par]))
        {
            swap(&event_heap[idx], &event_heap[par]);
            idx = par;
        }
        else
            break;
    }
}

event min()
{
    event min_eve = event_heap[0];
    event_heap[0] = event_heap[--heap_sz];
    heapify(0);
    return min_eve;
}

// queue functions
void enqueue(int proc_id)
{
    ready_queue[r_rear++] = proc_id;
}

int dequeue()
{
    if (r_frnt == r_rear)
        return -1;
    return ready_queue[r_frnt++];
}

// read input from file
void init()
{
    FILE *fp = fopen("proc.txt", "r");
    if (!fp)
    {
        printf("Error opening proc.txt\n");
        exit(1);
    }
    // printf("Reading input from proc.txt\n");
    fscanf(fp, "%d", &num_proc);

    for (int i = 0; i < num_proc; i++)
    {
        process *p = &processes[i];
        fscanf(fp, "%d %d", &p->id, &p->arrival_time);

        int temp = 0;
        p->tot_runtime = 0;
        while (1)
        {
            fscanf(fp, "%d", &p->cpu_bursts[temp]);
            p->tot_runtime += p->cpu_bursts[temp];

            fscanf(fp, "%d", &p->io_bursts[temp]);
            if (p->io_bursts[temp] == -1)
                break;
            p->tot_runtime += p->io_bursts[temp];
            temp++;
        }

        p->num_bursts = temp + 1;
        p->curr_burst = 0;
        p->rem_time = p->cpu_bursts[0];
        p->state = 1;

        event x = {i, p->arrival_time, 1};
        insert(x);
    }
    fclose(fp);
}

// main scheduling function
void schedule(int q)
{
    int curr_time = 0, run_proc = -1, idle_time = 0;
    r_frnt = 0, r_rear = 0;
    heap_sz = 0;

    // insert all the processes in the events heap
    for (int i = 0; i < num_proc; i++)
    {
        processes[i].curr_burst = 0;
        processes[i].rem_time = processes[i].cpu_bursts[0];
        processes[i].state = 1;
        event x = {i, processes[i].arrival_time, 1}; // at first all will be arrival thing
        insert(x);
    }
    // here the min heap desired will be created and the scheduler will start its functioning
#ifdef VERBOSE
    const int width = 10;
    printf("%-*d: Starting\n", width, curr_time);
#endif

    // main loop
    while (heap_sz > 0)
    {
        // based on requirement, heap top will give the process to be
        event eve = min();
        int ptime = curr_time;
        curr_time = eve.time;

        if (run_proc == -1 && ptime < curr_time)
            idle_time += curr_time - ptime;

        // retrieve the process from the processes array (PCB typo)
        process *p = &processes[eve.process_id];

        switch (eve.event_type)
        {
        case 1: // process arrival
        {
#ifdef VERBOSE
            printf("%-*d: Process %d joins ready queue upon arrival\n", width, curr_time, p->id);
#endif
            enqueue(eve.process_id); // process is added to the ready queue
            break;
        }
        case 2: // running state
        {
            if (p->curr_burst == p->num_bursts - 1) // last burst
            {
                p->state = 4;                          // finished
                p->tat = curr_time - p->arrival_time;  // turnaround time
                int wait_tm = p->tat - p->tot_runtime; // wait time
                const int width = 10;
                int x = (p->tat * 100) / (float)p->tot_runtime;
                printf("%-10d: Process %-3d exits. Turnaround time = %-4d (%3d%%), Wait time = %-4d\n",
                       curr_time, p->id, p->tat, x, wait_tm);
                run_proc = -1; // no process running
            }
            else
            {
                int io = p->io_bursts[p->curr_burst];
                p->curr_burst++;
                p->rem_time = p->cpu_bursts[p->curr_burst];
                p->state = 3; // waiting state
                event x = {eve.process_id, curr_time + io, 3};
                insert(x);     // insert the event in the heap
                run_proc = -1; // no process running
            }
            break;
        }
        case 3: // waiting state
        {
#ifdef VERBOSE
            printf("%-*d: Process %d joins ready queue after IO completion\n", width, curr_time, p->id);
#endif
            p->state = 1;            // ready state
            enqueue(eve.process_id); // add to ready queue
            break;
        }
        case 4: // timeout
        {
#ifdef VERBOSE
            printf("%-*d: Process %d joins ready queue after timeout\n", width, curr_time, p->id);
#endif
            p->state = 1;            // ready state
            enqueue(eve.process_id); // add to ready queue
            run_proc = -1;           // no process running
            break;
        }
        }

        if (run_proc == -1)
        {
            run_proc = dequeue(); // get the process from the ready queue
            if (run_proc != -1)
            {
                // process is scheduled to run
                process *nxt = &processes[run_proc];
                nxt->state = 2;                        // running state
                int time = minimumm(q, nxt->rem_time); // time for which the process will run quantum or remaining time

#ifdef VERBOSE
                printf("%-*d: Process %d is scheduled to run for time %d\n", width, curr_time, nxt->id, time);
#endif
                event e;
                e.process_id = run_proc;
                e.time = curr_time + time;

                if (time == nxt->rem_time)
                    e.event_type = 2; // running
                else
                    e.event_type = 4; // timeout

                nxt->rem_time -= time;
                insert(e); // insert the event in the heap
            }
            else
            {
#ifdef VERBOSE
                printf("%-*d: CPU goes idle\n", width, curr_time);
#endif
            }
        }
    }

    float wait_avg = 0;
    for (int i = 0; i < num_proc; i++)
        wait_avg += processes[i].tat - processes[i].tot_runtime;
    wait_avg /= num_proc;

    printf("\n");
    printf("Average wait time = %.2f\n", wait_avg);
    printf("Total turnaround time = %d\n", curr_time);
    printf("CPU idle time = %d\n", idle_time);
    printf("CPU utilization = %.2f%%\n", (curr_time - idle_time) / (float)curr_time * 100);
}

int main()
{
    init();

    printf("**** FCFS Scheduling ****\n");
    schedule(PINF);

    printf("\n**** RR Scheduling with q = 10 ****\n");
    schedule(10);

    printf("\n**** RR Scheduling with q = 5 ****\n");
    schedule(5);

    return 0;
}