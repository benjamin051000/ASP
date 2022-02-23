#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <vector>

#define DEBUG_MODE

// Usage: `D(cout << "debug print\n";)
// If DEBUG_MODE is not defined, these won't be compiled. Nice, right?
#ifdef DEBUG_MODE
#define D(x) x
#else 
#define D(x)
#endif

using std::string;
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

int main(int argc, char* argv[]) {
    // Ensure 2 CLI args are passed in
    if (argc != 3) {
        std::cerr << "Usage: ./transfProg <inputFile> <numWorkers>\n";
        exit(EXIT_FAILURE);
    }

    // Extract CLI args
    const string INPUT_FILENAME = argv[1];
    const int NUM_WORKERS = std::stoi(argv[2]);

    D(std::cout << "input filename: " << INPUT_FILENAME
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
    auto tokens = split(s, " ");
    D(for (auto& token : tokens) {
        std::cout << token << "\n";
    })


    D(std::cout << "Done.\n";)
    return 0;
}
