#include <iostream>
#include <iomanip>
#include <stdio.h>  // fgetc()
#include <stdlib.h> // malloc()
#include <string.h>
#include <pthread.h>
#include <string>
#include <unordered_map>

/**
 * @brief Read a text file into a buffer.
 * WARNING: You must free() the returned char*!
 */
char *readTextFile(void)
{
    const int BUF_SIZE = 1500;

    char *buffer = (char *)malloc(BUF_SIZE * sizeof *buffer);

    int index = 0;

    int c;
    while ((c = fgetc(stdin)) != EOF)
    {
        buffer[index++] = c;
    };

    return buffer;
}

void mapper(char *text)
{
    const auto delims = "(), \n";

    int token_i = 0;

    char *token = strtok(text, delims);

    std::unordered_map<std::string, int> action_points = {
        {"P", 50},
        {"L", 20},
        {"D", -10},
        {"C", 30},
        {"S", 40}};

    while (token != NULL)
    {
        int id, score;

        if (token_i == 0)
        {
            // This is the first of the three tuple entries, which is the ID.
            id = atoi(token);
            token_i++;
        }
        else if (token_i == 1)
        {
            // This is the second tuple entry, which is the category.
            auto action = std::string(token);
            score = action_points[action];
            token_i++;
        }
        else
        {
            auto topic = token;

            // TODO: move into buffer for reducer threads
            std::cout << "(" << std::setw(4) << std::setfill('0') << id << ", " << topic << ", " << score << ")\n";

            token_i = 0;
        }

        token = strtok(NULL, delims);
    } // end of while
} // end of mapper

int main(int argc, char *argv[])
{

    if (argc != 3)
    {
        std::cout << "Usage: combiner <no. slots> <no. reducer threads>\n";
        return -1;
    }

    const auto num_com_buf_slots = std::stoi(argv[1]);
    const auto num_users = std::stoi(argv[2]);

    // Read text file
    const auto text = readTextFile();

    mapper(text);

    // std::cout << text << '\n';

    return 0;
}