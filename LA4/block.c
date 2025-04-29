#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>

// A for storing the original block, B for current state of the block
int A[3][3], B[3][3];
int blockno, bfdin, bfdout, rn1fdout, rn2fdout, cn1fdout, cn2fdout;
// out -> pipe ka in
// in  -> pipe ka out

// helper for drawing
void draw_function()
{
    // clear the screen
    printf("\033[H\033[J");

    for (int i = 0; i < 3; i++)
    {
        printf(" +---+---+---+\n");
        printf(" │");
        for (int j = 0; j < 3; j++)
        {
            if (B[i][j] == 0)
            {
                printf("   |");
            }
            else
            {
                printf(" %d |", B[i][j]);
            }
        }
        printf("\n");
        if (i == 2)
            printf(" +---+---+---+\n");
    }
}
int row_check_receive(int row, int d)
{
    int std_out = dup(1);

    close(1);
    dup(rn1fdout);
    printf("r %d %d %d\n", row, d, bfdout);

    close(1);
    dup(rn2fdout);
    printf("r %d %d %d\n", row, d, bfdout);

    close(1);
    dup(std_out);

    int res1, res2;
    scanf("%d", &res1);
    scanf("%d", &res2);

    return res1 || res2;
}
int col_check_receive(int col, int d)
{
    int std_out = dup(1);

    close(1);
    dup(cn1fdout);
    printf("c %d %d %d\n", col, d, bfdout);

    close(1);
    dup(cn2fdout);
    printf("c %d %d %d\n", col, d, bfdout);

    close(1);
    dup(std_out);

    int res1, res2;
    scanf("%d", &res1);
    scanf("%d", &res2);

    return res1 || res2;
}
void handle_p_command(int cell, int d)
{
    int row = cell / 3;
    int col = cell % 3;
    if (A[row][col] != 0)
    {
        printf("Read-only cell\n");
        sleep(2);
        // function to draw the block
        draw_function();
        return;
    }

    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            if (B[i][j] == d)
            {
                printf("Block conflict\n");
                sleep(2);
                // function to draw the block
                draw_function();
                return;
            }
        }
    }

    if (row_check_receive(row, d))
    {
        printf("Row conflict\n");
        sleep(2);
        // function to draw the block
        draw_function();
        return;
    }

    if (col_check_receive(col, d))
    {
        printf("Column conflict\n");
        sleep(2);
        // function to draw the block
        draw_function();
        return;
    }

    B[row][col] = d;
    // function to draw the block
    draw_function();
}
void row_check_signal(int row, int d, int fdout)
{
    int flag = 0;
    for (int i = 0; i < 3; i++)
    {
        if (B[row][i] == d)
        {
            flag = 1;
            break;
        }
    }
    int std_out = dup(1);
    close(1);
    dup(fdout);
    printf("%d\n", flag);
    close(1);
    dup(std_out);
}
void col_check_signal(int col, int d, int fdout)
{
    int flag = 0;
    for (int i = 0; i < 3; i++)
    {
        if (B[i][col] == d)
        {
            flag = 1;
            break;
        }
    }
    int std_out = dup(1);
    close(1);
    dup(fdout);
    printf("%d\n", flag);
    close(1);
    dup(std_out);
}
int main(int argc, char const *argv[])
{
    if (argc != 8)
    {
        fprintf(stderr, "Usage : %s blockno bfdin bfdout "
                        "rn1fdout rn2fdout cn1fdout cn2fdout\n",
                argv[0]);
        exit(1);
    }
    blockno = atoi(argv[1]);
    bfdin = atoi(argv[2]);
    bfdout = atoi(argv[3]);
    rn1fdout = atoi(argv[4]);
    rn2fdout = atoi(argv[5]);
    cn1fdout = atoi(argv[6]);
    cn2fdout = atoi(argv[7]);

    dup(0);
    close(0);
    dup(bfdin);

    printf("Block %d ready\n", blockno);
    char command;
    while (scanf(" %c", &command) != EOF)
    {
        if (command == 'n')
        {
            // read the initial block state
            for (int i = 0; i < 3; i++)
            {
                for (int j = 0; j < 3; j++)
                {
                    scanf("%d", &A[i][j]);
                    B[i][j] = A[i][j];
                }
            }
            // function to draw the block
            draw_function();
        }
        else if (command == 'p')
        {
            // read the p command
            int cell, digit;
            scanf("%d %d", &cell, &digit);
            // function to handle the place
            handle_p_command(cell, digit);
        }
        else if (command == 'r')
        {
            // process the row request command
            int row, digit, fdout;
            scanf("%d %d %d", &row, &digit, &fdout);
            row_check_signal(row, digit, fdout);
        }
        else if (command == 'c')
        {
            // process the column request command
            int col, digit, fdout;
            scanf("%d %d %d", &col, &digit, &fdout);
            col_check_signal(col, digit, fdout);
        }
        else if (command == 'q')
        {
            // quit the whole process
            printf("\nBye ...\n");
            sleep(2);
            close(bfdin);
            close(bfdout);
            close(rn1fdout);
            close(rn2fdout);
            close(cn1fdout);
            close(cn2fdout);
            exit(0);
        }
        else
        {
            fprintf(stderr, "Block %d. Unknown command. Type 'h' for help.\n", blockno);
        }
    }

    close(bfdin);
    close(bfdout);
    close(rn1fdout);
    close(rn2fdout);
    close(cn1fdout);
    close(cn2fdout);
    return 0;
}