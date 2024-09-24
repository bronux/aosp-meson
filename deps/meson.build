inc = include_directories(
  './logging/liblog/include',
  './libbase/include',
  './core/include',
  './core/libcutils/include',
)

subdir('logging')
subdir('core')
subdir('libbase')

liblog = library(
  'log',
  log_files,
  include_directories: inc,
)
liblog_dep = declare_dependency(
  link_with: [liblog],
  include_directories: inc,
)

libbase = library(
  'base',
  base_files,
  include_directories: inc,
  cpp_args: ['-D_DEFAULT_SOURCE'],
  dependencies: [liblog_dep],
)
libbase_dep = declare_dependency(
  link_with: [libbase],
  include_directories: inc,
)

libcutils = library(
  'cutils',
  cutils_files,
  include_directories: inc,
  cpp_args: ['-DPROP_NAME_MAX=32'],
  dependencies: [libbase_dep, liblog_dep],
)
