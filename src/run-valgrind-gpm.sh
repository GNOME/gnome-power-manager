killall gnome-power-manager
valgrind --tool=memcheck --leak-check=full --leak-resolution=med ./gnome-power-manager --verbose --no-daemon --timed-exit
