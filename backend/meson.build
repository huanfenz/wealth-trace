inc = include_directories('include')

core_sources = files(
  'src/common/logging.cpp',
  'src/common/response.cpp',
  'src/config/config.cpp',
  'src/utils/money.cpp',
  'src/utils/rate.cpp',
  'src/utils/time_util.cpp',
  'src/utils/term_date.cpp',
  'src/utils/maintenance_rules.cpp',
  'src/utils/strings.cpp',
  'src/database/database.cpp',
  'src/database/statement.cpp',
  'src/database/transaction.cpp',
  'src/database/migration.cpp',
  'src/model/enums.cpp',
  'src/repository/household_repository.cpp',
  'src/repository/member_repository.cpp',
  'src/repository/account_repository.cpp',
  'src/repository/asset_repository.cpp',
  'src/repository/transaction_repository.cpp',
  'src/repository/statistics_repository.cpp',
  'src/repository/system_state_repository.cpp',
  'src/service/household_service.cpp',
  'src/service/member_service.cpp',
  'src/service/account_service.cpp',
  'src/service/asset_service.cpp',
  'src/service/transaction_service.cpp',
  'src/service/statistics_service.cpp',
  'src/service/daily_maintenance_service.cpp',
  'src/dto/json_helpers.cpp',
  'src/dto/serialization.cpp',
  'src/controller/household_controller.cpp',
  'src/controller/member_controller.cpp',
  'src/controller/account_controller.cpp',
  'src/controller/asset_controller.cpp',
  'src/controller/transaction_controller.cpp',
  'src/controller/statistics_controller.cpp',
  'src/controller/meta_controller.cpp',
  'src/controller/static_file_controller.cpp',
  'src/controller/maintenance_controller.cpp',
)

core_deps = [sqlite3_dep, crow_dep, asio_dep, nlohmann_dep, thread_dep]

core_lib = static_library('wealth_trace_core',
  core_sources,
  include_directories: inc,
  dependencies: core_deps,
  pic: true,
  install: false)

core_dep = declare_dependency(
  link_with: core_lib,
  include_directories: inc,
  dependencies: core_deps)

executable('wealth-trace',
  'src/main.cpp',
  dependencies: core_dep,
  install: false)

if get_option('enable_tests')
  subdir('test')
endif
