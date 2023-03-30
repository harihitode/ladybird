set terminal png
set output benchname.".p_to_c.png"
unset key
set xlabel "Numbor of Instructions (Producer to Consumer)"
set ylabel ""
set xrange [0:50]
set title benchname." Histogram of Instructions between Producer and Consumer"
set boxwidth 1.0 relative
set style fill solid border lc rgb "black"
filter(x,y)=int(x/y)*y
plot benchname.".log.read" using (filter($4,1.0)):(1.0) smooth freq with boxes
