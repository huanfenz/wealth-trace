gtest_dep = subproject('googletest').get_variable('gtest_dep')

test_sources = files(
  'smoke_test.cpp',
  'utils_test.cpp',
  'database_test.cpp',
  'service_test.cpp',
)

test_exe = executable('wealth_trace_tests',
  test_sources,
  dependencies: [core_dep, gtest_dep],
  cpp_args: ['-DWT_TEST_SOURCE_DIR="' + meson.project_source_root() + '"'],
  install: false)

test('wealth_trace_tests', test_exe,
  workdir: meson.current_build_dir(),
  timeout: 120)
