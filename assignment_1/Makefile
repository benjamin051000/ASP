OUTPUT=build/

build: mapper.c reducer.c
	mkdir -p $(OUTPUT)
	gcc -o $(OUTPUT)mapper mapper.c
	gcc -o $(OUTPUT)reducer reducer.c

clean:
	rm -r $(OUTPUT)
