project('googletest', 'cpp')

gtest_inc = include_directories('googletest/include', is_system: true)
gtest_root = include_directories('googletest')
gtest_threads = dependency('threads')

gtest_lib = static_library('gtest',
  'googletest/src/gtest-all.cc',
  include_directories: [gtest_inc, gtest_root],
  dependencies: gtest_threads,
  cpp_args: ['-w'],
  install: false)

gtest_main_lib = static_library('gtest_main',
  'googletest/src/gtest_main.cc',
  include_directories: [gtest_inc, gtest_root],
  dependencies: gtest_threads,
  cpp_args: ['-w'],
  install: false)

gtest_dep = declare_dependency(
  include_directories: gtest_inc,
  link_with: [gtest_main_lib, gtest_lib],
  dependencies: gtest_threads)
