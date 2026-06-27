project('proxy', 'c')

cc = meson.get_compiler('c')
deps = []
sources = ['src/main.c', 'src/utils.c']
incdir = [include_directories('src')]
if host_machine.system() == 'windows'
    deps += cc.find_library('ws2_32')
    sources += 'src/win32/socket.c'
    incdir += include_directories('src/win32')
else
    sources += 'src/unix/socket.c'
    incdir += include_directories('src/unix')
endif

executable('proxy', sources, dependencies: deps, include_directories: incdir, install: true)
