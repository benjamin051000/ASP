CC=g++
OUTPUT=a4
CFLAGS=-Wall -Wextra -pthread -g -o $(OUTPUT)
BUF_SIZE=10
NUM_REDUCERS=7
# Modify INPUT_DIR to change where the input files are located.
INPUT_DIR=./tests

all: $(OUTPUT)

$(OUTPUT): main.cpp
	$(CC) $(CFLAGS) main.cpp

run: $(OUTPUT)
	./$(OUTPUT) $(BUF_SIZE) $(NUM_REDUCERS) < $(INPUT_DIR)/input.txt

simple: $(OUTPUT)
	./$(OUTPUT) $(BUF_SIZE) $(NUM_REDUCERS) < $(INPUT_DIR)/simple_input.txt

test: SHELL:=/bin/bash   # Set the shell for test only
test: $(OUTPUT)
	@diff <(sort -n $(INPUT_DIR)/output.txt) <(make -s run | sort -n)
	@echo Done.

clean:
	rm $(OUTPUT)
