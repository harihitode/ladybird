#!/bin/sh

TESTS=`cat tests.txt`
OUTPUT=write_skip.dat

for item in ${TESTS}
do
    WSKIP=`tail -n 1 ${item}.riscv.log | cut -d " " -f 3`
    echo ${item} ${WSKIP}
done
