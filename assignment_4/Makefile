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

# hw: $(OUTPUT)
# ./$(OUTPUT) tests/input_hw3.txt $(NUM_WORKERS)

# simple: $(OUTPUT)
# ./$(OUTPUT) tests/simple_input.txt $(NUM_WORKERS)

clean:
	rm $(OUTPUT)
