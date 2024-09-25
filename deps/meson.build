pkg = import('pkgconfig')

inc = include_directories(
  './logging/liblog/include',
  './libbase/include',
  './core/include',
  './core/libcutils/include',
  './art/libartpalette/include',
  './art/libartbase',
  './libnativehelper/include',
  './libnativehelper/include_jni',
  './libnativehelper/include_platform',
)

subdir('logging')
subdir('core')
subdir('libbase')
subdir('libnativehelper')
subdir('art')

liblog = library(
  'log',
  log_files,
  include_directories: inc,
  install: true,
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
  install: true,
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
  install: true,
)

libnativehelper = library(
  'nativehelper',
  nativehelper_files,
  include_directories: inc,
  dependencies: [liblog_dep],
  c_args: ['-D_DEFAULT_SOURCE'],
  install: true,
)

libartpalette = library(
  'artpalette',
  artpalette_files,
  include_directories: inc,
  dependencies: [libbase_dep, liblog_dep],
  install: true,
)
libartpalette_dep = declare_dependency(
  link_with: [libartpalette],
  include_directories: inc,
)

pkg.generate(liblog)
pkg.generate(libbase)
pkg.generate(libcutils)
pkg.generate(libartpalette)

