CC=g++
OUTPUT=transfProg
CFLAGS=-Wall -Wextra -pthread -g -o $(OUTPUT)
NUM_WORKERS=2

all: $(OUTPUT)

$(OUTPUT): main.cpp
	$(CC) $(CFLAGS) main.cpp

run: $(OUTPUT)
	./$(OUTPUT) a3_input.txt $(NUM_WORKERS)

hw: $(OUTPUT)
	./$(OUTPUT) input_hw3.txt $(NUM_WORKERS)

clean:
	rm $(OUTPUT)
