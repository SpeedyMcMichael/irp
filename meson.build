project('irp', 'c',
  version : '0.1.0',
  default_options : [
    'c_std=c99',
    'warning_level=2',
    'optimization=2',
    'b_ndebug=if-release',
  ],
  meson_version : '>=0.56.0',
)

cc = meson.get_compiler('c')

# ncurses: try pkg-config, fall back to -lncurses like a savage
ncurses_dep = dependency('ncursesw', required : false)
if not ncurses_dep.found()
  ncurses_dep = dependency('ncurses', required : false)
endif
if not ncurses_dep.found()
  ncurses_dep = cc.find_library('ncurses', required : true)
endif

add_project_arguments(
  '-D_POSIX_C_SOURCE=200809L',
  language : 'c',
)

# config.h lives in project root; let the compiler find it
inc = include_directories('.')

executable('irp',
  'src/irp.c',
  include_directories : inc,
  dependencies : [ncurses_dep],
  install : true,
)

# man page if you ever write one
# install_man('irp.1')
