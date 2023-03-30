.PHONY: all clean

TARGET?=mm

all: read_skip.png write_skip.png

read_skip.png: read_skip.dat
	gnuplot -c read_skip.plt

read_skip.dat:
	./mkread_skip.sh > $@

write_skip.png: write_skip.dat
	gnuplot -c write_skip.plt

write_skip.dat:
	./mkwrite_skip.sh > $@

histograms:
#	./extract.sh
	gnuplot -e "benchname='$(TARGET)'" -c histogram_producer_overwriter.plt
	gnuplot -e "benchname='$(TARGET)'" -c histogram_producer_consumer.plt
	gnuplot -e "benchname='$(TARGET)'" -c histogram_use.plt

clean:
	$(RM) *.dat *.png
