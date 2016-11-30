# Copyright (C) 2016 Richard Hughes <richard@hughsie.com>
#
# Licensed under the GNU General Public License Version 2 or later

i18n.merge_file(
  input: 'org.gnome.PowerStats.appdata.xml.in',
  output: 'org.gnome.PowerStats.appdata.xml',
  type: 'xml',
  po_dir: join_paths(meson.source_root(), 'po'),
  install: true,
  install_dir: join_paths(get_option('datadir'), 'appdata')
)
