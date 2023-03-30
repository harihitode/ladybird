set terminal png
set output "write_skip.png"
unset key
set yrange [0.0:1.0]
set xlabel "Benchmarks" offset 0,2
set xtics rotate by -60
set ylabel "Reduction Rate (%)"
set title "Reduction Rate of Register Write Access"
set boxwidth 0.5 relative
set style fill solid border lc rgb "black"
plot "write_skip.dat" using 2:xticlabels(1) with boxes
replot
