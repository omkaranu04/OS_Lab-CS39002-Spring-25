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
int minimumm(int a, int b)
{
    return a < b ? a : b;
}
typedef struct
{
    int id, arrival_time, num_bursts, curr_burst, rem_time, tot_runtime, tat;
    int cpu_bursts[MAXB], io_bursts[MAXB];
    int state;
} process;

typedef struct
{
    int process_id, time, event_type;
} event;

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

void schedule(int q)
{
    int curr_time = 0, run_proc = -1, idle_time = 0;
    r_frnt = 0, r_rear = 0;
    heap_sz = 0;

    for (int i = 0; i < num_proc; i++)
    {
        processes[i].curr_burst = 0;
        processes[i].rem_time = processes[i].cpu_bursts[0];
        processes[i].state = 1;
        event x = {i, processes[i].arrival_time, 1};
        insert(x);
    }

#ifdef VERBOSE
    const int width = 10;
    printf("%-*d: Starting\n", width, curr_time);
#endif

    while (heap_sz > 0)
    {
        event eve = min();
        int ptime = curr_time;
        curr_time = eve.time;

        if (run_proc == -1 && ptime < curr_time)
            idle_time += curr_time - ptime;

        process *p = &processes[eve.process_id];

        switch (eve.event_type)
        {
        case 1:
        {
#ifdef VERBOSE
            printf("%-*d: Process %d joins ready queue upon arrival\n", width, curr_time, p->id);
#endif
            enqueue(eve.process_id);
            break;
        }
        case 2:
        {
            if (p->curr_burst == p->num_bursts - 1) // last burst
            {
                p->state = 4;
                p->tat = curr_time - p->arrival_time;
                int wait_tm = p->tat - p->tot_runtime;
                const int width = 10;
                // make ceil in x while dividing
                // int x = ((p->tat * 100) / p->tot_runtime);
                int x = (p->tat * 100) / (float)p->tot_runtime;
                // printf("%-*d: Process %d exits. Turnaround time = %d (%d%%), Wait time = %d\n", width, curr_time, p->id, p->tat, x, wait_tm);
                printf("%-10d: Process %-3d exits. Turnaround time = %-4d (%3d%%), Wait time = %-4d\n",
                       curr_time, p->id, p->tat, x, wait_tm);
                run_proc = -1;
            }
            else
            {
                int io = p->io_bursts[p->curr_burst];
                p->curr_burst++;
                p->rem_time = p->cpu_bursts[p->curr_burst];
                p->state = 3;
                event x = {eve.process_id, curr_time + io, 3};
                insert(x);
                run_proc = -1;
            }
            break;
        }
        case 3:
        {
#ifdef VERBOSE
            printf("%-*d: Process %d joins ready queue after IO completion\n", width, curr_time, p->id);
#endif
            p->state = 1;
            enqueue(eve.process_id);
            break;
        }
        case 4:
        {
#ifdef VERBOSE
            printf("%-*d: Process %d joins ready queue after timeout\n", width, curr_time, p->id);
#endif
            p->state = 1;
            enqueue(eve.process_id);
            run_proc = -1;
            break;
        }
        }

        if (run_proc == -1)
        {
            run_proc = dequeue();
            if (run_proc != -1)
            {
                process *nxt = &processes[run_proc];
                nxt->state = 2;
                int time = minimumm(q, nxt->rem_time);

#ifdef VERBOSE
                printf("%-*d: Process %d is scheduled to run for time %d\n", width, curr_time, nxt->id, time);
#endif
                event e;
                e.process_id = run_proc;
                e.time = curr_time + time;

                if (time == nxt->rem_time)
                    e.event_type = 2;
                else
                    e.event_type = 4;

                nxt->rem_time -= time;
                insert(e);
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