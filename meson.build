# Copyright © 2018, 2019 Iñigo Martínez
# Copyright © 2019 Christian Persch
#
# This library is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; either version 3 of the License, or (at your
# option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
# General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this library.  If not, see <https://www.gnu.org/licenses/>.

project(
  'bte',
  ['c', 'cpp'],
  version: '0.65.0',
  license: ['LGPL-3.0-or-later', 'GPL-3.0-or-later'],
  default_options: [
    'buildtype=release',
    'c_std=gnu11',
    'cpp_std=gnu++17',
    'warning_level=0',
    'b_ndebug=false',
  ],
  meson_version: '>= 0.50.0',
)

# Requirements

ctk3_req_version          = '3.20.0'
ctk3_min_req_version      = '3.18'
ctk3_max_allowed_version  = '3.20'

fribidi_req_version       = '1.0.0'
gio_req_version           = '2.52.0'
glib_req_version          = '2.67.3'
gnutls_req_version        = '3.2.7'
icu_uc_req_version        = '4.8'
pango_req_version         = '1.22.0'
pcre2_req_version         = '10.21'
systemd_req_version       = '220'

# API

bte_api_major_version = 2
bte_api_minor_version = 91
bte_api_version = '@0@.@1@'.format(bte_api_major_version, bte_api_minor_version)
bte_api_name = 'bte-@0@.@1@'.format(bte_api_major_version, bte_api_minor_version)

bte_ctk3_api_version = '@0@.@1@'.format(bte_api_major_version, bte_api_minor_version)

bte_ctk3_api_name = 'bte-' + bte_ctk3_api_version

bte_ctk3_api_path = bte_ctk3_api_name / 'bte'

# Library versioning

bte_version = meson.project_version()
version_array = bte_version.split('.')
bte_major_version = version_array[0].to_int()
bte_minor_version = version_array[1].to_int()
bte_micro_version = version_array[2].to_int()

libbte_soversion = 0

lt_revision = (bte_minor_version.is_odd() ? 0 : bte_micro_version)
lt_age = bte_minor_version * 100 + bte_micro_version - lt_revision
lt_current = bte_major_version + lt_age

libbte_ctk3_soversion = '@0@.@1@.@2@'.format(libbte_soversion, lt_current, lt_revision)

# i18n

bte_gettext_domain = bte_api_name

# Directories

bte_datadir = get_option('datadir')
bte_includedir = get_option('includedir')
bte_libexecdir = get_option('libexecdir')
bte_localedir = get_option('localedir')
bte_prefix = get_option('prefix')
bte_sysconfdir = get_option('sysconfdir')

# It is correct for this to be in ${prefix}/lib, even on systems where that
# does not match ${libdir}. This is what systemd uses on such platforms.
bte_systemduserunitdir = bte_prefix / 'lib' / 'systemd' / 'user'

# Debug

enable_debug = get_option('debugg') or get_option('debug') or get_option('buildtype').contains('debug')

# Meson modules

gnome = import('gnome')
pkg = import('pkgconfig')

# Compilers

cc = meson.get_compiler('c')
cxx = meson.get_compiler('cpp')

top_inc = include_directories('.')

# Start config.h

config_h = configuration_data()

config_h.set_quoted('GETTEXT_PACKAGE', bte_gettext_domain)
config_h.set_quoted('VERSION', bte_version)
config_h.set('BTE_DEBUG', enable_debug)
config_h.set('WITH_A11Y', get_option('a11y'))
config_h.set('WITH_FRIBIDI', get_option('fribidi'))
config_h.set('WITH_GNUTLS', get_option('gnutls'))
config_h.set('WITH_ICU', get_option('icu'))

if get_option('ctk3')
  ctk3_version_cppflags = []

  ver = ctk3_min_req_version.split('.')
  ctk3_version_cppflags += '-DCDK_VERSION_MIN_REQUIRED=(G_ENCODE_VERSION(' + ver[0] + ',' + ver[1] + '))'

  ver = ctk3_max_allowed_version.split('.')
  ctk3_version_cppflags += '-DCDK_VERSION_MAX_ALLOWED=(G_ENCODE_VERSION(' + ver[0] + ',' + ver[1] + '))'
endif

# FIXME AC_USE_SYSTEM_EXTENSIONS also supported non-gnu systems
config_h.set10('_GNU_SOURCE', true)

# Check headers

check_headers = [
  'locale.h',
  'pty.h',
  'stropts.h',
  'sys/resource.h',
  'sys/select.h',
  'sys/syslimits.h',
  'sys/termios.h',
  'sys/types.h',
  'sys/wait.h',
  'termios.h',
  'util.h',
  'wchar.h',
]

foreach header: check_headers
  config_h.set('HAVE_' + header.underscorify().to_upper(), cxx.has_header(header))
endforeach

# Check for symbols

check_symbols_required = [
  ['TIOCGWINSZ', 'sys/ioctl.h'],
]

foreach symbol: check_symbols_required
  assert(cxx.has_header_symbol(symbol[1], symbol[0]), symbol[0] + ' not found')
endforeach

# Check for functions

check_functions_required = [
  'fork',
  'grantpt',
  'posix_openpt',
  'ptsname',
  'tcgetattr',
  'unlockpt',
]

foreach func: check_functions_required
  assert(cxx.has_function(func), func + ' not found')
endforeach

check_functions = [
  'explicit_bzero',
  'fdwalk',
  'pread',
  'pwrite',
  'strchrnul',
]

foreach func: check_functions
  config_h.set('HAVE_' + func.underscorify().to_upper(), cxx.has_function(func))
endforeach

# Math functions

libm_dep = cxx.find_library('m')

check_math_functions_required = [
  'ceil',
  'floor',
]

foreach func: check_math_functions_required
  assert(cxx.has_function(func, dependencies: libm_dep), func + ' not found')
endforeach

check_math_functions = [
  'round',
]

foreach func: check_math_functions
  config_h.set('HAVE_' + func.underscorify().to_upper(), cxx.has_function(func, dependencies: libm_dep))
endforeach

# Compiler

# Meson has a misfeature where it allows the user to override the -std option
# for the C/C++ compiler. Disallow that.

assert(get_option('c_std') == 'gnu11', 'cannot override C std version')
assert(get_option('cpp_std') == 'gnu++17', 'cannot override C++ std version')

# Meson only checks that -std supports the given string, but *not* that
# the compiler really supports that C++ standard version. Do a simple version
# check based on https://gcc.gnu.org/projects/cxx-status.html#cxx17

if cxx.get_id() == 'gcc'
  assert(cxx.version().version_compare('>= 7.0'), 'needs G++ >= 7 for C++17 support')
endif

# Asserts must not be disabled

assert(get_option('b_ndebug') == 'false', 'assertions may not be disabled')

# LTO very much NOT supported

assert(get_option('b_lto') == false, 'LTO not supported')

# Compiler flags

compiler_flags_common = [
  '-Wall',
  '-Wextra',
  '-Wcast-align',
  '-Wcast-function-type',
  '-Wclobbered',
  '-Wempty-body',
  '-Wendif-labels',
  '-Werror=init-self',
  '-Werror=missing-include-dirs',
  '-Werror=pointer-arith',
  '-Wfloat-equal',
  '-Wignored-qualifiers',
  '-Winvalid-pch',
  '-Wlogical-op',
  '-Wmisleading-indentation',
  '-Wmissing-declarations',
  '-Wmissing-field-initializers',
  '-Wmissing-format-attribute',
  '-Wmissing-include-dirs',
  '-Wmissing-noreturn',
  '-Wno-address-of-packed-member',
  '-Wno-missing-field-initializers',
  '-Wno-packed',
  '-Wno-switch-enum',
  '-Wno-unused-parameter',
  '-Wshadow',
  '-Wshift-negative-value',
  '-Wsign-compare',
  '-Wstrict-aliasing=2',
  '-Wtype-limits',
  '-Wundef',
  '-Wuninitialized',
  '-Wuninitialized',
  '-Wunsafe-loop-optimizations',
  '-Wunused',
  '-Wunused-but-set-parameter',
  '-Wunused-but-set-variable',
  '-Wunused-function',
  '-Wunused-label',
  '-Wunused-local-typedefs',
  '-Wunused-value',
  '-Wunused-variable',
  '-Wvla',
  '-Wwrite-strings',
  '-fdiagnostics-show-option',
  '-fno-common',
  '-fno-semantic-interposition',
  '-fstack-protector',
  '-fstack-protector-strong',
]

if enable_debug
  compiler_flags_common += [
    '-ggdb3',
  ]
endif

# These are currently needed but the code should be fixed instead
compiler_flags_common_undesirable = [
  '-fno-strict-aliasing'
]

compiler_flags_c_only = [
  '-Waggregate-return',
  '-Werror=implicit-function-declaration',
  '-Werror=missing-prototypes',
  '-Wimplicit',
  '-Wimplicit-fallthrough=3',
  '-Wmissing-parameter-type',
  '-Wnested-externs',
  '-Wold-style-declaration',
  '-Wold-style-definition',
  '-Woverride-init',
  '-Wsign-compare',
  '-Wstrict-prototypes',
]

compiler_flags_cxx_only = [
  '-Wimplicit-fallthrough=5',
  '-Wnon-virtual-dtor',
  '-Wstrict-null-sentinel',
]

compiler_flags_cxx_required = [
  '-fvisibility-inlines-hidden',
  '-fvisibility=hidden',
]

global_cflags = cc.get_supported_arguments(compiler_flags_common +
                                           compiler_flags_common_undesirable +
                                           compiler_flags_c_only)
global_cxxflags = cxx.get_supported_arguments(compiler_flags_common +
                                              compiler_flags_common_undesirable +
                                              compiler_flags_cxx_only +
                                              compiler_flags_cxx_required)

foreach flag: compiler_flags_cxx_required
  assert(cxx.has_argument(flag), flag + ' is required but not supported')
endforeach


# These flags have to be tested together

compiler_flags_common_multi = [
  # These only work together with -Wformat
  [
    '-Werror=format=2',
    '-Werror=format-nonliteral',
    '-Werror=format-security',
  ],
]

foreach flags : compiler_flags_common_multi
  if cc.has_multi_arguments(flags)
    global_cflags += flags
  endif
  if cxx.has_multi_arguments(flags)
    global_cxxflags += flags
  endif
endforeach

# ... and now make these flags the default

add_project_arguments(global_cflags, language: 'c')
add_project_arguments(global_cxxflags, language: 'cpp')

# Linker flags

linker_flags = [
  ['-Wl,-Bsymbolic', false,],
  ['-Wl,-Bsymbolic-functions', get_option('_b_symbolic_functions'),],
]

foreach flag: linker_flags
  if cc.has_link_argument(flag[0])
    add_project_link_arguments(flag[0], language: 'c')
  elif flag[1]
    assert(false, flag[0] + ' is required but not supported')
  endif

  if cxx.has_link_argument(flag[0])
    add_project_link_arguments(flag[0], language: 'cpp')
  elif flag[1]
    assert(false, flag[0] + ' is required but not supported')
  endif
endforeach

# Dependencies

gio_dep      = dependency('gio-2.0',    version: '>=' + gio_req_version)
glib_dep     = dependency('glib-2.0',   version: '>=' + glib_req_version)
gobject_dep  = dependency('gobject-2.0')
pango_dep    = dependency('pango',      version: '>=' + pango_req_version)
pcre2_dep    = dependency('libpcre2-8', version: '>=' + pcre2_req_version)
pthreads_dep = dependency('threads')
zlib_dep     = dependency('zlib')

if get_option('fribidi')
  fribidi_dep = dependency('fribidi', version: '>=' + fribidi_req_version)
else
  fribidi_dep = dependency('', required: false)
endif

if get_option('gnutls')
  gnutls_dep = dependency('gnutls', version: '>=' + gnutls_req_version)
else
  gnutls_dep = dependency('', required: false)
endif

if get_option('ctk3')
  ctk3_dep = dependency('ctk+-3.0', version: '>=' + ctk3_req_version)
else
  ctk3_dep = dependency('', required: false)
endif

if get_option('icu')
  icu_dep = dependency('icu-uc', version: '>=' + icu_uc_req_version)
else
  icu_dep = dependency('', required: false)
endif

if host_machine.system() == 'linux' and get_option('_systemd')
  systemd_dep = dependency('libsystemd', version: '>=' + systemd_req_version)
else
  systemd_dep = dependency('', required: false)
endif

config_h.set('WITH_SYSTEMD', systemd_dep.found())

# Write config.h

configure_file(
  output: 'config.h',
  configuration: config_h,
)

# Subdirs

subdir('src')
subdir('bindings')
subdir('po')

if get_option('docs')
  subdir('doc/reference')
endif

# Simple compat Makefile

makefile_conf = configuration_data()
makefile_conf.set('srcdir', meson.current_source_dir())
makefile_conf.set('builddir', meson.current_build_dir())
makefile_conf.set('bte_ctk3_api_version', bte_ctk3_api_version)

configure_file(
  input: 'Makefile.meson',
  output: '@BASENAME@',
  configuration: makefile_conf,
)

# .gitignore everything in the build directory

configure_file(
  output: '.gitignore',
  command: ['echo', '**/**',],
  capture: true,
  install: false,
)

# Summary

output = '\n'
output += 'Configuration for BTE:\n\n'
output += '  Version:      ' + bte_version + '\n'
output += '\n'
output += '  C compiler:   ' + cc.get_id() + ' (version ' + cc.version() + ')\n'
output += '  C++ compiler: ' + cxx.get_id() + ' (version ' + cxx.version() + ')\n'
output += '\n'
output += '  Coverage:     ' + get_option('b_coverage').to_string() + '\n'
output += '  Debug:        ' + enable_debug.to_string() + '\n'
output += '\n'
output += '  A11y:         ' + get_option('a11y').to_string() + '\n'
output += '  Docs:         ' + get_option('docs').to_string() + '\n'
output += '  FRIBIDI:      ' + get_option('fribidi').to_string() + '\n'
output += '  GNUTLS:       ' + get_option('gnutls').to_string() + '\n'
output += '  CTK+ 3.0:     ' + get_option('ctk3').to_string() + '\n'
output += '  ICU:          ' + get_option('icu').to_string() + '\n'
output += '  GIR:          ' + get_option('gir').to_string() + '\n'
output += '  systemd:      ' + systemd_dep.found().to_string() + '\n'
output += '\n'
output += '  Prefix:       ' + get_option('prefix') + '\n'
message(output)

# Done
