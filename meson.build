project('proxy', 'c')

cc = meson.get_compiler('c')
deps = []
if host_machine.system() == 'windows'
    deps += cc.find_library('ws2_32')
endif

executable('proxy', ['proxy.c', 'utils.c'], dependencies: deps, install: true)
