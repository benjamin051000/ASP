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
} score_entry_t;

typedef enum
{
    ID_idx = 0,
    TOPIC_idx,
    SCORE_idx
} TokenIndex;

int main(void)
{
    // Buffer to hold lines as they come in via stdin.
    const unsigned TEXT_BUF_SIZE = 100;
    char text[TEXT_BUF_SIZE];
    // Used for token parsing.
    const char delims[] = "(), \n";

    TokenIndex token_idx = ID_idx;
    score_entry_t score_entries[100];
    int score_entries_size = 0;

    while (true)
    {
        // Get next line, delimited by newline.
        fgets(text, TEXT_BUF_SIZE, stdin);

#if DEBUG
        printf("Got \"%s\"\n", text);
#endif

        // If there is nothing else to read, we're done.
        if (strcmp(text, "Done.\n") == 0)
            break;
        

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
                score_entries[score_entries_size++] = (score_entry_t){
                    id, topic, score
                };


                // Iterate through each entry in score_entries.
                // If the topic changes, print the accumulated value tuple.
                // If the user changes, start over from the beginning.
                


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
