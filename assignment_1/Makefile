OUTPUT=build/

# Build each executable into the output directory.
build: mapper.c reducer.c combiner.c
	mkdir -p $(OUTPUT)
	gcc -Wall -Wextra -o $(OUTPUT)mapper mapper.c
	gcc -Wall -Wextra -o $(OUTPUT)reducer reducer.c
	gcc -Wall -Wextra -o $(OUTPUT)combiner combiner.c

# Run the programs as defined in run.sh. Ensure they are built, first.
run: build
	./run.sh

# Delete the output directory.
clean:
	rm -r $(OUTPUT)
