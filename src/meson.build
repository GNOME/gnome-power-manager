# Copyright (C) 2016 Richard Hughes <richard@hughsie.com>
#
# Licensed under the GNU General Public License Version 2 or later

cargs = ['-DG_LOG_DOMAIN="Gpm"']

gnome_power_statistics_resources = gnome.compile_resources(
  'gnome-power-statistics-resources',
  'gnome-power-manager.gresource.xml',
  source_dir : '.',
  c_name : 'gpm'
)

executable(
  'gnome-power-statistics',
  gnome_power_statistics_resources,
  sources : [
    'gpm-array-float.c',
    'gpm-array-float.h',
    'gpm-statistics.c',
    'egg-graph-point.c',
    'egg-graph-point.h',
    'egg-graph-widget.c',
    'egg-graph-widget.h',
  ],
  include_directories : [
    include_directories('..'),
  ],
  dependencies : [
    cairo,
    gdk,
    gtk,
    libm,
    upower
  ],
  c_args : cargs,
  install : true,
  install_dir : 'bin'
)

if get_option('enable-tests')
  e = executable(
    'gnome-power-self-test',
    sources : [
      'gpm-array-float.c',
      'gpm-array-float.h',
      'gpm-self-test.c'
    ],
    include_directories : [
      include_directories('..'),
    ],
    dependencies : [
      cairo,
      gdk,
      gtk,
      libm,
      upower
    ],
    c_args : cargs
  )
  test('gnome-power-self-test', e)
endif
