#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>

#define MAX_CHILDREN 1000

void trigger_print();
void throw_ball();
void print_header();

pid_t child_pids[MAX_CHILDREN];
int n, curr_player = 0, rem_players, child_status[MAX_CHILDREN];

void handle_sigusr1() // ball caught handling
{
    // print after each round
    trigger_print();

    // if more than 1 player remaining, then throw the ball
    if (rem_players > 1)
        throw_ball();
    else
    {
        // last player standing, then send SIGINT to the child process of that player
        print_header();
        for (int i = 0; i < n; i++)
        {
            kill(child_pids[i], SIGINT);
        }
        exit(0);
    }
}
void handle_sigusr2() // ball missed handling
{
    // print after each round
    trigger_print();

    child_status[curr_player] = 2; // mark child as out
    --rem_players;                 // decrease the remaining players

    // if more than 1 player remaining, then throw the ball
    if (rem_players > 1)
        throw_ball();
    else
    {
        // last player standing, then send SIGINT to the child process of that player
        for (int i = 0; i < n; i++)
        {
            kill(child_pids[i], SIGINT);
        }
        exit(0);
    }
}

// printing the header after every turn
void print_header()
{
    printf("\n+");
    for (int i = 0; i < (12 * n - 1); i++)
        printf("-");
    printf("+");
    fflush(stdout);
}

// for triggering the print action after each round
void trigger_print()
{
    print_header();
    printf("\n");

    // invoke a dummy process, so that till all the children print their status let the parent process sleep (be in pause)
    pid_t dummy_pid = fork();
    if (dummy_pid == 0)
    {
        execl("./dummy", "./dummy", NULL);
        perror("execl failed");
        exit(1);
    }
    // store it in dummycpid.txt so that child process can also access the process somehow to awaken parent process
    FILE *file = fopen("dummycpid.txt", "w");
    if (!file)
    {
        perror("Failed to open dummycpid.txt");
        exit(1);
    }
    fprintf(file, "%d\n", dummy_pid);
    fclose(file);

    kill(child_pids[0], SIGUSR1); // start status printing
    waitpid(dummy_pid, NULL, 0);  // wait for printing, through dummy process
}

void throw_ball() // provess to throw the ball to the next player who is still in game
{
    int o_player = curr_player;
    do
    {
        curr_player = (curr_player + 1) % n;
    } while (child_status[curr_player] == 2 && curr_player != o_player);
    // the child haven't missed the ball

    if (rem_players > 1)
    {
        kill(child_pids[curr_player], SIGUSR2); // throw the ball to the player
    }
}

int main(int argc, char const *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Expected: %s <number_of_children>\n", argv[0]);
        exit(1);
    }

    n = atoi(argv[1]);

    if (n <= 1 || n > MAX_CHILDREN)
    {
        if (n == 1)
        {
            printf("+++ Child 1: Yay! I am the winner!\n");
            exit(0);
        }
        fprintf(stderr, "Number of children must be between 1 and %d\n", MAX_CHILDREN);
        exit(1);
    }

    rem_players = n;

    for (int i = 0; i < n; i++)
    {
        child_status[i] = 3; // playing
    }

    // set signal handlers
    signal(SIGUSR1, handle_sigusr1);
    signal(SIGUSR2, handle_sigusr2);

    // create child processes for the command line argument n, store each of the pids in the childpid.txt file
    FILE *file = fopen("childpid.txt", "w");
    if (!file)
    {
        perror("Failed to open childpid.txt");
        exit(1);
    }
    fprintf(file, "%d\n", n);
    for (int i = 0; i < n; i++)
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            // for the child process run child.c
            execl("./child", "./child", NULL);
            perror("execl failed");
            exit(1);
        }
        child_pids[i] = pid;
        fprintf(file, "%d\n", pid);
    }
    fclose(file);

    printf("Parent: %d child processes created\n", n);
    printf("Parent: Waiting for child processes to read child database\n");
    fflush(stdout);

    sleep(3); // wait for the children to read the childpid.txt file

    for (int i = 0; i < n; i++)
    {
        // for printing neatly
        printf(" ");
        const int width = 11;
        const int padding = 5;
        printf("%*s%-*d", padding, "", width - padding, i + 1);
    }

    kill(child_pids[0], SIGUSR2); // begin the game

    // wait till signal is received
    while (1)
        pause();
    return 0;
}