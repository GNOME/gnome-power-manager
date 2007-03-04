export G_DEBUG=fatal_criticals
killall gnome-power-manager
./gnome-power-manager --verbose --no-daemon | tee debug.log
