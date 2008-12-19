if [ "$1" = "--help" ]; then
	echo "Usage:"
	echo "  $0 [run-options] [options]"
	echo
	echo "Run Options:"
	echo "  --debug:               Run with gdb"
	echo "  --profile:             Run in a timed loop"
	echo "  --memcheck:            Run with valgrind memcheck tool"
	echo "  --massif:              Run with valgrind massif heap-profiling tool"
	echo "  --efence:              Run with electric-fence chedking for overruns"
	echo "  --underfence:          Run with electric-fence checking for underruns"
	echo
	exit 0
fi

killall gnome-power-manager
export G_DEBUG=fatal_criticals
extra='--verbose'

if [ "$1" = "--profile" ] ; then
	shift
	cycles=20
	for ((i=0;i<=$cycles;i+=1)); do
	 echo -n "$i.."
	./gnome-power-manager $extra $@ --immediate-exit &> /dev/null
	done
	echo "total process time:"
	times
	exit 0
fi

if [ "$1" = "--massif2" ] ; then
	shift
	#delete all old memory outputs, else we get hundreds
	rm massif.*
	valgrind --tool=massif --format=html --depth=10 \
		 --alloc-fn=g_malloc --alloc-fn=g_realloc \
		 --alloc-fn=g_try_malloc --alloc-fn=g_malloc0 --alloc-fn=g_mem_chunk_alloc \
		 ./gnome-power-manager $extra $@ --timed-exit
	#massif uses the pid file, which is hard to process.
	mv massif.*.html massif.html
	mv massif.*.ps massif.ps
	#convert to pdf, and make readable by normal users
	ps2pdf massif.ps massif.pdf
#	chmod a+r massif.*
fi

if [ "$1" = "--debug" ] ; then
	shift
	prefix="gdb --args"
elif [ "$1" = "--memcheck" ] ; then
	shift
	prefix="valgrind --show-reachable=yes --leak-check=full --tool=memcheck --suppressions=./valgrind.supp $VALGRIND_EXTRA"
	extra="$extra --timed-exit"
	export G_DEBUG="gc-friendly"
	export G_SLICE="always-malloc"
elif [ "$1" = "--massif" ] ; then
	shift
	prefix="valgrind --tool=massif --suppressions=./valgrind.supp $VALGRIND_EXTRA"
	extra="$extra --timed-exit"
	export G_DEBUG="gc-friendly"
	export G_SLICE="always-malloc"
elif [ "$1" = "--efence" ] ; then
	shift
	prefix="gdb -x ./efence.gdb --args"
	extra="$extra --timed-exit"
	export G_DEBUG="gc-friendly"
	export G_SLICE="always-malloc"
elif [ "$1" = "--underfence" ] ; then
	shift
	prefix="gdb -x ./underfence.gdb --args"
	extra="$extra --timed-exit"
	export G_DEBUG="gc-friendly"
	export G_SLICE="always-malloc"
fi

echo "Execing: $prefix ./gnome-power-manager $extra $@"
$prefix ./gnome-power-manager $extra $@

