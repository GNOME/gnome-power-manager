subdir('appdata')
subdir('icons')

compiled = gnome.compile_schemas()
install_data('org.gnome.power-manager.gschema.xml',
  install_dir : 'share/glib-2.0/schemas'
)

i18n.merge_file(
  input: 'org.gnome.PowerStats.desktop.in',
  output: 'org.gnome.PowerStats.desktop',
  type: 'desktop',
  po_dir: join_paths(meson.source_root(), 'po'),
  install: true,
  install_dir: join_paths(get_option('datadir'), 'applications')
)

service_conf = configuration_data()
service_conf.set('bindir', join_paths(get_option('prefix'), get_option('bindir')))
configure_file(
  input: 'org.gnome.PowerStats.service.in',
  output: 'org.gnome.PowerStats.service',
  configuration: service_conf,
  install: true,
  install_dir: join_paths(get_option('datadir'), 'dbus-1', 'services')
)
