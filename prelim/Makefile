OUTPUT=build/

# all: mapper reducer

build:
	mkdir -p $(OUTPUT)
	gcc -o $(OUTPUT)mapper mapper.c
	gcc -o $(OUTPUT)reducer reducer.c

run: build
	$(OUTPUT)mapper | $(OUTPUT)reducer

clean:
	rm -r $(OUTPUT)
