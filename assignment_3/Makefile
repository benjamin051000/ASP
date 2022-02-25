CC=g++
OUTPUT=transfProg
CFLAGS=-Wall -Wextra -pthread -g -o $(OUTPUT)
NUM_WORKERS=1

all: $(OUTPUT)

$(OUTPUT): main.cpp
	$(CC) $(CFLAGS) main.cpp

run: $(OUTPUT)
	./$(OUTPUT) tests/a3_input.txt $(NUM_WORKERS)

hw: $(OUTPUT)
	./$(OUTPUT) tests/input_hw3.txt $(NUM_WORKERS)

simple: $(OUTPUT)
	./$(OUTPUT) tests/simple_input.txt $(NUM_WORKERS)

clean:
	rm $(OUTPUT)
