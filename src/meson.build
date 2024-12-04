name = 'rpcc_appset'

sources = files (
    'pipanel.c'
)

add_global_arguments('-Wno-unused-result', language : 'c')

gtk = dependency ('gtk+-3.0')
xml = dependency ('libxml-2.0')
deps = [ gtk, xml ]

shared_module(name, sources,
        dependencies: deps,
        install: true,
        install_dir: get_option('libdir') / 'rpcc',
        c_args : '-DPLUGIN_NAME="' + name + '"'
)
