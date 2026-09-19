project('sqlite3', 'c')

sqlite3_lib = static_library('sqlite3',
  'sqlite3.c',
  c_args: [
    '-w',
    '-DSQLITE_THREADSAFE=1',
    '-DSQLITE_DEFAULT_FOREIGN_KEYS=1',
    '-DSQLITE_ENABLE_JSON1',
    '-DSQLITE_OMIT_LOAD_EXTENSION',
  ],
  install: false)

sqlite3_dep = declare_dependency(
  link_with: sqlite3_lib,
  include_directories: include_directories('.', is_system: true))
