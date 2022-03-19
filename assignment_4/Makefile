CC=g++
OUTPUT=a4
CFLAGS=-Wall -Wextra -pthread -g -o $(OUTPUT)
BUF_SIZE=10
NUM_REDUCERS=1

all: $(OUTPUT)

$(OUTPUT): main.cpp
	$(CC) $(CFLAGS) main.cpp

run: $(OUTPUT)
	./$(OUTPUT) $(BUF_SIZE) $(NUM_REDUCERS) < ./tests/input.txt

simple: $(OUTPUT)
	./$(OUTPUT) 5 1 < ./tests/simple_input.txt

test: SHELL:=/bin/bash   # Set the shell for test only
test: $(OUTPUT)
	@diff <(sort -n tests/output.txt) <(make -s run | sort -n)
	@echo Done.

clean:
	rm $(OUTPUT)
