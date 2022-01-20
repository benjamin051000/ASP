#include <stdio.h>  // for printf(), fgetc()
#include <stdlib.h> // for malloc(), atoi()
#include <string.h> // for strtok()
#include <stdbool.h>

#define TOTAL_IDS 100
#define TOTAL_TOPICS 100

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
    while ((c = fgetc(stdin)) != EOF)
    {
        buffer[index++] = c;
    };

    return buffer;
}

void update_total_scores(char *id, char *topic, int score)
{
    static char *ids[TOTAL_IDS];
    static char *topics[TOTAL_IDS][TOTAL_TOPICS]; // List of topics for each ID
    static int total_score[TOTAL_IDS][TOTAL_TOPICS];
    // Indexes to traverse through the matrix of scores.
    static int id_size = 0;
    static int topic_sizes[TOTAL_TOPICS] = {0}; // Initialize to 0

    bool found_id = false;
    // First, try to find the ID.
    for (int id_idx = 0; id_idx < id_size; id_idx++)
    {
        if (strcmp(ids[id_idx], id) == 0)
        {
            // Found the relevant ID. Use this i in the matrix to get the topics.
            found_id = true;

            bool found_topic = false;
            // Next, find the topic.
            for (int topic_idx = 0; topic_idx < topic_sizes[id_idx]; topic_idx++)
            {
                if (strcmp(topics[id_idx][topic_idx], topic) == 0)
                {
                    found_topic = true;
                    // We know the index of the id and topic.
                    // Now we can modify the score.
                    total_score[id_idx][topic_idx] += score;
                }
            } // topic iterator

            if (!found_topic)
            {
                // Create the new topic.
                topics[id_idx][topic_sizes[id_idx]++] = topic;
            }
        }
    } // ID iterator

    if (!found_id)
    {
        // Print out the last id's topics.
        // But, only if there is already an ID in the array.
        if (id_size > 0)
        {
            // Iterate through the last ID's topics and print them out
            for (int topic_idx; topic_idx < topic_sizes[id_size]; topic_idx++)
            {
                int score = total_score[id_size][topic_idx];

                printf("(%s, %s, %d)\n", ids[id_size], topics[id_size][topic_idx], score);
            }
        }

        // Create it at the next available index.
        ids[id_size++] = id;
    }

} // update_total_scores

int main(void)
{
    // Read stdin
    char *text = read();

    // Parse the string, token-by-token.
    char delims[] = "(), \n";
    char *token = strtok(text, delims);
    int token_idx = 0;

    while (token != NULL)
    {
        // Temp variables to hold the values
        char *id, *topic;
        int score;

        switch (token_idx)
        {
        case 0:
            id = token;
            token_idx++;
            break;
        case 1:
            topic = token;
            token_idx++;
            break;
        case 2:
            score = atoi(token);

            update_total_scores(id, topic, score);

            token_idx = 0;
            break;
        } // switch

        token = strtok(NULL, delims);
    } // while (token != NULL)

    free(text);
    return 0;
} // main
