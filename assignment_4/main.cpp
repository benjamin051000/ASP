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
using std::string;
using std::array;

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
class mmapped_region_t {
public:
    /**
     * @brief IPC Queue object.
     */
    class queue {
        static const auto SEM_PSHARED = 1;
    public:
        
        static const size_t NUM_ENTRIES = 10;
        array<mapped_action, NUM_ENTRIES> data; // From mapper
        sem_t lock, size;
        mapped_action *head, tail; // Used to keep track of where mapper/reducer are in the queue.

        /**
         * @brief Initialize queue for IPC.
         */
        void init() {
            sem_init(&lock, SEM_PSHARED, 1); // Mutex
            sem_init(&size, SEM_PSHARED, 0); // Size of queue
        }

        /**
         * @brief Clean up queue.
         */
        void destroy() {
            sem_destroy(&lock);
            sem_destroy(&size);
        }
    }; // end of queue

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

    /**
     * @brief Initialize all IPC queues.
     */
    void init() {
        for(auto &q: queues) q.init();
    }

    /**
     * @brief Destory all queues.
     */
    void destroy() {
        for(auto &q: queues) q.destroy();
    }

}; // end of mmapped_region_t

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
  const topic_t topic;  // TODO topics still have trailing whitespace, consider
                        // stripping it
};

/**
 * @brief Parse tokens into actions.
 *
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
 * @brief Mapper worker process
 *
 * @param tokens vector of strings from stdin.
 * @param shared_mem
 */
void mapper(const std::vector<string> &tokens,
            mapped_data_structure *shared_mem) {
  DP("[m] Starting mapper...")

  // Used to convert user ID to an index, in the mmapped array.
  static std::vector<userid_t> id_index;

  const auto actions = parse_actions(tokens);

  // Map that coorelates an action to its cooresponding point value.
  const std::unordered_map<string, score_t> action_points{
      {"P", 50}, {"L", 20}, {"D", -10}, {"C", 30}, {"S", 40}};

  // Next, send the mapped data to the reducer, action by action.
  for (auto &action : actions) {
    // Get the index corresponding to this ID.
    const auto iter = std::find(id_index.begin(), id_index.end(), action.id);

    // Index used to push the data to the reducer.
    int index;

    if (iter == id_index.end()) {
      // It wasn't in the vector. Add it
      id_index.push_back(action.id);
      index = id_index.size() - 1;
    } else {
      // Get the index this element is at in the vector.
      index = std::distance(id_index.begin(), iter);
    }

    // Acquire lock
    pthread_mutex_lock(&shared_mem->locks[index]);
    DP("[m] Acquired lock.")

    // Create score object to be shared with reducer process
    mapped_data_t score;
    strcpy(score.topic, action.topic.c_str());
    
    score.score_adjustment =
        action_points.at(action.action);  // convert action to points

    
    // Get current size
    int size;
    sem_getvalue(&shared_mem->sizes[index], &size);

    shared_mem->data[index][size] = score;

    // Increase size of this IPC so reducer knows it can take something off
    sem_post(&shared_mem->sizes[index]);

    DP("[m] Sent index " << index << " data, new size = " << size+1) // size was just posted
    
    // Release lock
    pthread_mutex_unlock(&shared_mem->locks[index]);
    DP("[m] released lock.")
  }

    // All data has been sent to all reducers.
    // Now, append a "done" signal to each reducer queue.
    for(unsigned i = 0; i < id_index.size(); i++) {
        pthread_mutex_lock(&shared_mem->locks[i]);
        
        int size;
        sem_getvalue(&shared_mem->sizes[i], &size);
        // Place new thing here

        DP("[m] sem size is: " << size)
        
        DP("[m] Attempting to send \"done\" to reducer " << i)
        shared_mem->data[i][size] = mapped_data_t { 
            "",
            0,
            true // true signifies that there is no more data to be dealt with.
        };

        sem_post(&shared_mem->sizes[i]); // Wake up waiting procs (which will read the done signal)

        pthread_mutex_unlock(&shared_mem->locks[i]);
        DP("[m] Sent \"done\" to reducer " << i)
    }

    DP("[m] Done. Goodbye.")
}

/**
 * @brief Print shared mem out.
 * For debugging purposes.
 */
void dump_shared_mem(mapped_data_structure *shared_mem) {
  printf("----------dump_shared_mem()----------\n");

  for (unsigned i = 0; i < shared_mem->data.size(); i++) {
    const auto row = shared_mem->data[i];
    int size;
    sem_getvalue(&shared_mem->sizes[i], &size);

    printf("Row %d (size: %d): {", i, size);

    for (const auto &val : row) {
      // printf("{%s, %d}, ", val.topic, val.score_adjustment);
      std::cout << "{" << val.topic << ", " << val.score_adjustment << "}, ";
    }
    printf("}\n\n");
  }
  printf("----------dump_shared_mem() done----------\n");
}

//////////////////////////////////////////////////////

/**
 * @brief Reducer worker.
 * Run after fork()ing.
 * @param mapped_data Pointer to shared memory
 * @param id Which index of the data array (AKA ID)
 * this reducer should work on.
 */
void reducer(mapped_data_structure *mapped_data, int id) {
  DP("[r " << id << "] Reducer spun up with id " << id)

  std::unordered_map<topic_t, score_t>
      total_scores;  // topic -> total score for this ID


  while (true) {
    DP("[r " << id << "] waiting for size sem...")

    // Wait for data to appear in the queue.
    sem_wait(&mapped_data->sizes[id]);
    
    // Get the last element in the queue
    int size;
    sem_getvalue(&mapped_data->sizes[id], &size);

    DP("[r " << id << "] Got sem. Waiting for lock...")
    // for(unsigned i = 0; i < size; i++) {
    // Get a new value from shared mem.
    pthread_mutex_lock(&mapped_data->locks[id]);  // Acquire lock
    DP("[r " << id << "] Got lock.")

    // Now, read the data.
    auto val = mapped_data->data[id][size];

    if (val.done) {
        DP("[r " << id << "] Received done signal from mapper.")
        break; 
    }

    DP("[r " << id << "] Updating topic \"" << val.topic << "\"...")

    // This is okay, since operator[] will construct a default value if it
    // doesn't exist. Nice!
    total_scores[val.topic] += val.score_adjustment;

    pthread_mutex_unlock(&mapped_data->locks[id]);  // Release lock

  } // end of while(true)
  
  DP("r[ " << id << "] total_scores.size() = " << total_scores.size())
  D(
      // Print resulting map
      for (auto &e
           : total_scores) {
        printf("[r %d]  Topic \"%s\": %d\n", id, e.first.c_str(), e.second);
      })

    DP("[r " << id << "] Done. Goodbye.")
}

/**
 * @brief Initialize mmapp()ed region.
 * @return mmapped_region_t*
 */
mmapped_region_t *mapped_mem_init() {
    auto *shared_region = (mmapped_region_t *)mmap(
        NULL, 
        sizeof(mmapped_region_t),
        PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_ANONYMOUS, 
        -1, 
        0
    );
    
    if (shared_region == MAP_FAILED) {
        printf("Error: Couldn't map memory region.\n");
        exit(EXIT_FAILURE);
    }

    // Initialize shared region.
    shared_region->init();

    return shared_region;
}

/**
 * @brief fork() num_reducers times.
 * @param mapped_region
 * @param num_reducers
 */
void fork_it_up(mapped_data_structure *mapped_region, int num_reducers) {
  for (int i = 0; i < num_reducers; i++) {
    auto result = fork();

    if (result == -1) {
      printf("Error: fork() didn't work.\n");
      exit(EXIT_FAILURE);
    } else if (result == 0) {
      // Child process
      reducer(mapped_region, i);  // Run the reducer.
      exit(EXIT_SUCCESS);         // Exit this process
    }
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

  const auto buf_size = std::stoi(argv[1]);
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
  auto mapped_region = mapped_mem_init();

  // D(dump_shared_mem(mapped_region);)

  fork_it_up(mapped_region, num_reducers);

  // Now that all the reducers are spun up, run the mapper.
  mapper(tokens, mapped_region);

  D(dump_shared_mem(mapped_region);)

  DP("Waiting for reducer procs to finish...")

  for (int i = 0; i < num_reducers; i++) {
    wait(NULL);
  }

  // Destroy mutexes and semaphores.
  mapped_region->destroy();

  // Unmap shared memory
  if (munmap(mapped_region, sizeof(mapped_data_structure)) == -1) {
    printf("Error: Problem unmapping memory.\n");
    exit(EXIT_FAILURE);
  }

  DP("Goodbye.")
}
