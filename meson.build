project ('pipanel','c')

plugin_name = 'rpcc_' + meson.project_name()

share_dir = join_paths(get_option('prefix'), 'share')
presource_dir = join_paths(share_dir, 'rpcc')
pui_dir = join_paths(presource_dir, 'ui')

i18n = import('i18n')

add_project_arguments('-DPACKAGE_LOCALE_DIR="' + share_dir + '/locale"', language : 'c' )

subdir('po')
subdir('src')
subdir('data')
