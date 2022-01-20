#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG 0

/**
 * @brief Read a text file into a buffer.
 * WARNING: You must free() the returned char*!
 */
char *read(void)
{
    const int BUF_SIZE = 1500;
    
    char *buffer = malloc(BUF_SIZE * sizeof *buffer);
    int index = 0;
    
    int c;
    while((c = fgetc(stdin)) != EOF) {
        buffer[index++] = c;
    };
    
    return buffer;
}

typedef struct entry
{
    char *id;
    char *action;
    char *topic;
} entry_t;

int main(void)
{

    char *text = read();

#if DEBUG
    printf("text: \"%s\"\n\n", text);
#endif

    // List of entries
    entry_t entries[100];
    unsigned entries_size = 0;

    // Parse the text to obtain tokens.
    const char delims[] = "(), \n";

#if DEBUG
    printf("Tokens: ");
#endif

    char *token = strtok(text, delims); // TODO where are these tokens stored? Heap?
    int token_i = 0;

    while (token != NULL)
    {
#if DEBUG
        printf("\"%s\", ", token);
#endif
        char * id;
        char *action;
        char *topic;

        if (token_i == 0)
        {
            // This is the first of the three tuple entries, which is the ID.
            id = token;
            token_i++;
        }
        else if (token_i == 1)
        {
            // This is the second tuple entry, which is the category.
            action = token;
            token_i++;
        }
        else
        {
            topic = token;
            token_i = 0;

            // Construct the entry
            entries[entries_size++] = (entry_t){id, action, topic};
#if DEBUG
            printf("\n\tentries[%d] = {%d, %s, %s}\n", entries_size-1, id, action, topic);
#endif
        }

        // Get next token
        token = strtok(NULL, delims);
    } // End of while


    // Print all the entries
#if DEBUG
    printf("\n\nEntries:\n");
#endif

    for (unsigned i = 0; i < entries_size; i++)
    {
        entry_t *e = &entries[i];

#if DEBUG
        printf("Read entry {%d, %s, %s}\n", e->id, e->action, e->topic);
#endif

        // Get cooresponding score from user action
        int score;
        if (strcmp(e->action, "P") == 0) // Post
            score = 50;
        else if (strcmp(e->action, "L") == 0) // Like
            score = 20;
        else if (strcmp(e->action, "D") == 0) // Dislike
            score = -10;
        else if (strcmp(e->action, "C") == 0) // Comment
            score = 30;
        else if (strcmp(e->action, "S") == 0) // Share
            score = 40;

        printf("(%s, %s, %d)\n", e->id, e->topic, score);
    }

    free(text);
    return 0;
}
