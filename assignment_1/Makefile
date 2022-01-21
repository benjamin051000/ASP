OUTPUT=build/

build: mapper.c reducer.c
	mkdir -p $(OUTPUT)
	gcc -Wall -Wextra -o $(OUTPUT)mapper mapper.c
	gcc -Wall -Wextra -o $(OUTPUT)reducer reducer.c

clean:
	rm -r $(OUTPUT)
