/*
 * Copyright © 2014 Christian Persch
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __BTE_BTE_MACROS_H__
#define __BTE_BTE_MACROS_H__

#if !defined (__BTE_BTE_H_INSIDE__) && !defined (BTE_COMPILATION)
#error "Only <bte/bte.h> can be included directly."
#endif

#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 6)
#define _BTE_GNUC_PACKED __attribute__((__packed__))
#else
#define _BTE_GNUC_PACKED
#endif  /* !__GNUC__ */

#ifdef BTE_COMPILATION
#define _BTE_GNUC_NONNULL(position)
#else
#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ > 2)
#define _BTE_GNUC_NONNULL(position) __attribute__((__nonnull__(position)))
#else
#define _BTE_GNUC_NONNULL(position)
#endif
#endif

#define _BTE_PUBLIC __attribute__((__visibility__("default"))) extern

#if defined(BTE_COMPILATION) && defined(__cplusplus)
#if __cplusplus >= 201103L
#define _BTE_CXX_NOEXCEPT noexcept
#endif
#endif
#ifndef _BTE_CXX_NOEXCEPT
#define _BTE_CXX_NOEXCEPT
#endif

#endif /* __BTE_BTE_MACROS_H__ */
