project('wealth-trace', ['cpp', 'c'],
  version: '1.0.0',
  license: 'MIT',
  meson_version: '>=1.3.0',
  default_options: [
    'cpp_std=c++23',
    'c_std=c11',
    'warning_level=3',
    'buildtype=debugoptimized',
  ])

# ---------------------------------------------------------------------------
# Vendored third-party dependencies (see subprojects/).
# They are vendored so the project builds offline without system -dev packages.
# ---------------------------------------------------------------------------
sqlite3_proj = subproject('sqlite3')
sqlite3_dep = sqlite3_proj.get_variable('sqlite3_dep')

nlohmann_dep = subproject('nlohmann_json').get_variable('nlohmann_dep')
asio_dep = subproject('asio').get_variable('asio_dep')
crow_dep = subproject('crow').get_variable('crow_dep')

thread_dep = dependency('threads')

subdir('backend')
