#!/bin/sh
# Copyright (C) 2007 Richard Hughes <richard@hughsie.com>
#
# Licensed under the GNU General Public License Version 2
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

#$1 = method name
execute_dbus_method ()
{
	dbus-send --session --dest=org.freedesktop.PowerManagement		\
		  --type=method_call --print-reply --reply-timeout=2000	\
		  /org/freedesktop/PowerManagement 			\
		  org.freedesktop.PowerManagement.$1
	if [ $? -ne 0 ]; then
		echo "Failed"
	fi 
}

if [ "$1" = "suspend" ]; then
	echo "Suspending"
	execute_dbus_method "Suspend"
elif [ "$1" = "hibernate" ]; then
	echo "Hibernating"
	execute_dbus_method "Hibernate"
elif [ "$1" = "reboot" ]; then
	echo "Rebooting"
	execute_dbus_method "Reboot"
elif [ "$1" = "shutdown" ]; then
	echo "Shutting down"
	execute_dbus_method "Shutdown"
elif [ "$1" = "" ]; then
	echo "command required: suspend, shutdown, hibernate or reboot"
else
	echo "command '$1' not recognised, only suspend, shutdown, hibernate or reboot are valid"
	exit 1
fi
