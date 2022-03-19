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

// #define _DEBUG

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
        void write(mapped_action data) {
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
            sem_wait(&size);
            sem_wait(&lock);

            auto data = *head;

            // Incremenet head pointer
            if(head == &this->data[MAX_SIZE-1])
                head = &this->data[0];
            else
                head++;

            sem_post(&lock);
            
            // Return a copy (since this slot may be overwritten now)
            return data;
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

void fork_it_up(shared_region_t *mapped_region, int index);

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
    const unordered_map<string, score_t> action_points{
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
            
            // Make a new reducer process to handle this data.
            DP("[m] Forking...")
            fork_it_up(shared_mem, index);

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

        DP("[m] Sent index " << index << " data")
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
        
        DP("[m] Sent \"done\" to reducer at index" << i)
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
void reducer(shared_region_t *shared_mem, int index) {
    sem_wait(&shared_mem->userids_lock);
    const auto userid = shared_mem->userids[index];
    sem_post(&shared_mem->userids_lock);

    DP("[r " << userid << "] Reducer spun up")

    // id -> (topic -> total score) for this ID
    unordered_map<topic_t, score_t> total_scores;

    auto& queue = shared_mem->queues[index];

    while(true) {
        auto data = queue.read();
        if(data.done) {
            DP("[r " << userid << "] Received done from mapper.")
            break;
        }
        
        DP("[r " << userid << "] updating \"" << data.topic << "\"...")
        
        // This is okay, since operator[] will construct a default value if it
        // doesn't exist. Nice!
        total_scores[data.topic] += data.score_adjustment;
    }

    DP("r[ " << userid << "] total_scores.size() = " << total_scores.size())

    for (auto &pair : total_scores) {  // Loop through userids
        auto topic = pair.first;
        auto score = pair.second;
        printf("(%s,%s,%d)\n", userid, topic.c_str(), score);
    }

    DP("[r " << userid << "] Done. Goodbye.")
}

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
 * @brief fork() once.
 * @param mapped_region
 */
void fork_it_up(shared_region_t *mapped_region, int index) {
    auto result = fork();

    if (result == -1) {
        printf("Error: fork() didn't work.\n");
        exit(EXIT_FAILURE);
    } else if (result == 0) {
        // Child process
        reducer(mapped_region, index); // Run the reducer.
        exit(EXIT_SUCCESS); // Exit this process
    }
    // Parent process returns.
}

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


    // Now that all the reducers are spun up, run the mapper.
    mapper(tokens, shared_region);

    D(dump_shared_region(shared_region);)

    DP("Waiting for reducer procs to finish...")

    for (int i = 0; i < num_reducers; i++) {
        wait(NULL);
    }

    // Destroy mutexes and semaphores.
    shared_region->destroy();

    // Unmap shared memory
    if (munmap(shared_region, sizeof(shared_region_t)) == -1) {
        printf("Error: Problem unmapping memory.\n");
        exit(EXIT_FAILURE);
    }

    DP("Goodbye.")
}
