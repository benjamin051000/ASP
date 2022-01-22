OUTPUT=build/

build: mapper.c reducer.c combiner.c
	mkdir -p $(OUTPUT)
	gcc -Wall -Wextra -o $(OUTPUT)mapper mapper.c
	gcc -Wall -Wextra -o $(OUTPUT)reducer reducer.c
	gcc -Wall -Wextra -o $(OUTPUT)combiner combiner.c
	

clean:
	rm -r $(OUTPUT)
