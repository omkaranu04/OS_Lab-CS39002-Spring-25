#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <pwd.h>

#define MAX_USERS 1000
#define SIZE 1024

typedef struct
{
    unsigned int uid;
    char login[SIZE];
} Users;

Users users[MAX_USERS];
int users_cnt = 0;

void get_user_info()
{
    FILE *file = fopen("/etc/passwd", "r");
    if (file == NULL)
    {
        perror("Error opening /etc/passwd");
        return;
    }

    char line[SIZE];
    while (fgets(line, sizeof(line), file) && users_cnt < MAX_USERS)
    {
        char *username = strtok(line, ":");
        strtok(NULL, ":"); // skip password
        char *uid_name = strtok(NULL, ":");

        if (username && uid_name)
        {
            unsigned int uid = atoi(uid_name);
            users[users_cnt].uid = uid;
            strncpy(users[users_cnt].login, username, sizeof(users[users_cnt].login) - 1);
            users[users_cnt].login[sizeof(users[users_cnt].login) - 1] = '\0';
            users_cnt++;
        }
    }

    fclose(file);
}

const char *get_id(unsigned int uid)
{
    for (int i = 0; i < users_cnt; i++)
    {
        if (users[i].uid == uid)
            return users[i].login;
    }

    struct passwd *pwd = getpwuid(uid);
    if (pwd)
    {
        if (users_cnt < MAX_USERS)
        {
            users[users_cnt].uid = uid;
            strncpy(users[users_cnt].login, pwd->pw_name, sizeof(users[users_cnt].login) - 1);
            users[users_cnt].login[sizeof(users[users_cnt].login) - 1] = '\0';
            users_cnt++;
        }
        return pwd->pw_name;
    }
    return "unknown";
}

void findall(const char *directory, const char *extension, int *cnt)
{
    DIR *dir;
    struct dirent *entry;
    struct stat filestat;
    char path[SIZE];

    dir = opendir(directory);
    if (dir == NULL)
    {
        fprintf(stderr, "Error opening directory %s: %s\n", directory, strerror(errno));
        return;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        // skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name);

        if (lstat(path, &filestat) == -1)
        {
            fprintf(stderr, "Error getting file info for %s: %s\n", path, strerror(errno));
            continue;
        }
        if (S_ISDIR(filestat.st_mode))
            findall(path, extension, cnt);
        else if (S_ISREG(filestat.st_mode))
        {
            unsigned long lname = strlen(entry->d_name);
            unsigned long lextension = strlen(extension);

            if (lname > lextension + 1 && entry->d_name[lname - lextension - 1] == '.' && strcasecmp(entry->d_name + lname - lextension, extension) == 0)
            {
                const char *owner = get_id(filestat.st_uid);
                if (owner == NULL)
                {
                    fprintf(stderr, "Error getting owner for UID %u\n", filestat.st_uid);
                    continue;
                }

                printf("%4d : %-10s %-12ld %s\n",
                       (*cnt) + 1,
                       owner,
                       (long)filestat.st_size,
                       path);
                (*cnt)++;
            }
        }
    }
    closedir(dir);
}

int main(int argc, char const *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <directory> <extension>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *directory = argv[1];
    const char *extension = argv[2];

    get_user_info();
    printf("NO   : OWNER       SIZE         NAME\n");
    printf("--   : -----       ----         ----\n");

    int nofiles = 0;
    findall(directory, extension, &nofiles);

    printf("+++ %d files match the extension %s\n", nofiles, extension);
    return 0;
}