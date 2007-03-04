#!/bin/sh

#delete all old memory outputs, else we get hundreds
rm massif.*

valgrind --tool=massif --format=html --depth=10 \
	 --alloc-fn=g_malloc --alloc-fn=g_realloc \
	 --alloc-fn=g_try_malloc --alloc-fn=g_malloc0 --alloc-fn=g_mem_chunk_alloc \
	 ./gnome-power-manager --verbose --no-daemon --timed-exit

#massif uses the pid file, which is hard to process.
mv massif.*.html massif.html
mv massif.*.ps massif.ps
#convert to pdf, and make readable by normal users
ps2pdf massif.ps massif.pdf
chmod a+r massif.*
