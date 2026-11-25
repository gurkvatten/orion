#!/bin/bash

# This script runs cmake with limited CPU affinity to 16 CPUs which
# which helps to reduce memory (RAM) pressure issues
taskset -c 0-15 cmake "$@"