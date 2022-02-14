OUTPUT=build/
FLAGS=-Wall -Wextra -pthread

# Build each executable into the output directory.
build: main.cpp
	mkdir -p $(OUTPUT)
	g++ $(FLAGS) -o $(OUTPUT)main main.cpp

# Run the programs as defined in run.sh. Ensure they are built, first.
run: build
	$(OUTPUT)main 10 2 < input.txt

test:
	for i in {1..1000}; do echo $i; make run > /dev/null; done; echo $i tests completed.

# Delete the output directory.
clean:
	rm -r $(OUTPUT)
