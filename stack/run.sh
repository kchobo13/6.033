#!/usr/bin/env bash

ulimit -s unlimited
echo 'Ben Bitdiddle    F' > grades.txt
echo 'Alice Jones      A' >> grades.txt
exec env - SHLVL=0 "$@"
