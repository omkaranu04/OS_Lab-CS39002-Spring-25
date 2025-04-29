#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>
#include "boardgen.c"

typedef struct
{
    int fdin;  // read
    int fdout; // write
} fds;

void initial_print()
{
    printf("\nCommands supported\n");
    printf("\tn\t\tStart new game\n");
    printf("\tp b c d\t\tPut digit d [1-9] at cell [0-8] of block b [0-8]\n");
    printf("\ts\t\tShow solution\n");
    printf("\th\t\tPrint this help message\n");
    printf("\tq\t\tQuit\n");
    printf("\nNumbering scheme for blocks and cells\n");
    for (int i = 0; i < 9; i += 3)
    {
        printf("\t+---+---+---+\n");
        printf("\t| %d | %d | %d |\n", i, i + 1, i + 2);
    }
    printf("\t+---+---+---+\n");
}
void row_get(int block, int *n1, int *n2)
{
    int row = block / 3;
    int row_start = row * 3;
    *n1 = row_start + ((block + 1) % 3);
    *n2 = row_start + ((block + 2) % 3);
}
void col_get(int block, int *n1, int *n2)
{
    *n1 = ((block + 3) % 9);
    *n2 = ((block + 6) % 9);
    if (*n1 / 3 == block / 3)
        *n1 = ((block + 6) % 9);
    if (*n2 / 3 == block / 3)
        *n2 = ((block + 3) % 9);
}
int main(int argc, char const *argv[])
{
    fds pipes[9];
    pid_t child_pids[9];
    int A[9][9], S[9][9];
    char command;

    initial_print();

    int std_out = dup(1);

    // create pipes for each block
    for (int i = 0; i < 9; i++)
    {
        // pipefd[1] -> write to the pipe
        // pipefd[0] -> read from the pipe
        int pipefd[2];
        if (pipe(pipefd) == -1)
        {
            printf("Pipe creation failed.\n");
            exit(1);
        }
        pipes[i].fdin = pipefd[0];
        pipes[i].fdout = pipefd[1];
    }

    // fork child processes
    for (int i = 0; i < 9; i++)
    {
        pid_t pid = fork();
        if (pid == -1)
        {
            printf("Fork failed.\n");
            exit(1);
        }
        if (pid == 0)
        {
            int row_n1, row_n2, col_n1, col_n2;
            row_get(i, &row_n1, &row_n2);
            col_get(i, &col_n1, &col_n2);

            char block_str[8], read_fd_str[8], write_fd_str[8];
            char rn1_fd_str[8], rn2_fd_str[8], cn1_fd_str[8], cn2_fd_str[8];
            char geom_str[20];

            sprintf(block_str, "%d", i);
            sprintf(read_fd_str, "%d", pipes[i].fdin);
            sprintf(write_fd_str, "%d", pipes[i].fdout);
            sprintf(rn1_fd_str, "%d", pipes[row_n1].fdout);
            sprintf(rn2_fd_str, "%d", pipes[row_n2].fdout);
            sprintf(cn1_fd_str, "%d", pipes[col_n1].fdout);
            sprintf(cn2_fd_str, "%d", pipes[col_n2].fdout);

            char title[10];
            sprintf(title, "Block %d", i);
            // xterm -T "Block 0" -fa Monospace -fs 15 -geometry "17x8+800+300" -bg "#331100"
            // -e ./block blockno bfdin bfdout rn1fdout rn2fdout cn1fdout cn2fdout
            int geom_x = 50 + (i % 3) * 500, geom_y = 50 + (i / 3) * 500; // for my laptop
            // int geom_x = 50 + (i % 3) * 250, geom_y = 50 + (i / 3) * 250; // for softy PC
            sprintf(geom_str, "17x8+%d+%d", geom_x, geom_y);
            execlp("xterm", "xterm",
                   "-T", title,
                   "-fa", "Monospace",
                   "-fs", "15",
                   "-geometry", geom_str,
                   "-bg", "#331100",
                   "-e", "./block",
                   block_str,
                   read_fd_str, write_fd_str,
                   rn1_fd_str, rn2_fd_str,
                   cn1_fd_str, cn2_fd_str,
                   NULL);
            printf("execlp failed\n");
            exit(1);
        }
        child_pids[i] = pid;
    }
    int flag = 0;
    while (1)
    {
        close(1);
        dup(std_out);
        printf("Foodoku> ");
        scanf(" %c", &command);
        if (command == 'h')
        {
            initial_print();
        }
        else if (command == 'n')
        {
            if (flag == 0)
                flag = 1;
            // function for new game
            newboard(A, S);

            for (int i = 0; i < 9; i++)
            {
                int block_row = (i / 3) * 3;
                int block_col = (i % 3) * 3;

                close(1);
                dup(pipes[i].fdout);
                printf("n");
                for (int j = 0; j < 3; j++)
                {
                    for (int k = 0; k < 3; k++)
                    {
                        printf(" %d", A[block_row + j][block_col + k]);
                    }
                }
                printf("\n");
            }
        }
        else if (command == 'p')
        {
            if (flag == 0)
            {
                int b, c, d;
                scanf("%d %d %d", &b, &c, &d);
                close(1);
                dup(std_out);
                printf("Start a new game first.\n");
                continue;
            }
            int b, c, d;
            scanf("%d %d %d", &b, &c, &d);
            if (b < 0 || b >= 9 || c < 0 || c >= 9 || d < 1 || d > 9)
            {
                close(1);
                dup(std_out);
                printf("Invalid parameters.\n");
            }
            else
            {
                close(1);
                dup(pipes[b].fdout);
                printf("p %d %d\n", c, d);
            }
        }
        else if (command == 's')
        {
            if (flag = 0)
            {
                close(1);
                dup(std_out);
                printf("Start a new game first.\n");
                continue;
            }
            if (flag = 1)
                flag = 0;
            for (int i = 0; i < 9; i++)
            {
                int block_row = (i / 3) * 3;
                int block_col = (i % 3) * 3;

                close(1);
                dup(pipes[i].fdout);

                printf("n");
                for (int j = 0; j < 3; j++)
                {
                    for (int k = 0; k < 3; k++)
                    {
                        printf(" %d", S[block_row + j][block_col + k]);
                    }
                }
                printf("\n");
            }
        }
        else if (command == 'q')
        {
            for (int i = 0; i < 9; i++)
            {
                close(1);
                dup(pipes[i].fdout);
                printf("q\n");
            }
            for (int i = 0; i < 9; i++)
            {
                waitpid(child_pids[i], NULL, 0);
            }
            close(1);
            dup(std_out);
            printf("Game Over\n");
            exit(0);
            break;
        }
        else
        {
            close(1);
            dup(std_out);
            printf("Invalid command. Type 'h' for help\n");
        }
    }
    return 0;
}