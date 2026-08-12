# Copyright © 2018, 2019 Iñigo Martínez
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

# Meson insufficiency! Would be so much easier to just make this
# (bte_ctk3_sources - bte_ctk3_public_headers).filter('.h'), but
# but there is no array subtraction or filtering. Or just allow listing the
# headers that we DO want to get scanned.
# So instead just list all the headers here again... :-(

private_headers = [
  'buffer.h',
  'caps.hh',
  'cell.hh',
  'config.h',
  'debug.h',
  'keymap.h',
  'marshal.h',
  'modes.hh',
  'modes-ecma.hh',
  'modes-private.hh',
  'parser.hh',
  'parser-arg.hh',
  'parser-c01.hh',
  'parser-charset.hh',
  'parser-charset-tables.hh',
  'parser-cmd.hh',
  'parser-csi.hh',
  'parser-dcs.hh',
  'parser-esc.hh',
  'parser-glue.hh',
  'parser-osc.hh',
  'parser-reply.hh',
  'parser-string.hh',
  'ring.hh',
  'tabstops.hh',
  'bteconv.h',
  'btedraw.h',
  'bteinternal.hh',
  'bterowdata.hh',
  'btestream-base.h',
  'btestream-file.h',
  'btestream.h',
  'btetypebuiltins.h',
  'bteunistr.h',
]

scan_args = [
  '--deprecated-guards="BTE_DISABLE_DEPRECATED"',
  '--ignore-decorators=_BTE_GNUC_NONNULL\s*\([^)]*\)|_BTE_CXX_NOEXCEPT',
]

glib_prefix = glib_dep.get_pkgconfig_variable('prefix')

version_conf = configuration_data()
version_conf.set('VERSION', bte_version)

content_files = configure_file(
  input: 'version.xml.in',
  output: '@BASENAME@',
  configuration: version_conf
)

if get_option('ctk3')
  ctk3_prefix = ctk3_dep.get_pkgconfig_variable('prefix')

  fixxref_args = [
    '--html-dir=' + (bte_prefix / gnome.gtkdoc_html_dir(bte_ctk3_api_name)),
    '--extra-dir=' + (glib_prefix / gnome.gtkdoc_html_dir('glib')),
    '--extra-dir=' + (glib_prefix / gnome.gtkdoc_html_dir('gio')),
    '--extra-dir=' + (ctk3_prefix / gnome.gtkdoc_html_dir('cdk')),
    '--extra-dir=' + (ctk3_prefix / gnome.gtkdoc_html_dir('cdk-pixbuf')),
    '--extra-dir=' + (ctk3_prefix / gnome.gtkdoc_html_dir('ctk')),
  ]

  gnome.gtkdoc(
    'bte',
    main_xml: 'bte-docs.xml',
    module_version: bte_api_version,
    src_dir: [src_inc, bte_inc],
    ignore_headers: private_headers,
    include_directories: top_inc,
    dependencies: libbte_ctk3_dep,
    c_args: '-DBTE_COMPILATION',
    namespace: 'bte',
    scan_args: scan_args,
    mkdb_args: '--source-suffixes=h,hh,c,cc',
    fixxref_args: fixxref_args,
    gobject_typesfile: 'bte.types',
    content_files: content_files,
    install: true,
  )
endif
