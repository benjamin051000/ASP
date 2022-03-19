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


clean:
	rm $(OUTPUT)
