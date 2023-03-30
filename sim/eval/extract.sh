#!/bin/sh

for bench in `cat tests.txt`
do
    awk -e '{ if ($3 == "W") print $0 }' $bench.riscv.log > $bench.log.write
    awk -e '{ if ($3 == "R") print $0 }' $bench.riscv.log > $bench.log.read
done
