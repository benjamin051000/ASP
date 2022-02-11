#include <iostream>
#include <iomanip>
#include <stdio.h>  // fgetc()
#include <stdlib.h> // malloc()
#include <string.h>
#include <pthread.h>
#include <string>
#include <queue>
#include <unordered_map>

using std::unordered_map;
using id_type = int;
using topic_type = std::string;
using score_type = int;

struct mapper_out_data
{
    id_type id;
    topic_type topic;
    score_type score;
};

// Mutex for queue pushes and pops
pthread_mutex_t queue_lock, cout_lock;
std::queue<mapper_out_data> queue;

/**
 * @brief Read a text file into a buffer.
 * WARNING: You must free() the returned char*!
 */
char *readTextFile(void)
{
    const int BUF_SIZE = 1500;

    char *buffer = (char *)malloc(BUF_SIZE * sizeof *buffer);

    int index = 0;

    int c;
    while ((c = fgetc(stdin)) != EOF)
    {
        buffer[index++] = c;
    };

    return buffer;
}

/**
 * @brief Mapper worker
 * @param text (char*) string of input text
 */
void *mapper_worker(void *text)
{
    const auto delims = "(), \n";

    int token_i = 0;

    char *token = strtok((char *)text, delims);

    unordered_map<std::string, int> action_points = {
        {"P", 50},
        {"L", 20},
        {"D", -10},
        {"C", 30},
        {"S", 40}};

    while (token != NULL)
    {
        int id, score;

        if (token_i == 0)
        {
            // This is the first of the three tuple entries, which is the ID.
            id = atoi(token);
            token_i++;
        }
        else if (token_i == 1)
        {
            // This is the second tuple entry, which is the category.
            auto action = std::string(token);
            score = action_points[action];
            token_i++;
        }
        else
        {
            auto topic = token;

            // TODO: move into buffer for reducer threads
            pthread_mutex_lock(&cout_lock);
            std::cout << "[mapper] (" << std::setw(4) << std::setfill('0') << id << ", " << topic << ", " << score << ")\n";
            pthread_mutex_unlock(&cout_lock);

            pthread_mutex_lock(&queue_lock);
            queue.push(mapper_out_data{id, topic, score});
            pthread_mutex_unlock(&queue_lock);

            token_i = 0;
        }

        token = strtok(NULL, delims);
    } // end of while

    return nullptr;
} // end of mapper

/**
 * @brief reducer worked
 * @return void* (unused, void* is here for the pthread_create interface.)
 */
void *reducer_worker(void *)
{
    static unordered_map<id_type, unordered_map<topic_type, score_type>> total_scores;

    pthread_mutex_lock(&queue_lock);
    auto data = queue.front();
    queue.pop();
    pthread_mutex_unlock(&queue_lock);

    auto& tot_score = total_scores[data.id][data.topic];
    tot_score += data.score;

    pthread_mutex_lock(&cout_lock);
    std::cout << "[reducer " << pthread_self() 
    << "] (" << data.id << ", " << data.topic << ", " << tot_score << ")\n";
    pthread_mutex_unlock(&cout_lock);

    return nullptr;
}

int main(int argc, char *argv[])
{

    pthread_mutex_init(&queue_lock, NULL);

    if (argc != 3)
    {
        std::cout << "Usage: combiner <no. slots> <no. reducer threads>\n";
        return -1;
    }

    const auto num_com_buf_slots = std::stoi(argv[1]);
    const auto num_users = std::stoi(argv[2]);

    // Read text file
    const auto text = readTextFile();

    pthread_t mapper_thread;
    pthread_t reducer_threads[num_users];

    // Create threads
    pthread_create(&mapper_thread, NULL, mapper_worker, text);
    for (auto &r_thread : reducer_threads)
    {
        pthread_create(&r_thread, NULL, reducer_worker, NULL);
    }

    // Join threads
    pthread_join(mapper_thread, NULL);
    for (auto &r_thread : reducer_threads)
    {
        pthread_join(r_thread, NULL);
    }

    pthread_mutex_destroy(&queue_lock);
    free(text);

    return 0;
}
