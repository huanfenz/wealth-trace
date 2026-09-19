project('crow', 'cpp')

asio_dep = subproject('asio').get_variable('asio_dep')

crow_dep = declare_dependency(
  include_directories: include_directories('include', is_system: true),
  dependencies: asio_dep)
