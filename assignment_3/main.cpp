#include <iostream>

#define DEBUG_MODE

// Usage: `D(cout << "debug print\n";)
// If DEBUG_MODE is not defined, these won't be compiled. Nice, right?
#ifdef DEBUG_MODE
#define D(x) x
#else 
#define D(x)
#endif

using std::string;

int main(int argc, char* argv[]) {

    const string INPUT_FILENAME = argv[1];
    const int NUM_WORKERS = std::stoi(argv[2]);
    
    D(std::cout << "input filename: " << INPUT_FILENAME << "\nnum workers: " << NUM_WORKERS << "\n";)

    D(std::cout << "Done.\n";)
    return 0;
}
