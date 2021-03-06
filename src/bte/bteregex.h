/*
 * Copyright © 2015 Christian Persch
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __BTE_BTE_REGEX_H__
#define __BTE_BTE_REGEX_H__

#if !defined (__BTE_BTE_H_INSIDE__) && !defined (BTE_COMPILATION)
#error "Only <bte/bte.h> can be included directly."
#endif

#include <glib.h>
#include <glib-object.h>

#include "btemacros.h"

G_BEGIN_DECLS

typedef struct _BteRegex BteRegex;

#define BTE_TYPE_REGEX (bte_regex_get_type())

_BTE_PUBLIC
GType bte_regex_get_type (void);

#define BTE_REGEX_ERROR (bte_regex_error_quark())

_BTE_PUBLIC
GQuark bte_regex_error_quark (void);

/* This is PCRE2_NO_UTF_CHECK | PCRE2_UTF | PCRE2_NEVER_BACKSLASH_C */
#define BTE_REGEX_FLAGS_DEFAULT (0x00080000u | 0x40000000u | 0x00100000u)

_BTE_PUBLIC
BteRegex *bte_regex_ref      (BteRegex *regex) _BTE_CXX_NOEXCEPT _BTE_GNUC_NONNULL(1);

_BTE_PUBLIC
BteRegex *bte_regex_unref    (BteRegex *regex) _BTE_CXX_NOEXCEPT _BTE_GNUC_NONNULL(1);

_BTE_PUBLIC
BteRegex *bte_regex_new_for_match (const char *pattern,
                                   gssize      pattern_length,
                                   guint32     flags,
                                   GError    **error) _BTE_CXX_NOEXCEPT _BTE_GNUC_NONNULL(1);

_BTE_PUBLIC
BteRegex *bte_regex_new_for_search (const char *pattern,
                                    gssize      pattern_length,
                                    guint32     flags,
                                    GError    **error) _BTE_CXX_NOEXCEPT _BTE_GNUC_NONNULL(1);

_BTE_PUBLIC
gboolean  bte_regex_jit     (BteRegex *regex,
                             guint32   flags,
                             GError  **error) _BTE_CXX_NOEXCEPT _BTE_GNUC_NONNULL(1);

_BTE_PUBLIC
char *bte_regex_substitute(BteRegex *regex,
                           const char *subject,
                           const char *replacement,
                           guint32 flags,
                           GError **error) _BTE_CXX_NOEXCEPT _BTE_GNUC_NONNULL(1) _BTE_GNUC_NONNULL(2) _BTE_GNUC_NONNULL(3) G_GNUC_MALLOC;

G_DEFINE_AUTOPTR_CLEANUP_FUNC(BteRegex, bte_regex_unref)

G_END_DECLS

#endif /* __BTE_BTE_REGEX_H__ */
