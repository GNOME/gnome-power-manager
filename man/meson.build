docbook2man = find_program('docbook2man', required : false)
if docbook2man.found()
  custom_target('gnome-power-statistics-man',
    output : 'gnome-power-statistics.1',
    input : 'gnome-power-statistics.sgml',
    command : [docbook2man, '@INPUT@', '--output', 'man'],
    install : true,
    install_dir : 'share/man/man1'
  )
endif
