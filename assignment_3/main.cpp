#include <pthread.h>
#include <semaphore.h>

#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <queue>

#define DEBUG_MODE
// Usage: `D(cout << "debug print\n";)
// If DEBUG_MODE is not defined, these won't be compiled. Nice, right?
#ifdef DEBUG_MODE
#define D(x) x
#else
#define D(x)
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

    AccountData(int val): val(val) {
        pthread_mutex_init(&lock, NULL);
    }

    ~AccountData() {
        pthread_mutex_destroy(&lock);
    }
};

/**
 * @brief Transaction details
 */
class Transaction {
public:
    string src, dest;
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
    D(for (auto& key: accounts) {
        cout << key.first << ": " << key.second.val << "\n";
    })

    // Ensure the iterator stopped just before the first "Transfer" token.
    // If it did, the iterator successfully went through each initial account ID and value.
    if(*iter != "Transfer") {
        std::cerr << "ERROR: Token iterator did not parse account initial values properly.\n";
        exit(EXIT_FAILURE);
    }

    // Return the index the iterator stopped at.
    return iter - tokens.begin();
}

// TODO is it smart to combine these into one class? Should pthread_t be in it's own DS? Not sure.
struct WorkerData {
    pthread_t thread;
    sem_t q_size;
    std::queue<Transaction> q;

    WorkerData(pthread_t t) : thread(t) {
        sem_init(&q_size, 0, 0);
    }

    ~WorkerData() {
        sem_destroy(&q_size);
    }
};

/**
 * @brief Worker threads with their queues and semaphores
 */
std::vector<WorkerData> workers; // TODO does this need to be global? Probably...

/**
 * @brief Round-robin index to determine which worker to send data to next.
 * 
 */
int next_worker = 0; // TODO does this need to be global? Probably...

void dispatch_transfer_jobs(const std::vector<string>& tokens, const int offset, const int num_workers) {
    
    // Create threads.
    for (int i = 0; i < num_workers; i++) {
        pthread_t t;
        pthread_create(&t, NULL, &worker, NULL);
        workers.push_back(WorkerData(t));
    }
    
    auto it = tokens.begin() + offset;
    
    while(it != tokens.end()) {
        
        D(cout << *it << "\n");
        
        // First one is "Transfer". Skip it.
        it++;

        string source = *it++;

        string dest = *it++;

        int amt = std::stoi(*it++);
        
        // Create transaction object
        Transaction t{source, dest, amt};
        

        // Put it in the queue to the next worker thread in the round robin.
        // TODO this is where you left off
    }
}

void* worker(void* args) {
    // Acquire one mutex
    // Acquire other mutex
    // Complete transaction
    // Release both mutexes
    return nullptr; 
}

int main(int argc, char* argv[]) {
    // Ensure 2 CLI args are passed in
    if (argc != 3) {
        std::cerr << "Usage: ./transfProg <inputFile> <numWorkers>\n";
        exit(EXIT_FAILURE);
    }

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
    auto tokens = split(s, "[ \n]"); // Delimiters are spaces OR newlines.
    D(for (auto& token : tokens) { cout << token << "\n"; })

    const auto transfer_idx = init_accounts(tokens);
    
    dispatch_transfer_jobs(tokens, transfer_idx, NUM_WORKERS);

    // // Join threads
    // for (auto& t : threads) {
    //     pthread_join(t, NULL);
    // }

    D(cout << "Done.\n";)
    return 0;
}
