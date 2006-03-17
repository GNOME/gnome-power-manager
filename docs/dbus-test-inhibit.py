#!/usr/bin/python
import dbus
import time

bus = dbus.Bus(dbus.Bus.TYPE_SESSION)
devobj = bus.get_object('org.gnome.PowerManager', '/org/gnome/PowerManager')
dev = dbus.Interface (devobj, "org.gnome.PowerManager")

cookie = dev.InhibitInactiveSleep('Test Application', 'Copying files')
cookie2 = dev.InhibitInactiveSleep('Test Application', 'Copying *MORE* files')
time.sleep(10)
dev.AllowInactiveSleep(cookie)
dev.AllowInactiveSleep(cookie2)
