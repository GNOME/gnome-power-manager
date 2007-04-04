killall gnome-power-manager
export G_SLICE=always-malloc
export G_DEBUG=gc-friendly
valgrind --tool=memcheck --leak-check=full --leak-resolution=high ./gnome-power-manager --verbose --no-daemon --timed-exit &> ./debug-valgrind.log

