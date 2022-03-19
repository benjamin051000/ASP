#include <semaphore.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wait.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>
using std::array;
using std::string;
using std::unordered_map;

using userid_t = string;
using topic_t = string;
using score_t = int;

#define _DEBUG

#ifdef _DEBUG
// Used for debug operations
#define D(x) x
#define DP(x) std::cout << x << "\n";
#else
// Don't expand to anything
#define D(x)
#define DP(x)
#endif
/////////////////////////////////////////

/**
 * @brief Tuple to hold score adjustments per topic for an ID.
 * Mapper will send this info to the reducer designated for this action's ID.
 */
struct mapped_action {
    char topic[50];
    score_t score_adjustment;
    bool done = false;
};

/**
 * @brief Data structure the mmapped region will be cast to.
 * Use this to conveniently access members in the shared region.
 */
class shared_region_t {
    static const auto SEM_PSHARED = 1;
public:
    /**
     * @brief IPC Queue object.
     */
    class queue {
        int MAX_SIZE;
    public:
        static const size_t NUM_ENTRIES = 10;
        array<mapped_action, NUM_ENTRIES> data;  // From mapper
        sem_t lock, size;
        // Used to keep track of valid data in the queue.
        mapped_action *head, *tail; 

        /**
         * @brief Initialize queue for IPC.
         */
        void init(int max_q_size) {
            sem_init(&lock, SEM_PSHARED, 1);  // Mutex
            sem_init(&size, SEM_PSHARED, 0);  // Size of queue
            
            head = tail = &data[0];
            MAX_SIZE = max_q_size;
        }

        /**
         * @brief Clean up queue.
         */
        void destroy() {
            sem_destroy(&lock);
            sem_destroy(&size);
        }

        /**
         * @brief Write to the back of the queue.
         * @param data mapped_action obj to write.
         */
        void write(mapped_action data) { // TODO include buf_size here?
            sem_wait(&lock);
            
            int curr_size;
            sem_getvalue(&size, &curr_size);

            if(curr_size == MAX_SIZE) {
                printf("ERROR: Buffer is full!\n");
                exit(EXIT_FAILURE);
            }
            else {
                *tail = data; // TODO verify this works with char*s (it won't)
                
                // Increment tail pointer
                if(tail == &this->data[MAX_SIZE-1]) {
                    tail = &this->data[0];
                }
                else tail++;
            }
            
            sem_post(&size);
            sem_post(&lock);
        }

        /**
         * @brief Read from the head of the queue.
         * 
         * @return mapped_action 
         */
        mapped_action read() {

        }
    };  // end of queue

    static const size_t MAX_REDUCERS = 7;
    array<queue, MAX_REDUCERS> queues;

    /**
     * @brief A convenient way for reducers to obtain their userid.
     * Reducers have an index associated with them. userids[index] is
     * their userid in string form.
     * WARNING: Be sure to strcpy() into this array,
     * or it won't be saved in shared memory.
     */
    array<char[50], MAX_REDUCERS> userids;
    size_t userids_size = 0;
    sem_t userids_lock; // Mutex for userids array. TODO necessary?
    /**
     * @brief Initialize all IPC queues.
     */
    void init(int max_q_size) {
        for (auto &q : queues) q.init(max_q_size);

        sem_init(&userids_lock, SEM_PSHARED, 1);
    }

    /**
     * @brief Destory all queues.
     */
    void destroy() {
        for (auto &q : queues) q.destroy();

        sem_destroy(&userids_lock);
    }

};  // end of mmapped_region_t

/**
 * @brief Read a text file into a string buffer.
 */
string read_stdin() {
    string out;
    int c;
    while ((c = fgetc(stdin)) != EOF) {
        out += c;
    }

    return out;
}

/**
 * @brief Split by delimiter, return tokens.
 * @param str String to be split.
 * @param regex_str Regular expression of delimiters to split str by.
 * @return std::vector<string> Iterator of tokens.
 */
std::vector<string> split(const std::string str, const std::string regex_str) {
    const std::regex re(regex_str);
    return {std::sregex_token_iterator(str.begin(), str.end(), re, -1),
            std::sregex_token_iterator()};
}

/**
 * @brief Struct to represent the original id/topic/action info from stdin.
 */
struct input_data_t {
    const userid_t id;
    const string action;
    const topic_t topic;  // TODO topics still have trailing whitespace,
                          // consider stripping it
};

/**
 * @brief Parse tokens into actions.
 * @param tokens
 */
std::vector<input_data_t> parse_actions(const std::vector<string> &tokens) {
    std::vector<input_data_t> actions;

    // Iterate through the tokens and produce action tuples.
    auto it = tokens.begin();
    while (it != tokens.end()) {
        actions.push_back(
            // TODO Does this copy into input_data_t? I think it does
            input_data_t{*it++, *it++, *it++});
    }

    return actions;
}

/**
 * @brief Mapper process
 * @param tokens vector of strings from stdin.
 * @param shared_mem Pointer to shared region
 */
void mapper(const std::vector<string> &tokens, shared_region_t *shared_mem) {
    DP("[m] Starting mapper...")

    // Used to convert userid_t to an index, in the mmapped array.
    // static std::vector<userid_t> id_index;

    const auto actions = parse_actions(tokens);

    // Map that coorelates an action to its cooresponding point value.
    const std::unordered_map<string, score_t> action_points{
        {"P", 50}, {"L", 20}, {"D", -10}, {"C", 30}, {"S", 40}};

    // Next, send the mapped data to the reducer, action by action.
    for (auto &action : actions) {
        sem_wait(&shared_mem->userids_lock);
        // Get the index corresponding to this ID.
        const auto iter =
            // std::find(id_index.begin(), id_index.end(), action.id);
            std::find(shared_mem->userids.begin(), shared_mem->userids.end(), action.id); // TODO ensure this works since action.id is a string, not char*

        // Index used to push the data to the reducer.
        int index;
        if (iter == shared_mem->userids.end()) {
            // It wasn't in the array. Add it
            strcpy(
                shared_mem->userids[shared_mem->userids_size], 
                action.id.c_str()
            );
            
            index = shared_mem->userids_size++;
            // TODO fork() here for dynamic num_reducers.
        } else {
            // Get the index this element is at in the vector.
            index = std::distance(shared_mem->userids.begin(), iter);
        }

        sem_post(&shared_mem->userids_lock);

        // Get reference to this index's queue
        auto& queue = shared_mem->queues[index];

        // Create score object to be shared with reducer process
        mapped_action score;
        strcpy(score.topic, action.topic.c_str());
        // convert action to points
        score.score_adjustment = action_points.at(action.action);

        // Write to queue.
        queue.write(score);

        DP("[m] Sent index " << index << " data, new size = ")  // size was just posted

        DP("[m] released lock.")
    }  // end of for(auto &action: actions)

    // All data has been sent to all reducers.
    // Now, append a "done" signal to each reducer queue.
    for(unsigned i = 0; i < shared_mem->userids_size; i++) {
        auto &queue = shared_mem->queues[i];
        queue.write(mapped_action {
            "",
            0,
            true
        });
        
        DP("[m] Sent \"done\" to reducer " << i)
    }

    DP("[m] Done. Goodbye.")
} // end of mapper

/**
 * @brief Print shared mem out.
 * For debugging purposes.
 */
void dump_shared_region(shared_region_t *shared_mem) {
    printf("--dump_shared_region()----------\n");
    for(unsigned i = 0; i < shared_mem->queues.size(); i++) {
        auto& q = shared_mem->queues[i];
        const auto& id = shared_mem->userids[i];

        int q_size;
        sem_getvalue(&q.size, &q_size);
        
        printf("ID \"%s\": {", id);
        
        for(auto& e: q.data) {
            if(!e.done)
                printf("{\"%s\", %d}, ", e.topic, e.score_adjustment);
            else 
                printf("{DONE},");
        }

        printf("}\n");
    }
    printf("--dump_shared_region() done.----------\n");
}

//////////////////////////////////////////////////////

/**
 * @brief Reducer worker.
 * Run after fork()ing.
 * @param mapped_data Pointer to shared memory
 * @param id Which index of the data array (AKA ID)
 * this reducer should work on.
 */
// void reducer(mapped_data_structure *mapped_data, int id) {
//     DP("[r " << id << "] Reducer spun up with id " << id)

//     // id -> (topic -> total score) for this ID
//     std::unordered_map<userid_t, std::unordered_map<topic_t, score_t>>
//         total_scores;

//     int head = 0;
//     while (true) {
//         DP("[r " << id << "] waiting for size sem...")

//         // Wait for data to appear in the queue.
//         sem_wait(&mapped_data->sizes[id]);

//         // Get the last element in the queue
//         // int size;
//         // sem_getvalue(&mapped_data->sizes[id], &size);

//         DP("[r " << id << "] Got sem. Waiting for lock...")
//         // for(unsigned i = 0; i < size; i++) {
//         // Get a new value from shared mem.
//         pthread_mutex_lock(&mapped_data->locks[id]);  // Acquire lock
//         DP("[r " << id << "] Got lock.")

//         // Now, read the data.
//         auto val = mapped_data->data[id][head++];

//         if (val.done) {
//             DP("[r " << id << "] Received done signal from mapper.")
//             break;
//         } else if (string(val.topic) == "") {
//             DP("[r " << id << "] !!!Found a null string, skipping it.")
//             pthread_mutex_unlock(&mapped_data->locks[id]);
//             continue;
//         }

//         DP("[r " << id << "] Updating topic \"" << val.topic << "\"...")

//         // This is okay, since operator[] will construct a default value if it
//         // doesn't exist. Nice!
//         total_scores[val.id][val.topic] += val.score_adjustment;

//         pthread_mutex_unlock(&mapped_data->locks[id]);  // Release lock

//     }  // end of while(true)

//     DP("r[ " << id << "] total_scores.size() = " << total_scores.size())

//     for (auto &id : total_scores) {  // Loop through userids
//         auto userid = id.first;
//         for (auto &t : id.second) {  // Loop through topics
//             printf("(%s,%s,%d)\n", userid.c_str(), t.first.c_str(), t.second);
//         }
//     }

//     DP("[r " << id << "] Done. Goodbye.")
// }

/**
 * @brief Initialize shared region.
 * @return shared_region_t*
 */
shared_region_t *shared_region_init(int max_q_size) {
    auto *shared_region = (shared_region_t *)mmap(
        NULL, sizeof(shared_region_t), PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    if (shared_region == MAP_FAILED) {
        printf("Error: Couldn't map memory region.\n");
        exit(EXIT_FAILURE);
    }

    // Initialize shared region.
    shared_region->init(max_q_size);

    return shared_region;
}

/**
 * @brief fork() num_reducers times.
 * @param mapped_region
 * @param num_reducers
 */
// void fork_it_up(mapped_data_structure *mapped_region, int num_reducers) {
//     for (int i = 0; i < num_reducers; i++) {
//         auto result = fork();

//         if (result == -1) {
//             printf("Error: fork() didn't work.\n");
//             exit(EXIT_FAILURE);
//         } else if (result == 0) {
//             // Child process
//             reducer(mapped_region, i);  // Run the reducer.
//             exit(EXIT_SUCCESS);         // Exit this process
//         }
//     }

//     // Parent process returns.
// }

//////////////////////////////////////////////////////
int main(int argc, char *argv[]) {
    // Get CLI args
    if (argc != 3) {
        printf("Usage: <buffer size> <no. reducers>\n");
        exit(EXIT_FAILURE);
    }

    const auto max_q_size = std::stoi(argv[1]);
    const auto num_reducers = std::stoi(argv[2]);

    const auto text = read_stdin();

    // delims are anything inside the []s
    const auto temp_tokens = split(text, "[(),\n]");

    std::vector<string> tokens;
    // Filter whitespace tokens out
    for (const auto &e : temp_tokens) {
        if (e.find_first_not_of(' ') != string::npos) tokens.push_back(e);
    }

    // Set up shared memory
    auto shared_region = shared_region_init(max_q_size);

    D(dump_shared_region(shared_region);)

    // fork_it_up(mapped_region, num_reducers);

    // // Now that all the reducers are spun up, run the mapper.
    mapper(tokens, shared_region);

    D(dump_shared_region(shared_region);)

    // DP("Waiting for reducer procs to finish...")

    // for (int i = 0; i < num_reducers; i++) {
    //     wait(NULL);
    // }

    // // Destroy mutexes and semaphores.
    // mapped_region->destroy();

    // // Unmap shared memory
    // if (munmap(mapped_region, sizeof(mapped_data_structure)) == -1) {
    //     printf("Error: Problem unmapping memory.\n");
    //     exit(EXIT_FAILURE);
    // }

    DP("Goodbye.")
}
