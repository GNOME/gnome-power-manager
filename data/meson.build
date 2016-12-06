subdir('appdata')
subdir('icons')

compiled = gnome.compile_schemas()
install_data('org.gnome.power-manager.gschema.xml',
  install_dir : 'share/glib-2.0/schemas'
)

# replace @VERSION@
versionedfile = configure_file(
  input : 'org.gnome.PowerStats.desktop.in.in',
  output : 'org.gnome.PowerStats.desktop.in',
  configuration : conf
)
i18n.merge_file(
  input: versionedfile,
  output: 'org.gnome.PowerStats.desktop',
  type: 'desktop',
  po_dir: join_paths(meson.source_root(), 'po'),
  install: true,
  install_dir: join_paths(get_option('datadir'), 'applications')
)
