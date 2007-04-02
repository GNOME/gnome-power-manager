#!/usr/bin/python
import dbus
import time

bus = dbus.Bus(dbus.Bus.TYPE_SESSION)
devobj = bus.get_object('org.freedesktop.PowerManagement', '/org/freedesktop/PowerManagement')
dev = dbus.Interface (devobj, "org.freedesktop.PowerManagement.Inhibit")

cookie = dev.Inhibit('Nautilus', 'Copying files')
cookie2 = dev.Inhibit('SELinux', 'Performing database relabel')

time.sleep(10)

dev.UnInhibit(cookie)
dev.UnInhibit(cookie2)

