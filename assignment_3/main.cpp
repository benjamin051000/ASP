#include <pthread.h>
#include <semaphore.h>

#include <fstream>
#include <iostream>
#include <queue>
#include <regex>
#include <sstream>
#include <unordered_map>
#include <vector>

// #define DEBUG_MODE
// Usage: `D(cout << "debug print\n";)
// If DEBUG_MODE is not defined, these won't be compiled. Nice, right?
#ifdef DEBUG_MODE
#define D(x) x

pthread_mutex_t cout_lock;
#define COUTL pthread_mutex_lock(&cout_lock)
#define COUTU pthread_mutex_unlock(&cout_lock)

#define DPRINTL(x)     \
    COUTL;             \
    cout << x << "\n"; \
    COUTU;
#else
// Give them definitions that get compiled away
#define D(x)
#define COUTL
#define COUTU
#define DPRINTL(x)
#endif

using std::cout;
using std::string;
using std::unordered_map;
using account_id_t = string;

/**
 * @brief An account, with an account number and a value
 * (representing the amount of money they have.)
 */
struct AccountData {
    pthread_mutex_t lock;
    int val;

    AccountData(int val) : val(val) { pthread_mutex_init(&lock, NULL); }

    ~AccountData() { pthread_mutex_destroy(&lock); }
};

/**
 * @brief Transaction details
 */
class Transaction {
   public:
    account_id_t src, dest;
    int amt;
};

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

void* worker(void* args);

/**
 * @brief Stores accounts and their values.
 */
unordered_map<account_id_t, AccountData> accounts;

/**
 * @brief Initialize accounts map
 *
 * @param tokens vector of string tokens.
 * @return
 */
int init_accounts(const std::vector<string>& tokens) {
    auto iter = tokens.begin();

    while (iter != tokens.end() and *iter != "Transfer") {
        // Get account number
        account_id_t account = *iter++;

        int val = std::stoi(*iter++);

        // Add to the map
        accounts.insert({account, AccountData(val)});
    }

    D(std::cout << "-----Accounts created-----\n";)

    // Print the map out
    D(for (auto& key
           : accounts) { cout << key.first << ": " << key.second.val << "\n"; })

    // Ensure the iterator stopped just before the first "Transfer" token.
    // If it did, the iterator successfully went through each initial account ID
    // and value.
    if (*iter != "Transfer") {
        std::cerr << "ERROR: Token iterator did not parse account initial "
                     "values properly.\n";
        exit(EXIT_FAILURE);
    }

    // Return the index the iterator stopped at.
    return iter - tokens.begin();
}

// TODO is it smart to combine these into one class? Should pthread_t be in it's
// own DS? Not sure.
struct WorkerData {
    /**
     * Thread ID
     */
    pthread_t thread;

    sem_t q_size;
    pthread_mutex_t q_lock;
    std::queue<Transaction> q;

    WorkerData(pthread_t t) : thread(t) {
        sem_init(&q_size, 0, 0);
        pthread_mutex_init(&q_lock, NULL);
    }

    ~WorkerData() {
        sem_destroy(&q_size);
        pthread_mutex_destroy(&q_lock);
    }
};

/**
 * @brief Worker threads with their queues and semaphores
 */
std::vector<WorkerData> workers;

/**
 * @brief global flag to alert workers when there 
 * is no new data and they can stop working.
 */
bool no_new_data = false;

void dispatch_transfer_jobs(const std::vector<string>& tokens, const int offset,
                            const int num_workers) {
    // Create threads.
    for (int i = 0; i < num_workers; i++) {
        pthread_t t;
        int* temp = new int(i);
        pthread_create(&t, NULL, &worker, static_cast<void*>(temp));
        // D(cout << "Created thread " << t << ", idx=" << i << "\n";)
        workers.push_back(WorkerData(t));
    }

    auto it = tokens.begin() + offset;

    /**
     * @brief Round-robin index to determine which worker to send data to next.
     */
    int next_worker = 0;  // TODO does this need to be global? Probably...

    while (it != tokens.end()) {
        // D(cout << *it << "\n");

        // First one is "Transfer". Skip it.
        it++;

        string source = *it++;

        string dest = *it++;

        int amt = std::stoi(*it++);

        // Create transaction object
        Transaction t{source, dest, amt};

        // Put it in the queue to the next worker thread in the round robin.
        // TODO this is where you left off
        D(int temp_idx = next_worker;)
        auto& conn = workers[next_worker++];

        // Keep within bounds of num_workers
        next_worker %= num_workers;

        // Acquire lock
        pthread_mutex_lock(&conn.q_lock);  // TODO this is failing

        conn.q.push(t);

        // DPRINTL("[d] Posting semaphore...")
        // Increment the semaphore.
        sem_post(&conn.q_size);

        D(int size;
        sem_getvalue(&conn.q_size, &size);)
        DPRINTL("[d] t " << temp_idx << " q_size after post: " << size)

        pthread_mutex_unlock(&conn.q_lock);
    }  // end of while

    // Alert worker threads that we are done.
    no_new_data = true;
}

void* worker(void* args) {
    // Get connection from args
    int idx;
    {
        // Inner scope in case I want to
        // reuse "temp" as a var name later
        int* temp = static_cast<int*>(args);
        idx = *temp;
        delete temp;
    }

    DPRINTL("[t " << idx << "] Started.")
    auto& conn = workers[idx];

    // While the dispatcher is still sending out data...
    while (true) {
        int curr_q_size;
        sem_getvalue(&conn.q_size, &curr_q_size);
        if (no_new_data and curr_q_size == 0) {
            break;
        }

        DPRINTL("[t " << idx << "] q_size before waiting: " << curr_q_size)
        DPRINTL("[t " << idx << "] waiting on q.size...")

        // Ensure there is data.
        sem_wait(&conn.q_size);

        // The main thread may have posted to
        // alert us that there is no new data.
        // Double check semaphore size, too.
        sem_getvalue(&conn.q_size, &curr_q_size);

        if(no_new_data and curr_q_size == 0) {
            break;
        }

        DPRINTL("[t " << idx << "] waiting on queue mutex...")

        // Get data from the queue
        pthread_mutex_lock(&conn.q_lock);

        // src, dest, amt
        auto data = conn.q.front();
        conn.q.pop();

        pthread_mutex_unlock(&conn.q_lock);

        // Next, DPP. We need to acquire the mutexes for src and dest accounts.
        // Then, we change their data.
        // Then, release both.

        // Since only the dispatcher writes to accounts, probably doesn't need a
        // mutex.
        auto& src = accounts.at(data.src);
        auto& dest = accounts.at(data.dest);

        // If this thread is even in the round-robin, acq src first.
        // Otherwise, acq dest first.
        if (idx % 2 == 0) {
            DPRINTL("[t " << idx << "] waiting on src mutex...")
            // Acquire one mutex
            pthread_mutex_lock(&src.lock);

            DPRINTL("[t " << idx << "] waiting on dest mutex...")
            // Acquire other mutex
            pthread_mutex_lock(&dest.lock);
        } else {
            // Acquire in reverse order (prevents deadlocking).
            DPRINTL("[t " << idx << "] waiting on dest mutex...")
            pthread_mutex_lock(&dest.lock);

            DPRINTL("[t " << idx << "] waiting on src mutex...")
            pthread_mutex_lock(&src.lock);
        }

        // Complete transaction
        src.val -= data.amt;
        dest.val += data.amt;

        // Release both mutexes
        pthread_mutex_unlock(&dest.lock);
        pthread_mutex_unlock(&src.lock);

        DPRINTL("[t " << idx << "] Transaction complete.")
    }

    DPRINTL("[t " << idx << "] done.")

    return nullptr;
}

int main(int argc, char* argv[]) {
    // Ensure 2 CLI args are passed in
    if (argc != 3) {
        std::cerr << "Usage: ./transfProg <inputFile> <numWorkers>\n";
        exit(EXIT_FAILURE);
    }

    D(pthread_mutex_init(&cout_lock, NULL);)

    // Extract CLI args
    const string INPUT_FILENAME = argv[1];
    const int NUM_WORKERS = std::stoi(argv[2]);

    D(cout << "input filename: " << INPUT_FILENAME
           << "\nnum workers: " << NUM_WORKERS << "\n";)

    // Open file
    std::ifstream file(INPUT_FILENAME);

    if (!file) {
        std::cerr << "ERROR: Invalid filename.\n";
        exit(EXIT_FAILURE);
    }

    // Use a string stream to read the buffered file.
    std::stringstream ss;
    ss << file.rdbuf();
    std::string s = ss.str();

    // Split tokens
    auto tokens = split(s, "[ \n]");  // Delimiters are spaces OR newlines.
    D(for (auto& token : tokens) { cout << token << "\n"; })

    const auto transfer_idx = init_accounts(tokens);

    D(cout << "-----Dispatching transactions-----\n";)

    dispatch_transfer_jobs(tokens, transfer_idx, NUM_WORKERS);

    DPRINTL("-----Dispatch done. Waiting on workers to finish.-----")

    // Post all the semaphores just in case one 
    // was blocked waiting for data when the dispatcher quit.
    for(int i = 0; i < NUM_WORKERS; i++) {
        auto& t = workers[i];

        sem_post(&t.q_size);
        int size;
        sem_getvalue(&t.q_size, &size);
        DPRINTL("t " << i << " q_size after post: " << size)
    }

    // Join threads
    for (auto& t : workers) {
        pthread_join(t.thread, NULL);
    }

    DPRINTL("----------")

    // Print final results
    for (auto& key: accounts) { 
        cout << key.first << " " << key.second.val << "\n"; 
    }

    D(pthread_mutex_destroy(&cout_lock);)

    D(cout << "----------\nDone.\n";)
    return 0;
}
