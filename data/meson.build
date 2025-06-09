if build_standalone
  install_data('pipanel.ui', install_dir: ui_dir)
  i18n.merge_file(input: 'pipanel.desktop.in',
    output: 'pipanel.desktop',
    type: 'desktop',
    po_dir: '../po',
    install: true,
    install_dir: desktop_dir
  )
endif

if build_plugin
  install_data('pipanel.ui', install_dir: pui_dir)
  install_subdir('icons', install_dir: share_dir)
  gnome = import ('gnome')
  gnome.post_install (gtk_update_icon_cache : true)
endif
