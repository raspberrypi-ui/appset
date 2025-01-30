sources = files (
    'pipanel.c',
    'desktop.c',
    'taskbar.c',
    'system.c',
    'defaults.c'
)

add_global_arguments('-Wno-unused-result', language : 'c')

gtk = dependency ('gtk+-3.0')
xml = dependency ('libxml-2.0')
deps = [ gtk, xml ]

if meson.project_name().contains('rpcc')
  shared_module(meson.project_name(), sources, dependencies: deps, install: true,
        install_dir: get_option('libdir') / 'rpcc',
        c_args : '-DPLUGIN_NAME="' + meson.project_name() + '"',
        link_args : '-Wl,-z,defs'
  )
else
  executable (meson.project_name(), sources, dependencies: deps, install: true)
endif
