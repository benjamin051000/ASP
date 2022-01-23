#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <wait.h>

#define DEBUG 0

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

void mapper_proc(int pipein[2], int pipeout[2])
{
    // Close unused pipes for this process.
    close(pipein[PIPE_W]);
    close(pipeout[PIPE_R]);

    // Input from the text
    if(dup2(pipein[PIPE_R], STDIN_FILENO) == -1) {
        printf("[child] ERROR dup2(): Couldn't redirect mapper stdin.\n");
        exit(EXIT_FAILURE);
    }

    // Output to the pipe to the reducer.
    if(dup2(pipeout[PIPE_W], STDOUT_FILENO) == -1) {
        printf("[child] ERROR dup2(): Couldn't redirect mapper stdout.\n");
        exit(EXIT_FAILURE);
    }
#if DEBUG
    printf("[child] running mapper...\n");
#endif
    // Run mapper
    // argv[0] is the process name (for ps)
    execl("./build/mapper", "./build/mapper", (char *)NULL);

    printf("ERROR: Couldn't execute reducer.\n");
    exit(EXIT_FAILURE);
}

void reducer_proc(int pipein[2])
{
    // Close unused pipes for this process.
    close(pipein[PIPE_W]);

    // Input from the pipe from the mapper.
    if(dup2(pipein[PIPE_R], STDIN_FILENO) == -1) {
        printf("[child] ERROR dup2(): Couldn't redirect reducer stdin.\n");
        exit(EXIT_FAILURE);
    }
#if DEBUG
    printf("[child] running reducer...\n");
#endif
    // Run reducer
    execl("./build/reducer", "./build/reducer", (char *)NULL);

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

#if DEBUG
    printf("[combiner] text: %s\n", text);
#endif

    write(comb2map[PIPE_W], text, strlen(text)+1);

#if DEBUG
    printf("Starting mapper and reducer...\n");
#endif

    // Send text as the stdin
    pid_t mapper_pid, reducer_pid;

    switch ((mapper_pid = fork()))
    {
    case -1:
        printf("ERROR: Couldn't fork() properly.\n");
        exit(EXIT_FAILURE);

    case 0: // Child process
        mapper_proc(comb2map, map2red);
        break;

    default: // Parent process
        // Close pipe for parent.
        close(comb2map[PIPE_R]);
        close(comb2map[PIPE_W]);

        switch ((reducer_pid = fork()))
        {
        case -1:
            printf("ERROR: Couldn't fork() properly.\n");
            exit(EXIT_FAILURE);

        case 0:
            reducer_proc(map2red);
            break;

        default:
            // Close pipes for parent.
            close(map2red[PIPE_R]);
            close(map2red[PIPE_W]);

            // Wait for both processes to finish.
            wait(&mapper_pid);
            wait(&reducer_pid);
            #if DEBUG
            printf("[combiner] Mapper and reducer finished.\n");
            #endif
        }
    }
#if DEBUG
    printf("[combiner] Done.\n");
#endif
    return 0;
}
