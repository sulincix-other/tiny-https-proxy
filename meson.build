project('proxy', 'c')

cc = meson.get_compiler('c')
deps = []
sources = ['proxy.c', 'utils.c']
if host_machine.system() == 'windows'
    deps += cc.find_library('ws2_32')
    sources += 'socket_win.c'
else
    deps += dependency('threads')
    sources += 'socket.c'
endif

executable('proxy', sources, dependencies: deps, install: true)
