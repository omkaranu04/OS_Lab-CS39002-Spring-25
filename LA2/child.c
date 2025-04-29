#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>

#define MAX_CHILDREN 1000

pid_t child_pids[MAX_CHILDREN];
int n, id, state[MAX_CHILDREN];

void handle_sigusr1() // SIGUSR1 has been allocated to print the status of the children
{
    for (int i = 0; i < n; i++)
    {
        // for printing neatly
        const int width = 11;
        const int padding = 3;

        if (i == 0 && i == id)
        {
            printf("|");
        }
        if (i == id)
        {
            if (state[id] == 3) // state 3 tells child is out of game so print blanks
            {
                printf("%*s%-*s|", padding, "", width - padding, "");
            }
            else if (state[id] == 1) // state 1 tells the child has caught the ball so print catch and move on
            {
                printf("%*s%-*s|", padding, "", width - padding, "CATCH");
                // after printing catch immediately change state so that "....." is printed further
                state[id] = 0;
            }
            else if (state[id] == 2) // state 2 tells the child has missed the ball so print miss and move on
            {
                printf("%*s%-*s|", padding, "", width - padding, "MISS");
                // after printing miss immediately change state so that "     " is printed further
                state[id] = 3;
            }
            else // the child is still playing and waiting for the next turn
            {
                printf("%*s%-*s|", padding, "", width - padding, ".....");
            }
        }
    }
    fflush(stdout);

    /* after the ball is thrown to the child, we pass SIGUSR1 to children further so as to give them the chance
    to print about their status, based on the status array. This continues in a circular manner always.
    If it is the last child then from dummycpid.txt read the pid of process D, and then send SIGINT to that process,
    in order to awaken the P process which was in the pause() state till all of this was happening */

    if (id < n - 1)
        kill(child_pids[id + 1], SIGUSR1);
    else
    {
        pid_t dummy_pid;
        FILE *file = fopen("dummycpid.txt", "r");
        if (!file)
        {
            perror("Failed to open dummycpid.txt");
            exit(1);
        }
        fscanf(file, "%d", &dummy_pid);
        fclose(file);
        kill(dummy_pid, SIGINT);
    }
}
void handle_sigusr2() // SIGUSR2 tells if the parent throws the ball to this child
{
    if (state[id] == 0) // state 0 is telling that the child is still playing the game
    {
        float chance = (float)rand() / (float)RAND_MAX; // generate random probability
        if (chance < 0.8)
        {
            state[id] = 1;            // catch, state updated to 2
            kill(getppid(), SIGUSR1); // catch, send SIGUSR1 to parent telling this child have caught the ball
        }
        else
        {
            state[id] = 2;            // miss, state updated to 3
            kill(getppid(), SIGUSR2); // miss, send SIGUSR2 to parent telling this child have missed the ball
        }
    }
}
void handle_sigint() // for the final child, to print the winner SIGINT is passed
{
    if (state[id] != 3) // if the child is not out of the game
    {
        // for printing neatly
        printf("\n+");
        for (int i = 0; i < (12 * n - 1); i++)
            printf("-");
        printf("+\n");

        for (int i = 0; i < n; i++)
        {
            printf(" ");
            const int width = 11;
            const int padding = 5;
            printf("%*s%-*d", padding, "", width - padding, i + 1);
        }

        printf("\n+++ Child %d: Yay! I am the winner!\n", id + 1);
        fflush(stdout);
    }
    exit(0);
}
int main(int argc, char const *argv[])
{
    srand(time(NULL) ^ getpid() ^ (getpid() << 16)); // seed for random number generation

    // setting signal handler
    signal(SIGUSR1, handle_sigusr1);
    signal(SIGUSR2, handle_sigusr2);
    signal(SIGINT, handle_sigint);

    sleep(3); // wait for parent to write the pid of the dummy process

    FILE *file = fopen("childpid.txt", "r");
    if (!file)
    {
        perror("Failed to open childpid.txt");
        exit(1);
    }
    fscanf(file, "%d", &n);
    // update the id with the child number with which the process was created
    for (int i = 0; i < n; i++)
    {
        fscanf(file, "%d", &child_pids[i]);
        if (child_pids[i] == getpid())
            id = i;
    }
    fclose(file);

    // sleep till signal is received
    while (1)
        pause();

    return 0;
}