#include <pthread.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

// Enable print debugging
#define DEBUG

// Helpful debug stuff
#ifdef DEBUG
// Since threads will print various info, they need a mutex for cout.
#define COUT_LOCK pthread_mutex_lock(&cout_lock);
#define COUT_UNLOCK pthread_mutex_unlock(&cout_lock);

#define COUT_SYNC(stuff) \
    COUT_LOCK;           \
    std::cout << stuff;  \
    COUT_UNLOCK;

// Lock to synchronize thread cout
pthread_mutex_t cout_lock;
#endif

// Type aliases
using std::string;
using std::unordered_map;

using id_type = string;
using topic_type = string;
using score_type = int;

// Struct to hold arguments passed from main to mapper worker thread.
struct mapper_args_t {
    size_t buf_size;
    char *text;
};

// Struct to hold output from the mapper.
struct mapped_data {
    id_type id;
    topic_type topic;
    score_type score;
};

// For debug-printing the map object.
std::ostream &operator<<(std::ostream &out, const mapped_data &o) {
    out << "(" << o.id << ", " << o.topic << ", " << o.score << ")";
    return out;
}

/**
 * Connection from the mapper worker to
 * a single reducer worker. Includes a
 * queue, mutexes, cond vars, and an id.
 *
 * mapper worker will use ID from struct
 * to choose which queue to send the data through
 *
 * Could just have a vector/map of these structs to hold
 * them or vector::find?
 */
struct ReducerConnection {
    id_type id;
    std::queue<mapped_data> q;
    pthread_mutex_t q_lock;
    pthread_cond_t q_full_cond, q_empty_cond;
    bool q_full = false;
    bool q_empty = true;

    // Default constructor (should not be called)
    ReducerConnection() {}

    // Use this constructor instead
    ReducerConnection(const id_type id) : id(id) {
        pthread_mutex_init(&q_lock, NULL);
        pthread_cond_init(&q_empty_cond, NULL);
        pthread_cond_init(&q_full_cond, NULL);
    }

    ~ReducerConnection() {
        pthread_mutex_destroy(&q_lock);
        pthread_cond_destroy(&q_empty_cond);
        pthread_cond_destroy(&q_full_cond);
    }
};

/**
 * Connections between producer and consumer threads.
 * Includes queues for data passing, mutexes, and condition variables.
 */
unordered_map<id_type, ReducerConnection> thread_conns;
pthread_mutex_t thread_conns_lock;

// Vector of pthread IDs (for joining)
std::vector<pthread_t> reducer_workers;

// Reducer workers do their work in this map.
unordered_map<id_type, unordered_map<topic_type, score_type>> total_scores;
pthread_mutex_t total_scores_lock;

// A global flag to tell reducer workers whether the mapper has finished
// producing data. Once this flag turns true, reducers know once the queue is
// empty, it's time to terminate. This does not need a mutex because only one
// thread writes to it, mapper_worker.
bool mapper_done = false;

void *reducer_worker(void *args);

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
 * @param args A struct containing a buffer size and a char* to the input text.
 */
void *mapper_worker(void *args) {
    // Unpack args
    mapper_args_t &mapper_args = *static_cast<mapper_args_t *>(args);

    auto text = mapper_args.text;
    const auto BUF_SIZE = mapper_args.buf_size;

    const auto delims = "(), \n";

    // Map that coorelates an action to its cooresponding point value.
    const unordered_map<string, int> action_points{
        {"P", 50}, {"L", 20}, {"D", -10}, {"C", 30}, {"S", 40}};

    // Make temp variables the tokenizer will use
    char *token;  // Substring of input text that represents a single token.

    // Used by strtok_r to preserve tokenizer state.
    char *rest = static_cast<char *>(text);

    // Get each token from the string.
    while ((token = strtok_r(rest, delims, &rest))) {
        const auto id = token;  // First token, the user ID.

        // Move on to next token
        if (!(token = strtok_r(rest, delims, &rest))) {
            std::cout << "ERROR: Token was NULL, expected action.\n";
            exit(EXIT_FAILURE);
        }

        const auto score = action_points.at(token);  // Cooresponding score

        // Move on to next token
        if (!(token = strtok_r(rest, delims, &rest))) {
            std::cout << "ERROR: Token was NULL, expected topic.\n";
            exit(EXIT_FAILURE);
        }

        const auto topic = token;

        // All the tokens are collected! Construct the mapped tuple object.
        mapped_data m_data{id, topic, score};

#ifdef DEBUG
        COUT_SYNC("[m] parsed data: " << m_data << "\n")
#endif
        // TODO check if we have added too many reducer threads and print a
        // warning or something?

        // Time to push the data to the appropriate reducer thread.
        // First, get the struct with IPC stuff

        // Test to see if the element exists in the map.
        pthread_mutex_lock(&thread_conns_lock);
        if (thread_conns.find(id) == thread_conns.end()) {
            // It wasn't. Make one and add it to the map.

            thread_conns.emplace(id, ReducerConnection(id));

            auto &r_con = thread_conns.at(id);

#ifdef DEBUG
            COUT_SYNC("[m] creating a new reducer thread for ID \"" << r_con.id
                                                                    << "\"\n")
#endif

            pthread_t new_reducer_thread;

            // Save this pthread in the global vector.
            // This should be ok because pthread_t is a primitive type.
            // TODO verify this
            reducer_workers.push_back(new_reducer_thread);

            // Start the thread
            pthread_create(&new_reducer_thread, NULL, reducer_worker,
                           &r_con.id);
        }  // end of if

        // Now, the reducer connection should be in the map. Get a reference
        // to it.
        auto &r_con = thread_conns.at(id);

        // We should be able to do this here, because r_con points to a single
        // part of thread_conns that has its own mutexes.
        pthread_mutex_unlock(&thread_conns_lock);

        // Send data to the worker
        pthread_mutex_lock(&r_con.q_lock);

        // Wait for queue to have room to push a new one (AKA wait for not
        // full)
        while (r_con.q_full) {
#ifdef DEBUG
            COUT_SYNC(
                "[m] queue full. Waiting for reducer to consume some "
                "elements...\n")
#endif

            pthread_cond_wait(&r_con.q_full_cond, &r_con.q_lock);
        }

        // At this point, queue is not full.

#ifdef DEBUG
        COUT_SYNC("[m] queue is currently not full. Sending data...\n")
#endif

        r_con.q.push(m_data);

        // Now that we've pushed a new item, tell other
        // threads that the queue is no longer empty.
        r_con.q_empty = false;
        pthread_cond_signal(&r_con.q_empty_cond);

        // TODO: above line wakes up a reducer *every* time.
        // Instead, wake the reducer when the buffer is full
        // (in context of having reducers for each user ID)
        // - more efficient, probably less error-prone too

        // However, it may be full.
        if (r_con.q.size() == BUF_SIZE) {
            r_con.q_full = true;
        }

        pthread_mutex_unlock(&r_con.q_lock);

#ifdef DEBUG
        COUT_SYNC("[m] Done sending this tuple. Restarting...\n");
#endif
    }  // end of while

#ifdef DEBUG
    COUT_SYNC(
        "\033[32;1m[m] Finished parsing all the tokens. "
        "Terminating...\033[0m\n");
#endif

    // No more tokens to parse. Alert reducer threads that the mapper has
    // finished.
    mapper_done = true;
    return nullptr;
}  // end of mapper

/**
 * @brief reducer worker
 * @return void* (unused, void* is here for the pthread create interface.)
 */
void *reducer_worker(void *args) {
    // Get mapper connection from args
    string &r_id = *static_cast<string *>(args);

#ifdef DEBUG
    COUT_SYNC("[r " << pthread_self() << "] r_id = " << r_id << "\n")
#endif

    // Get mapper connection data structures
    pthread_mutex_lock(&thread_conns_lock);

    if (thread_conns.find(r_id) == thread_conns.end()) {
        std::cerr << "Error: reducer can't find thread connection obj for id "
                  << r_id << "\n";
        exit(EXIT_FAILURE);
    }

    // We were able to find it, so grab it.
    auto &m_conn = thread_conns.at(r_id);

    pthread_mutex_unlock(&thread_conns_lock);

#ifdef DEBUG
    COUT_SYNC("[r " << pthread_self() << "] Starting for user id:" << m_conn.id
                    << "\n")
#endif

    while (true) {
        pthread_mutex_lock(&m_conn.q_lock);

        // If mapper_done flag is asserted, and the queue is empty,
        // There is no more work to be done. Terminate this thread.
        if (mapper_done and m_conn.q_empty) {
#ifdef DEBUG
            COUT_SYNC("\033[32;1m[r "
                      << pthread_self()
                      << "] Mapper is done, and I have nothing left in "
                         "my queue. I'm done!\033[0m\n")
#endif
            pthread_mutex_unlock(&m_conn.q_lock);  // probably unnecessary
            break;
        }

        // Wait for queue to have elements (AKA wait for not empty)
        while (m_conn.q_empty) {
#ifdef DEBUG
            COUT_SYNC(
                "\033[33;1m[r "
                << pthread_self()
                << "] queue is empty. Waiting for a mapper "
                   "worker to alert us that it is no longer empty...\033[0m\n")
#endif
            pthread_cond_wait(&m_conn.q_empty_cond, &m_conn.q_lock);
        }

#ifdef DEBUG
        COUT_SYNC("[r " << pthread_self()
                        << "] queue is currently not empty. Fetching data...\n")
#endif

        const auto data = m_conn.q.front();
        m_conn.q.pop();

        // It's no longer full. Alert mapper.
        m_conn.q_full = false;
        pthread_cond_signal(&m_conn.q_full_cond);

        // However, it may be empty.
        if (m_conn.q.empty()) {
            m_conn.q_empty = true;
        }
        pthread_mutex_unlock(&m_conn.q_lock);

#ifdef DEBUG
        COUT_SYNC("[r " << pthread_self() << "] attempting to change id "
                        << data.id << ", topic " << data.topic << "...\n")
#endif
        // Increment score for each one.
        pthread_mutex_lock(&total_scores_lock);

        total_scores[data.id][data.topic] += data.score;
        pthread_mutex_unlock(&total_scores_lock);

#ifdef DEBUG
        COUT_SYNC("[r " << pthread_self()
                        << "] Updated total scores. Restarting...\n")
#endif

    }  // end of while(true)

    return nullptr;
}

int main(int argc, char *argv[]) {
    // Ensure user input 2 CLI args
    if (argc != 3) {
        std::cout << "Usage: combiner <no. slots> <no. reducer threads>\n";
        exit(EXIT_FAILURE);
    }

    // Get number of buffer slots & number of reducer workers from CLI args
    const size_t BUF_SIZE = std::stoi(argv[1]);
    const int NUM_USERS = std::stoi(argv[2]);

    // Ensure valid CLI args
    if (BUF_SIZE == 0) {
        std::cout << "ERROR: BUF_SIZE must be a postitive integer.\n";
        exit(EXIT_FAILURE);
    }

    if (NUM_USERS < 1) {
        std::cout << "ERROR: NUM_USERS must be at least 1.\n";
        exit(EXIT_FAILURE);
    }

#ifdef DEBUG
    // Print CLI args
    std::cout << "Args: buffer size =" << BUF_SIZE
              << ", number of user threads =" << NUM_USERS << "\n";
#endif

    // Iniitalize mutexes
    pthread_mutex_init(&total_scores_lock, NULL);
    pthread_mutex_init(&thread_conns_lock, NULL);

#ifdef DEBUG
    pthread_mutex_init(&cout_lock, NULL);
#endif

    // Read text file
    const auto text = readTextFile();

    // Struct to send text + BUF_SIZE to mapper thread
    mapper_args_t mapper_args = {BUF_SIZE, text};

    // Create mapper thread
    // (reducers will be created dynamically by mapper)
    pthread_t mapper_thread;

    pthread_create(&mapper_thread, NULL, mapper_worker, &mapper_args);

    // Join threads
    pthread_join(mapper_thread, NULL);

    for (auto &r_thread : reducer_workers) {
        pthread_join(r_thread, NULL);
    }

    // At this point, only the main thread remains.

#ifdef DEBUG
    std::cout << "--------------------------------------------------\n";
#endif

    // Print final results
    for (auto &id_map : total_scores) {
        const auto id = id_map.first;

        for (auto &topic_map : id_map.second) {
            const auto topic = topic_map.first;
            const auto tot_score = topic_map.second;
            std::cout << "(" << id << ", " << topic << ", " << tot_score
                      << ")\n";
        }
    }

    // Destroy all resources used.
#ifdef DEBUG
    pthread_mutex_destroy(&cout_lock);
#endif

    // Destroy mutexes
    pthread_mutex_destroy(&total_scores_lock);
    pthread_mutex_destroy(&thread_conns_lock);

    // Free input text
    free(text);

#ifdef DEBUG
    std::cout << "Goodbye.\n";
#endif

    return 0;
}
