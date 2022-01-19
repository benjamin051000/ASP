#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define DEBUG 1

typedef struct
{
    int id;
    char *topic;
    int accum_score; // This ID's accumulated score for this topic.
} total_score_t;

typedef enum
{
    ID_idx = 0,
    TOPIC_idx,
    SCORE_idx
} TokenIndex;

int main(void)
{
    // Buffer to hold lines as they come in via stdin.
    const unsigned BUF_SIZE = 100;
    char text[BUF_SIZE];
    // Used for token parsing.
    const char delims[] = "(), \n";

    TokenIndex token_idx = ID_idx;

    // Array of entries for accumulated scores
    char *current_topic;
    int current_score = 0;

    while (true)
    {
        // Get next line, delimited by newline.
        fgets(text, BUF_SIZE, stdin);

#if DEBUG
        printf("Got \"%s\"\n", text);
#endif

        // If there is nothing else to read, we're done.
        if (strcmp(text, "Done.\n") == 0)
        {
            break;
        }

        // Temp variables to construct the entry
        int id;
        char *topic;
        int score;

        // Otherwise, parse the tokens.
        // Get first token.
        char *token = strtok(text, delims);

        while (token != NULL)
        {

            switch (token_idx)
            {
            case ID_idx:
                id = atoi(token);
                token_idx == TOPIC_idx;
                break;

            case TOPIC_idx:
                topic = token;
                token_idx = SCORE_idx;
                break;

            case SCORE_idx:
                score = atoi(token);

                // Construct the entry

                
                token_idx = ID_idx;
                break;
            } // end of switch

            // Get next token
            token = strtok(NULL, delims);
        } // end of while (token != NULL)
    
    } // end of while (true)

    printf("Done.\n");

    return 0;
}
