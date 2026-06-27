project('proxy', 'c')

cc = meson.get_compiler('c')
deps = []
sources = ['src/utils.c', 'src/proxy.c']
incdir = [include_directories('src')]
if host_machine.system() == 'windows'
    deps += cc.find_library('ws2_32')
    sources += ['src/win32/socket.c', 'src/win32/server.c']
    incdir += include_directories('src/win32')
else
    sources += ['src/unix/socket.c', 'src/unix/server.c']
    incdir += include_directories('src/unix')
endif

executable('proxy', ['src/main.c'] + sources,
    dependencies: deps,
    include_directories: incdir,
    install: true)
