#include <cstdio>
#include <cstdlib>
#include <regex>
#include <string>
#include <vector>
using std::string;

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

int main(char argc, char* argv[]) {
    const auto text = read_stdin();

    // delims are anything inside the []s
    const auto temp_tokens = split(text, "[(),]");

    std::vector<string> tokens;
    // Filter whitespace tokens out
    for (const auto& e : temp_tokens) {
        if (e.find_first_not_of(' ') != string::npos) tokens.push_back(e);
    }

    for (const auto& tok : tokens) {
        printf("%s\n", tok.c_str());
    }

    return 0;
}
