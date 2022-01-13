#include <stdio.h>
#include <stdlib.h>

/**
 * @brief Read entire text file.
 * WARNING: You must free() the returned char*!
 */
char* read(const char* filename) {
    FILE *fp;
    if((fp = fopen(filename, "r")) == NULL) {
        printf("Error opening file.\n");
        exit(1);
    }

    // Get size of file
    fseek(fp, 0L, SEEK_END);
    const long size = ftell(fp);
    // Reset offset
    fseek(fp, 0L, SEEK_SET);

    // Allocate buffer
    char* buf = malloc(size * sizeof *buf);

    // Read data into buf
    unsigned index = 0;
    int curr_char;

    while((curr_char = fgetc(fp)) != EOF) {
        // Get next char
        buf[index] = curr_char;

        index++;
    }

    fclose(fp);

    // Debug print
    // printf("buf: \"%s\"\n", buf);

    return buf;
}

int main(void) {

    char* text = read("input.txt");

    printf("text: \"%s\"\n", text);

    return 0;
}