#!/usr/bin/python
import dbus
import time

bus = dbus.Bus(dbus.Bus.TYPE_SESSION)
devobj = bus.get_object('org.gnome.PowerManager', '/org/gnome/PowerManager')
dev = dbus.Interface (devobj, "org.gnome.PowerManager")

cookie = dev.Inhibit('Nautilus', 'Copying files')
cookie2 = dev.Inhibit('SELinux', 'Performing database relabel')
time.sleep(10)
dev.UnInhibit(cookie)
dev.UnInhibit(cookie2)
