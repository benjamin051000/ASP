# Assignment 4
**Note**: This Makefile configuration specifies input files in `./tests`. To change this, modify the `INPUT_DIR` variable in the `Makefile`.

## Command-line Input Behavior
The behavior of each command line argument is described below:
### Buffer size
If too small, the application will crash if it attempts to write too many times to the buffer.

### Number of Reducer processes
If too small, the application will crash if it attempts to create too many processes. While the code could easily be modified to create as many reducer processes as necessary, it was chosen not to do this.

<br/>

*In general, keep these values greater than or equal to the amounts necessary for a given input file.*

# make commands
## Build
Run `make` to build the executable.

## Run
Run `make run`, which will run the project. You can edit the Makefile variables to change the command-line arguments passed into the program.

## Test
Run `make test` to automatically run the executable and compare its output with the correct output via `diff`.

## Clean
Run `make clean` to remove the executable.
