#!/bin/bash

# Assignment 1 run script

# Mapper test
./mapper < input.txt

# Reducer test
./reducer < Mapper_Output.txt

# Combiner test
./combiner < input.txt

