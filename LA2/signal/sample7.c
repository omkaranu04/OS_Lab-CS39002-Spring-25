/*
    OS Laboratory Assignment 2
    Anit Mangal
    21CS10005
*/

#include <stdio.h>     // Standard I/O operations
#include <stdlib.h>    // Standard library functions
#include <unistd.h>    // For process control (fork, exec, etc.)
#include <sys/types.h> // For data types used in system calls
#include <sys/wait.h>  // For waiting on processes
#include <string.h>    // For string manipulation
#include <signal.h>    // For signal handling
#include <time.h>      // For generating random values

// Compact string definitions for process statuses
#define SELF "mgr"       // Manager process
#define FINISHED "fin"   // Finished process
#define SUSPENDED "sus"  // Suspended process
#define TERMINATED "ter" // Terminated process
#define KILLED "kil"     // Killed process

// Global variables
int currpr = 0;    // Index of the currently running process in the process table
char pt[11][4][6]; // Process table (pid, pgid, status, name)

// Signal Handler function to manage SIGINT and SIGTSTP signals
void sigHan(int sig)
{
    if (currpr)
    { // If a process is currently running
        if (sig == SIGINT)
        {                                       // Handle SIGINT (^C)
            kill(atoi(pt[currpr][0]), SIGKILL); // Kill the process
            strcpy(pt[currpr][2], TERMINATED);  // Update its status to TERMINATED
            printf("\n");
            currpr = 0; // Reset current process
        }
        else if (sig == SIGTSTP)
        {                                       // Handle SIGTSTP (^Z)
            kill(atoi(pt[currpr][0]), SIGTSTP); // Suspend the process
            strcpy(pt[currpr][2], SUSPENDED);   // Update its status to SUSPENDED
            printf("\n");
            currpr = 0; // Reset current process
        }
    }
    else
    {
        // If no process is running, print prompt
        printf("mgr> ");
    }
}

int main()
{
    // Set up signal handlers for SIGINT (^C) and SIGTSTP (^Z)
    signal(SIGINT, sigHan);
    signal(SIGTSTP, sigHan);

    // Seed the random number generator
    srand((unsigned int)time(NULL));

    // Populate the first entry of the process table with manager process info
    sprintf(pt[0][0], "%d", getpid());          // PID of manager
    sprintf(pt[0][1], "%d", getpgid(getpid())); // PGID of manager
    strcpy(pt[0][2], SELF);                     // Status is SELF
    strcpy(pt[0][3], "mgr");                    // Name is "mgr"

    int numJobs = 0; // Number of processes currently in the process table
    int status;      // Used to store process status information

    // Main loop to handle user commands
    while (1)
    {
        char inp;          // Variable to store user input
        printf("mgr> ");   // Print prompt
        scanf("%s", &inp); // Read user input

        if (inp == 'p')
        {
            // Print process table
            printf("NO\tPID\t\tPGID\t\tSTATUS\t\t\tNAME\n");
            for (int i = 0; i <= numJobs; i++)
            {
                printf("%d\t", i);          // Process number
                printf("%s\t\t", pt[i][0]); // PID
                printf("%s\t\t", pt[i][1]); // PGID

                // Print status in human-readable format
                if (strcmp(pt[i][2], SELF) == 0)
                    printf("SELF\t\t\t");
                else if (strcmp(pt[i][2], FINISHED) == 0)
                    printf("FINISHED\t\t");
                else if (strcmp(pt[i][2], SUSPENDED) == 0)
                    printf("SUSPENDED\t\t");
                else if (strcmp(pt[i][2], TERMINATED) == 0)
                    printf("TERMINATED\t\t");
                else if (strcmp(pt[i][2], KILLED) == 0)
                    printf("KILLED\t\t\t");

                printf("%s\n", pt[i][3]); // Process name
            }
        }
        else if (inp == 'r')
        {
            // Run a new job
            if (numJobs == 10)
            { // Check if process table is full
                printf("Process table is full. Quitting...\n");
                exit(1);
            }

            int pid; // PID of the new process
            char arg[2];
            arg[1] = '\0';              // Null-terminate the argument string
            arg[0] = 'A' + rand() % 26; // Generate a random argument (A-Z)

            if ((pid = fork()) == 0)
            {
                // In child process
                execl("./job", "blank", arg, NULL); // Execute the job program
            }
            else
            {
                // In parent process
                numJobs++;        // Increment the job count
                currpr = numJobs; // Set current process to the new job
                setpgid(pid, 0);  // Set group ID to PID for the child

                // Populate the process table with new job details
                sprintf(pt[numJobs][0], "%d", pid);          // PID
                sprintf(pt[numJobs][1], "%d", getpgid(pid)); // PGID
                char nm[6];
                strcpy(nm, "job ");
                nm[4] = arg[0];
                nm[5] = '\0';
                strcpy(pt[numJobs][3], nm); // Name

                waitpid(pid, &status, WUNTRACED); // Wait for the job to terminate or stop

                // If the process exited normally and is still the current process
                if (WEXITSTATUS(status) == 0 && currpr)
                {
                    strcpy(pt[currpr][2], FINISHED); // Mark it as finished
                    currpr = 0;                      // Reset current process
                }
            }
        }
        else if (inp == 'c')
        {
            // Continue a suspended job
            int cnt = 0;        // Counter for suspended jobs
            int candidates[10]; // Array to store indices of suspended jobs

            for (int i = 1; i <= numJobs; i++)
            {
                if (strcmp(pt[i][2], SUSPENDED) == 0)
                    candidates[cnt++] = i;
            }

            if (cnt)
            {
                printf("Suspended jobs: ");
                for (int i = 0; i < cnt; i++)
                {
                    printf("%d", candidates[i]);
                    if (i < cnt - 1)
                        printf(", ");
                    else
                        printf(" (Pick one): ");
                }

                int num;
                scanf("%d", &num); // Get user choice

                int check = 0; // Validate the choice
                for (int i = 0; i < cnt; i++)
                    if (candidates[i] == num)
                        check = 1;

                if (check)
                {
                    currpr = num;                                  // Set current process
                    kill(atoi(pt[num][0]), SIGCONT);               // Send continue signal
                    waitpid(atoi(pt[num][0]), &status, WUNTRACED); // Wait for process

                    if (WEXITSTATUS(status) == 0 && currpr)
                    {
                        strcpy(pt[currpr][2], FINISHED); // Mark as finished
                        currpr = 0;                      // Reset current process
                    }
                }
                else
                {
                    printf("Invalid job number.\n");
                }
            }
        }
        else if (inp == 'k')
        {
            // Kill a suspended job
            int cnt = 0;
            int candidates[10];

            for (int i = 1; i <= numJobs; i++)
            {
                if (strcmp(pt[i][2], SUSPENDED) == 0)
                    candidates[cnt++] = i;
            }

            if (cnt)
            {
                printf("Suspended jobs: ");
                for (int i = 0; i < cnt; i++)
                {
                    printf("%d", candidates[i]);
                    if (i < cnt - 1)
                        printf(", ");
                    else
                        printf(" (Pick one): ");
                }

                int num;
                scanf("%d", &num);

                int check = 0;
                for (int i = 0; i < cnt; i++)
                    if (candidates[i] == num)
                        check = 1;

                if (check)
                {
                    kill(atoi(pt[num][0]), SIGKILL); // Kill the process
                    strcpy(pt[num][2], KILLED);      // Mark as killed
                    currpr = 0;                      // Reset current process
                }
                else
                {
                    printf("Invalid job number.\n");
                }
            }
        }
        else if (inp == 'h')
        {
            // Print help message
            printf("\tCommand\t: Action\n");
            printf("\t   c\t: Continue a suspended job\n");
            printf("\t   h\t: Print this help message\n");
            printf("\t   k\t: Kill a suspended job\n");
            printf("\t   p\t: Print the process table\n");
            printf("\t   q\t: Quit\n");
            printf("\t   r\t: Run a new job\n");
        }
        else if (inp == 'q')
        {
            // Quit the manager, killing all suspended jobs
            for (int i = 1; i <= numJobs; i++)
            {
                if (strcmp(pt[i][2], SUSPENDED) == 0)
                    kill(atoi(pt[i][0]), SIGKILL);
            }
            exit(0);
        }
    }
}