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
    'gpm-rotated-widget.c',
    'gpm-statistics.c',
    'egg-graph-point.c',
    'egg-graph-widget.c',
  ],
  include_directories : [
    include_directories('..'),
  ],
  dependencies : [
    cairo,
    gtk,
    libm,
    upower
  ],
  c_args : cargs,
  install : true,
  install_dir : get_option('bindir')
)

if get_option('enable-tests')
  e = executable(
    'gnome-power-self-test',
    sources : [
      'gpm-array-float.c',
      'gpm-self-test.c'
    ],
    include_directories : [
      include_directories('..'),
    ],
    dependencies : [
      cairo,
      gtk,
      libm,
      upower
    ],
    c_args : cargs
  )
  test('gnome-power-self-test', e)
endif
