#!/bin/sh

export G_DEBUG=fatal_criticals
killall gnome-power-manager

cycles=20
for ((i=0;i<=$cycles;i+=1)); do
 echo -n "$i.."
./gnome-power-manager --verbose --no-daemon --immediate-exit &> /dev/null
done

echo ""
echo "total process time:"
times
