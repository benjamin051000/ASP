#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define PIPE_R 0
#define PIPE_W 1

/**
 * @brief Read a text file into a buffer.
 * WARNING: You must free() the returned char*!
 */
char *readTextFile(void)
{
    const int BUF_SIZE = 1500;

    char *buffer = malloc(BUF_SIZE * sizeof *buffer);

    int index = 0;

    int c;
    while ((c = fgetc(stdin)) != EOF)
    {
        buffer[index++] = c;
    };

    return buffer;
}

void reducer_proc(pid_t pid, int pipein)
{
    // Input from the pipe from the mapper.
    dup2(STDIN_FILENO, pipein);

    // Run reducer
    char *args[] = {NULL};
    execv("./build/reducer", args);

    printf("ERROR: Couldn't execute reducer.\n");
    exit(EXIT_FAILURE);
}

void mapper_proc(int pipein, int pipeout)
{
    // Input from the text
    dup2(STDIN_FILENO, pipein);

    // Output to the pipe to the reducer.
    dup2(STDOUT_FILENO, pipeout);

    // Run mapper
    char *args[] = {NULL};
    execv("./build/mapper", args);

    printf("ERROR: Couldn't execute reducer.\n");
    exit(EXIT_FAILURE);
}

int main(void)
{
    // Set up pipes for IPC

    // First pipe, from this process to the mapper
    int comb2map[2];
    if (pipe(comb2map) == -1)
    {
        printf("ERROR: Couldn't make comb2map pipe.\n");
        exit(EXIT_FAILURE);
    }

    // Second pipe, from mapper to reducer
    int map2red[2];
    if (pipe(map2red) == -1)
    {
        printf("ERROR: Couldn't make map2red pipe.\n");
        exit(EXIT_FAILURE);
    }

    // Read text input, and write it to the first pipe.
    char *text = readTextFile();
    write(comb2map[PIPE_W], text, strlen(text));
    free(text);

    // Send text as the stdin
    pid_t result = fork();

    switch (result)
    {
    case -1:
        printf("ERROR: Couldn't fork() properly.\n");
        exit(EXIT_FAILURE);

    case 0: // Child process
        mapper_proc(comb2map[PIPE_R], map2red[PIPE_W]);
        break;

    default: // Parent process
        reducer_proc(result, map2red[PIPE_R]);
    }

    return 0;
}
