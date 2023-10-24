#!/bin/sh

for bench in `cat eval/tests.txt`
do
	echo "======"
	echo $bench
	./launch_sim ~/riscv-benchmarks/$bench.riscv --stat --htif --tohost 0x80001000 --fromhost 0x80001040 --config-rom
	echo "======"
done
