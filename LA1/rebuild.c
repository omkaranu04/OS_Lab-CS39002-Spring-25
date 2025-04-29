#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdbool.h>

#define MAX_FOODULES 500
#define MAX_DEPENDENCIES 500

void initialise_done_file(char *donefile)
{
    FILE *file = fopen(donefile, "w");
    if (!file)
    {
        perror("Failed to open done.txt for writing");
        exit(1);
    }
    for (int i = 1; i <= MAX_FOODULES; i++)
    {
        fprintf(file, "0");
    }
    fprintf(file, "\n");
    fclose(file);
}

void read_done_file(char *donefile, int visited[], int n)
{
    FILE *file = fopen(donefile, "r");
    if (!file)
    {
        perror("Failed to open done.txt for reading");
        exit(1);
    }
    for (int i = 1; i <= n; i++)
    {
        fscanf(file, "%d", &visited[i]);
    }
    fclose(file);
}

void write_done_file(char *donefile, int visited[], int n)
{
    FILE *file = fopen(donefile, "w");
    if (!file)
    {
        perror("Failed to open done.txt for writing");
        exit(1);
    }
    for (int i = 1; i <= n; i++)
    {
        fprintf(file, "%d ", visited[i]);
    }
    fprintf(file, "\n");
    fclose(file);
}

void print_out(char *rebuilt_from, int foodule)
{
    if (strlen(rebuilt_from) > 0)
    {
        rebuilt_from[strlen(rebuilt_from) - 2] = '\0';
        char *foo0 = "foo0";
        char *found = strstr(rebuilt_from, foo0);
        if (found)
        {
            int len = strlen(foo0);
            for (int i = 0; i < len; i++)
            {
                found[i] = ' ';
            }
        }
        char *foo = "foo";
        found = strstr(rebuilt_from, foo);
        if (!found)
            printf("foo%d rebuilt\n", foodule);
        else
            printf("foo%d rebuilt from %s\n", foodule, rebuilt_from);
    }
}

void solve(int foodule, int is_root)
{
    char *depfile = "foodep.txt";
    char *donefile = "done.txt";
    int n; // total number of foodules
    FILE *file;

    if (is_root)
    {
        initialise_done_file(donefile);
    }

    file = fopen(depfile, "r");
    if (!file)
    {
        perror("failed opening foodep.txt\n");
        exit(1);
    }
    fscanf(file, "%d", &n);

    // dependencies of current foodule
    char line[1000];
    int dep[MAX_DEPENDENCIES], dep_cnt = 0;
    while (fgets(line, sizeof(line), file))
    {
        int curr;
        char *tok = strtok(line, ": ");
        curr = atoi(tok);
        if (curr == foodule)
        {
            tok = strtok(NULL, ": ");
            while (tok)
            {
                dep[dep_cnt++] = atoi(tok);
                tok = strtok(NULL, ": ");
            }
            break;
        }
    }
    fclose(file);

    int visited[MAX_FOODULES + 1] = {0};
    read_done_file(donefile, visited, n);

    char rebuilt_from[2000] = "";
    for (int i = 0; i < dep_cnt; i++)
    {
        int d = dep[i];
        if (!visited[d])
        {
            pid_t pid = fork();
            if (pid == 0)
            {
                char dep_str[10];
                sprintf(dep_str, "%d", d);
                execl("./rebuild", "rebuild", dep_str, "child", NULL);
                perror("Failed to exec rebuild");
                exit(1);
            }
            else
            {
                wait(NULL); 
            }
        }
        char temp[10];
        sprintf(temp, "foo%d, ", d);
        strcat(rebuilt_from, temp);
    }

    read_done_file(donefile, visited, n);
    visited[foodule] = 1;
    write_done_file(donefile, visited, n);

    print_out(rebuilt_from, foodule);
}

int main(int argc, char const *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "no command line argument found\n");
        return 1;
    }
    int fodule = atoi(argv[1]);
    int is_root = (argc == 2); // for accessing the done array

    solve(fodule, is_root);

    return 0;
}