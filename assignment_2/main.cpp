#include <iostream>
#include <iomanip>
#include <pthread.h>
#include <string>
#include <queue>
#include <unordered_map>
// C-header files
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Only create a mapper worker, remove all reducer workers.
// For debugging purposes only.
// #define DEBUG_MAPPER

// Run synchronously (i.e., no multithreading). 
// mapper runs first, then reducer.
// #define SYNC

using std::unordered_map;
using id_type = std::string;
using topic_type = std::string;
using score_type = int;


struct mapper_out_data
{
    id_type id;
    topic_type topic;
    score_type score;
};


// For debug-printing the map object.
std::ostream &operator<<(std::ostream &out, const mapper_out_data &o)
{
    out << "mapper_out_data {" << o.id << ", " << o.topic << ", " << o.score << "}";
    return out;
}


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

    unordered_map<std::string, int> action_points = {
        {"P", 50},
        {"L", 20},
        {"D", -10},
        {"C", 30},
        {"S", 40}};

    char *token;
    char *rest = static_cast<char *>(text);

    std::string id;
    int score;

    // WARNING: rest (AKA text) will be modified! If you
    // need text preserved, use strdup to make a copy of it.
    while ((token = strtok_r(rest, delims, &rest)))
    {
        // std::cout << "token: " << token << "\n";
        if (token_i == 0)
        {
            // This is the first of the three tuple entries, which is the ID.
            // id = token;
            id = token;
            token_i++;
        }
        else if (token_i == 1)
        {
            // This is the second tuple entry, which is the category.
            const auto action = std::string(token);
            score = action_points[action];
            token_i++;
        }
        else
        {
            std::string topic(token);
            // std::string topic = token; // copy
            // std::string topic(token);
            // topic.copy();

            mapper_out_data m_data{id, topic, score};
            // Debug print
            pthread_mutex_lock(&cout_lock);
            std::cout << "[mapper] " << m_data << "\n";
            pthread_mutex_unlock(&cout_lock);

            // Push data into inter-thread queue
            pthread_mutex_lock(&queue_lock);
            queue.push(m_data);
            pthread_mutex_unlock(&queue_lock);

            // Reset token index for next round.
            token_i = 0;
        }

        // token = strtok(NULL, delims);
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

    for (int i = 0; i < 5; i++)
    {
        pthread_mutex_lock(&queue_lock);
        const auto data = queue.front();
        queue.pop();
        pthread_mutex_unlock(&queue_lock);

        // auto &tot_score = total_scores[data.id][data.topic];
        // tot_score += data.score;

        pthread_mutex_lock(&cout_lock);
        std::cout << "[reducer " << pthread_self()
                  << "] " << data << "\n";

        // std::cout << "[reducer " << pthread_self()
        //   << "] (" << data.id << ", " << data.topic << ", " << tot_score << ")\n";
        pthread_mutex_unlock(&cout_lock);
    }
    return nullptr;
}


int main(int argc, char *argv[])
{
#ifndef SYNC
    pthread_mutex_init(&queue_lock, NULL);
    pthread_mutex_init(&cout_lock, NULL);
#endif

    if (argc != 3)
    {
        std::cout << "Usage: combiner <no. slots> <no. reducer threads>\n";
        return -1;
    }

    // Get number of buffer slots & number of reducer workers from CLI args
    const auto num_com_buf_slots = std::stoi(argv[1]);
    
    // Read text file
    const auto text = readTextFile();

#ifndef SYNC
    
    // Create mapper thread
    pthread_t mapper_thread;
    pthread_create(&mapper_thread, NULL, mapper_worker, text);

#ifndef DEBUG_MAPPER
    // Create reducer threads
    const int num_users = std::stoi(argv[2]);
    pthread_t reducer_threads[num_users];

    for (auto &r_thread : reducer_threads)
    {
        pthread_create(&r_thread, NULL, reducer_worker, NULL);
    }
#endif

    // Join threads
    pthread_join(mapper_thread, NULL);

#ifndef DEBUG_MAPPER
    for (auto &r_thread : reducer_threads)
    {
        pthread_join(r_thread, NULL);
    }
#endif

#else // SYNC
    std::cout << "SYNC MODE...\n";
    mapper_worker(text);
    reducer_worker(nullptr);
#endif

    // Destroy all resources used.
#ifndef SYNC
    pthread_mutex_destroy(&queue_lock);
    pthread_mutex_destroy(&cout_lock);
#endif
    free(text);

    return 0;
}
