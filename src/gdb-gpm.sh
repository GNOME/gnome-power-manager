export G_DEBUG=fatal_criticals
killall gnome-power-manager
echo "run" > /tmp/gdb
echo "bt" >> /tmp/gdb
gdb --batch --command=/tmp/gdb --args ./gnome-power-manager --verbose --no-daemon
rm /tmp/gdb

