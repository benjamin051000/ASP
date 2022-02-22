CC=g++
OUTPUT=transfProg
CFLAGS=-Wall -Wextra -g -o $(OUTPUT)

all: main

$(OUTPUT): main.cpp
	$(CC) $(CFLAGS) main.cpp

run: $(OUTPUT)
	./$(OUTPUT) a3_input.txt 3

test: $(OUTPUT)
	./$(OUTPUT) input_hw3.txt 3

clean:
	rm $(OUTPUT)
