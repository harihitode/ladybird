.PHONY: all clean

all: read_skip.png write_skip.png

read_skip.png: read_skip.dat
	gnuplot -c read_skip.plt

read_skip.dat:
	./mkread_skip.sh > $@

write_skip.png: write_skip.dat
	gnuplot -c write_skip.plt

write_skip.dat:
	./mkwrite_skip.sh > $@

clean:
	$(RM) *.dat *.png
