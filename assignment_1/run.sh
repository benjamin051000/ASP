#!/bin/bash

# Assignment 1 run script

# Mapper test
./build/mapper < input.txt

# Reducer test
./build/reducer < Mapper_Output.txt

# Combiner test
./build/combiner < input.txt
