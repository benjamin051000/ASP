#include <pthread.h>

#include <iomanip>
#include <iostream>
#include <queue>
#include <string>
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

// Enable print debugging
// #define DEBUG

using std::unordered_map;
using id_type = std::string;
using topic_type = std::string;
using score_type = int;

struct mapper_out_data {
    id_type id;
    topic_type topic;
    score_type score;
};

// For debug-printing the map object.
std::ostream &operator<<(std::ostream &out, const mapper_out_data &o) {
    out << "(" << o.id << ", " << o.topic << ", " << o.score
        << ")";
    return out;
}

////////////////////////////////
// Synchronization components //
////////////////////////////////
// Mutex for queue pushes and pops
pthread_mutex_t q_lock, cout_lock;
std::queue<mapper_out_data> queue;
// Condition variables for the queue
bool q_full = false, q_empty = true;
pthread_cond_t q_full_cond, q_empty_cond;

// A global flag to tell reducer workers whether the mapper has finished
// producing data. Once this flag turns true, reducers know once the queue is
// empty, it's time to terminate. This does not need a mutex because only one
// thread writes to it, mapper_worker.
bool mapper_done = false;

/**
 * @brief Read a text file into a buffer.
 * WARNING: You must free() the returned char*!
 */
char *readTextFile(void) {
    const int BUF_SIZE = 1500;

    char *buffer = (char *)malloc(BUF_SIZE * sizeof *buffer);

    int index = 0;

    int c;
    while ((c = fgetc(stdin)) != EOF) {
        buffer[index++] = c;
    };

    return buffer;
}

/**
 * @brief Mapper worker
 * @param text (char*) string of input text
 */
void *mapper_worker(void *args) {
    // Unpack args
    struct mapper_args_t {
        size_t buf_size;
        char *text;
    } *mapper_args = (mapper_args_t *)args;

    char *text = mapper_args->text;
    const size_t buf_size = mapper_args->buf_size;

    const auto delims = "(), \n";

    int token_i = 0;

    unordered_map<std::string, int> action_points {
        {"P", 50}, {"L", 20}, {"D", -10}, {"C", 30}, {"S", 40}};

    char *token;
    char *rest = static_cast<char *>(text);

    std::string id;
    int score;

    // WARNING: rest (AKA text) will be modified! If you
    // need text preserved, use strdup to make a copy of it.
    while ((token = strtok_r(rest, delims, &rest))) {
        // std::cout << "token: " << token << "\n";
        if (token_i == 0) {
            // This is the first of the three tuple entries, which is the ID.
            // id = token;
            id = token;
            token_i++;
        } else if (token_i == 1) {
            // This is the second tuple entry, which is the category.
            const auto action = std::string(token);
            score = action_points[action];
            token_i++;
        } else {
            mapper_out_data m_data{id, token, score};
#ifdef DEBUG
            // Debug print
            pthread_mutex_lock(&cout_lock);
            std::cout << "[mapper] " << m_data << "\n";
            pthread_mutex_unlock(&cout_lock);
#endif

            // Push data into inter-thread queue
            pthread_mutex_lock(&q_lock);

            // Wait for queue to have room to push a new one (AKA wait for not
            // full)
            while (q_full) {
#ifdef DEBUG
                pthread_mutex_lock(&cout_lock);
                std::cout << "\033[33;1m[mapper] queue is full. Waiting for a "
                             "reducer "
                             "worker to alert us that it is no longer "
                             "full...\033[0m\n";
                pthread_mutex_unlock(&cout_lock);
#endif
                pthread_cond_wait(&q_full_cond, &q_lock);
            }

#ifdef DEBUG
            pthread_mutex_lock(&cout_lock);
            std::cout << "\033[32;1m[mapper] queue is no longer full!\033[0m\n";
            pthread_mutex_unlock(&cout_lock);
#endif

            // At this point, queue is not full.
            queue.push(m_data);

            // Now that we've pushed a new item, tell other
            // threads that the queue is no longer empty.
            q_empty = false;
            pthread_cond_signal(&q_empty_cond);

            // However, it may be full.
            if (queue.size() == buf_size) {
                q_full = true;
            }

            pthread_mutex_unlock(&q_lock);

            // Reset token index for next round.
            token_i = 0;
        }

        // token = strtok(NULL, delims);
    }  // end of while

    // No more tokens to parse. Alert reducer threads that the mapper has
    // finished.
    mapper_done = true;
    return nullptr;
}  // end of mapper

/**
 * @brief reducer worked
 * @return void* (unused, void* is here for the pthread_create interface.)
 */
void *reducer_worker(void *) {
    static unordered_map<id_type, unordered_map<topic_type, score_type>>
        total_scores;

    while (true) {
        pthread_mutex_lock(&q_lock);

        // If mapper_done flag is asserted, and the queue is empty,
        // There is no more work to be done. Terminate this thread.
        if (mapper_done and q_empty) {
            pthread_mutex_unlock(&q_lock);  // probably unnecessary
            break;
        }

        // Wait for queue to have elements (AKA wait for not empty)
        while (q_empty) {
#ifdef DEBUG
            pthread_mutex_lock(&cout_lock);
            std::cout
                << "\033[33;1m[reducer] queue is empty. Waiting for a mapper "
                   "worker to alert us that it is no longer empty...\033[0m\n";
            pthread_mutex_unlock(&cout_lock);
#endif
            pthread_cond_wait(&q_empty_cond, &q_lock);
        }

#ifdef DEBUG
        pthread_mutex_lock(&cout_lock);
        std::cout << "\033[32;1m[reducer] queue is no longer empty!\033[0m\n";
        pthread_mutex_unlock(&cout_lock);
#endif

        const auto data = queue.front();
        queue.pop();

        // It's no longer full. Alert other threads.
        q_full = false;
        pthread_cond_signal(&q_full_cond);

        // However, it may be empty.
        if (queue.empty()) {
            q_empty = true;
        }

        pthread_mutex_unlock(&q_lock);

        // auto &tot_score = total_scores[data.id][data.topic];
        // tot_score += data.score;

        pthread_mutex_lock(&cout_lock);
        std::cout
#ifdef DEBUG
            << "[reducer " << pthread_self() << "] "
#endif
            << data << "\n";

        // std::cout << "[reducer " << pthread_self()
        //   << "] (" << data.id << ", " << data.topic << ", " << tot_score <<
        //   ")\n";
        pthread_mutex_unlock(&cout_lock);
    }
    return nullptr;
}

int main(int argc, char *argv[]) {
#ifndef SYNC
    pthread_mutex_init(&q_lock, NULL);
    pthread_mutex_init(&cout_lock, NULL);
#endif

    if (argc != 3) {
        std::cout << "Usage: combiner <no. slots> <no. reducer threads>\n";
        return -1;
    }

    // Get number of buffer slots & number of reducer workers from CLI args
    const size_t buf_size = std::stoi(argv[1]);

    // Read text file
    const auto text = readTextFile();

    struct {
        size_t buf_size;
        char *text;
    } mapper_args = {buf_size, text};

#ifndef SYNC

    // Create mapper thread
    pthread_t mapper_thread;
    pthread_create(&mapper_thread, NULL, mapper_worker, &mapper_args);

#ifndef DEBUG_MAPPER
    // Create reducer threads
    const int num_users = std::stoi(argv[2]);
    pthread_t reducer_threads[num_users];

    for (auto &r_thread : reducer_threads) {
        pthread_create(&r_thread, NULL, reducer_worker, NULL);
    }
#endif

    // Join threads
    pthread_join(mapper_thread, NULL);

#ifndef DEBUG_MAPPER
    for (auto &r_thread : reducer_threads) {
        pthread_join(r_thread, NULL);
    }
#endif

#else  // SYNC
    std::cout << "SYNC MODE...\n";
    mapper_worker(text);
    reducer_worker(nullptr);
#endif

    // Destroy all resources used.
#ifndef SYNC
    pthread_mutex_destroy(&q_lock);
    pthread_mutex_destroy(&cout_lock);
#endif
    free(text);

    return 0;
}
