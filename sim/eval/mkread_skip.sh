#!/bin/sh

TESTS=`cat tests.txt`
OUTPUT=read_skip.dat

for item in ${TESTS}
do
    WSKIP=`tail -n 1 ${item}.riscv.log | cut -d " " -f 5`
    echo ${item} ${WSKIP}
done
