#!/bin/sh
# Copyright (C) 2016 Richard Hughes <richard@hughsie.com>
#
# Licensed under the GNU General Public License Version 2 or later

if [ -z $MESON_INSTALL_PREFIX ]; then
    echo 'This is meant to be ran from Meson only!'
    exit 1
fi

if [ -z $DESTDIR ]; then
    echo 'Compiling GSchema'
    glib-compile-schemas "$MESON_INSTALL_PREFIX/share/glib-2.0/schemas"
    echo 'Updating icon cache'
    gtk-update-icon-cache -qtf "$MESON_INSTALL_PREFIX/share/icons/hicolor"
    echo 'Updating desktop database'
    update-desktop-database -q "$MESON_INSTALL_PREFIX/share/applications"
fi
