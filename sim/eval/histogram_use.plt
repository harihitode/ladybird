set terminal png
set output benchname.".use.png"
unset key
set xlabel "Usage Count of Data"
set ylabel ""
set xrange [0:50]
set title benchname." Histogram of Usage Count of Produced Data (ALU ONLY)"
set boxwidth 1.0 relative
set style fill solid border lc rgb "black"
filter(x,y)=int(x/y)*y
plot benchname.".log.write" using (filter($5,1.0)):(1.0) smooth freq with boxes
