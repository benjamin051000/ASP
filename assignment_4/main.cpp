#include <cstdio>
#include <cstdlib>
#include <regex>
#include <string>
#include <vector>
#include <iostream>
#include <unordered_map>
#include <sys/mman.h>
using std::string;

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

struct mapped_data_t;

struct mapped_data_structure {
    // TODO make this a map with IDs as keys and Queues and stuff as values
    std::vector<mapped_data_t> data;
};


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
 * All mapper-related code
 */
// namespace mapper {

/**
 * @brief Struct to represent the original id/topic/action info from stdin.
 */
struct input_data_t {
    const userid_t id;
    const topic_t topic; // TODO topics still have trailing whitespace, consider stripping it
    const string action;
};


/**
 * @brief Tuple to hold score adjustments per topic for an ID.
 * Mapper will send this info to the reducer designated for this action's ID.
 */
struct mapped_data_t {
    topic_t topic;
    score_t score_adjustment;
};

/**
 * @brief Parse tokens into actions.
 * 
 * @param tokens 
 */
std::vector<input_data_t> parse_actions(const std::vector<string>& tokens) {
    
    DP("----------parse_actions()----------")
    std::vector<input_data_t> actions;

    // Iterate through the tokens and produce action tuples.
    auto it = tokens.begin();
    while(it != tokens.end()) {
        actions.push_back(
            // TODO Does this copy? I think it does
            input_data_t {
                *it++,
                *it++,
                *it++
            }
        );
    }

    D(for(const auto& action: actions) {
        std::cout << "action {" << action.id << ", " << action.topic << ", " << action.action << "}\n";
    })
    DP("----------parse_actions() done.----------")
    return actions;
}

/**
 * @brief Mapper worker process
 */
void mapper(const std::vector<string>& tokens, mapped_data_structure* shared_mem) {

    const auto actions = parse_actions(tokens);

    // Map that coorelates an action to its cooresponding point value.
    const std::unordered_map<string, score_t> action_points{
        {"P", 50}, {"L", 20}, {"D", -10}, {"C", 30}, {"S", 40}};

    // Next, send the mapped data to the reducer.
    for(auto& action: actions) {
        const auto score = mapped_data_t {
            action.topic,
            action_points.at(action.action)
        };

        // Send to mapped region
        shared_mem->data.push_back(score);
    }
}


// } // end of namespace mapper_stuff


//////////////////////////////////////////////////////

int main(int argc, char* argv[]) {
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
    for (const auto& e : temp_tokens) {
        if (e.find_first_not_of(' ') != string::npos) tokens.push_back(e);
    }

    D(for (const auto& tok : tokens) {
        printf("%s\n", tok.c_str());
    })

    // Set up shared memory
    auto *mapped_region = (mapped_data_structure*)mmap(NULL, sizeof(mapped_data_structure), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if(mapped_region == MAP_FAILED) {
        printf("Error: Couldn't map memory region.\n");
        exit(EXIT_FAILURE);
    }
    *mapped_region = mapped_data_structure();

    mapper(tokens, mapped_region);

    // Unmap shared memory
    if(munmap(mapped_region, sizeof(mapped_data_structure)) == -1) {
        printf("Error: Problem unmapping memory.\n");
        exit(EXIT_FAILURE);
    }

    DP("Goodbye.")
}
