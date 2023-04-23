project('instrew-rerunner', ['c'],
        default_options: [
            'buildtype=debugoptimized',
            'default_library=static',
            'warning_level=3',
            'c_std=c17'
        ])

add_project_arguments(['-Wmissing-field-initializers',
                       '-Wunused-parameter',
                       '-Wshadow',
                       '-Wpointer-arith',
                       '-Wwrite-strings',
                       '-Winline',
                       '-Wformat-nonliteral',
                       '-Wformat-security',
                       '-Wswitch-default',
                       '-Wundef',
                       '-Wno-cast-align', # we frequently do casts
                       '-Werror=incompatible-pointer-types',
                       '-Werror=implicit-function-declaration'],
                      language: 'c')


sources = [
    'dispatch.c',
    'elf-loader.c',
    'emulate.c',
    'main.c',
    'math.c',
    'memory.c',
    'minilib.c',
    'rtld.c',
]

if host_machine.cpu_family() == 'aarch64'
  sources += [
    'memset-aarch64.S'
  ]
endif

c_args = ['-D_GNU_SOURCE', '-fno-builtin', '-fno-stack-protector', '-fomit-frame-pointer', '-fPIC']

link_args = ['-nostartfiles', '-lgcc']

compiler = meson.get_compiler('c')
if compiler.has_argument('-static-pie')
    link_args += ['-static-pie']
else
    link_args += ['-Wl,-static', '-Wl,-pie', '-Wl,--no-dynamic-linker', '-Wl,-z,text']
endif

if host_machine.cpu_family() == 'x86_64'
  if cc.has_argument('-mpreferred-stack-boundary=3') # GCC
    client_c_args += ['-mpreferred-stack-boundary=3']
  elif cc.has_argument('-mstack-alignment=3') # Clang
    client_c_args += ['-mstack-alignment=3']
  else
    error('could not lower stack aligment')
  endif
endif

runner = executable('instrew-rerunner',
                    sources,
                    include_directories: include_directories('.'),
                    c_args: c_args,
                    link_args: link_args,
                    install: true)
