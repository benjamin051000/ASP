OUTPUT=build/
FLAGS=-Wall -Wextra -pthread -g

# Build each executable into the output directory.
build: main.cpp
	mkdir -p $(OUTPUT)
	g++ $(FLAGS) -o $(OUTPUT)main main.cpp

# Run the programs as defined in run.sh. Ensure they are built, first.
run: build
	$(OUTPUT)main 10 7 < input.txt

# Delete the output directory.
clean:
	rm -r $(OUTPUT)
