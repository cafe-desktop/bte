/*
 * Copyright (C) 2002 Red Hat, Inc.
 * Copyright © 2018, 2019 Christian Persch
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

/* The interfaces in this file are subject to change at any time. */

#pragma once

#ifdef __clang__
/* Clang generates lots of warnings for the code below. However while
 * the pointer in the BteCellAttr struct is indeed only 4-byte aligned,
 * the BteCellAttr is only used when a member of a BteCell, where it is
 * at offset 4, resulting in a sufficient overall alignment. So we can
 * safely ignore the warning.
 */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Waddress-of-packed-member"
#endif /* __clang__ */

#include <string.h>

#include "bteunistr.h"
#include "btemacros.h"
#include "btedefines.hh"

#include "attr.hh"
#include "color-triple.hh"

#define BTE_TAB_WIDTH_MAX		((1 << BTE_ATTR_COLUMNS_BITS) - 1)

#define BTE_CELL_ATTR_COMMON_BYTES      12  /* The number of common bytes in BteCellAttr and BteStreamCellAttr */

/*
 * BteCellAttr: A single cell style attributes
 *
 * When adding new attributes, keep in sync with BteStreamCellAttr and
 * update BTE_CELL_ATTR_COMMON_BYTES accordingly.
 * Also don't forget to update basic_cell below!
 */

#define CELL_ATTR_BOOL(lname,uname) \
        inline void set_##lname(bool value) \
        { \
                bte_attr_set_bool(&attr, BTE_ATTR_##uname##_MASK, value); \
        } \
        \
        inline constexpr bool lname() const \
        { \
                return bte_attr_get_bool(attr, BTE_ATTR_##uname##_SHIFT); \
        }

#define CELL_ATTR_UINT(lname,uname) \
        inline void set_##lname(unsigned int value) \
        { \
                bte_attr_set_value(&attr, BTE_ATTR_##uname##_MASK, BTE_ATTR_##uname##_SHIFT, value); \
        } \
        \
        inline constexpr uint32_t lname() const \
        { \
                return bte_attr_get_value(attr, BTE_ATTR_##uname##_VALUE_MASK, BTE_ATTR_##uname##_SHIFT); \
        }

typedef struct _BTE_GNUC_PACKED BteCellAttr {
        uint32_t attr;

	/* 4-byte boundary (8-byte boundary in BteCell) */
        uint64_t m_colors;                     /* fore, back and deco (underline) colour */

        /* 12-byte boundary (16-byte boundary in BteCell) */
        uint32_t hyperlink_idx; /* a unique hyperlink index at a time for the ring's cells,
                                   0 means not a hyperlink, BTE_HYPERLINK_IDX_TARGET_IN_STREAM
                                   means the target is irrelevant/unknown at the moment.
                                   If bitpacking, choose a size big enough to hold a different idx
                                   for every cell in the ring but not yet in the stream
                                   (currently the height rounded up to the next power of two, times width)
                                   for supported BTE sizes, and update BTE_HYPERLINK_IDX_TARGET_IN_STREAM. */

        /* Methods */

        inline constexpr uint64_t colors() const { return m_colors; }

        inline void copy_colors(BteCellAttr const& other)
        {
                m_colors = bte_color_triple_copy(other.colors());
        }

#define CELL_ATTR_COLOR(name) \
        inline void set_##name(uint32_t value) \
        { \
                bte_color_triple_set_##name(&m_colors, value); \
        } \
        \
        inline constexpr uint32_t name() const \
        { \
                return bte_color_triple_get_##name(m_colors); \
        }

        CELL_ATTR_COLOR(fore)
        CELL_ATTR_COLOR(back)
        CELL_ATTR_COLOR(deco)
#undef CELL_ATTR_COLOR

        inline constexpr bool has_any(uint32_t mask) const
        {
                return !!(attr & mask);
        }

        inline constexpr bool has_all(uint32_t mask) const
        {
                return (attr & mask) == mask;
        }

        inline constexpr bool has_none(uint32_t mask) const
        {
                return !(attr & mask);
        }

        inline void unset(uint32_t mask)
        {
                attr &= ~mask;
        }

        CELL_ATTR_UINT(columns, COLUMNS)
        CELL_ATTR_BOOL(fragment, FRAGMENT)
        CELL_ATTR_BOOL(bold, BOLD)
        CELL_ATTR_BOOL(italic, ITALIC)
        CELL_ATTR_UINT(underline, UNDERLINE)
        CELL_ATTR_BOOL(strikethrough, STRIKETHROUGH)
        CELL_ATTR_BOOL(overline, OVERLINE)
        CELL_ATTR_BOOL(reverse, REVERSE)
        CELL_ATTR_BOOL(blink, BLINK)
        CELL_ATTR_BOOL(dim, DIM)
        CELL_ATTR_BOOL(invisible, INVISIBLE)
        /* ATTR_BOOL(boxed, BOXED) */
} BteCellAttr;
static_assert(sizeof (BteCellAttr) == 16, "BteCellAttr has wrong size");
static_assert(offsetof (BteCellAttr, hyperlink_idx) == BTE_CELL_ATTR_COMMON_BYTES, "BteCellAttr layout is wrong");

/*
 * BteStreamCellAttr: Variant of BteCellAttr to be stored in attr_stream.
 *
 * When adding new attributes, keep in sync with BteCellAttr and
 * update BTE_CELL_ATTR_COMMON_BYTES accordingly.
 */

typedef struct _BTE_GNUC_PACKED _BteStreamCellAttr {
        uint32_t attr; /* Same as BteCellAttr. We only access columns
                        * and fragment, however.
                        */
        /* 4-byte boundary */
        uint64_t colors;
        /* 12-byte boundary */
        guint16 hyperlink_length;       /* make sure it fits BTE_HYPERLINK_TOTAL_LENGTH_MAX */

        /* Methods */
        CELL_ATTR_UINT(columns, COLUMNS)
        CELL_ATTR_BOOL(fragment, FRAGMENT)
} BteStreamCellAttr;
static_assert(sizeof (BteStreamCellAttr) == 14, "BteStreamCellAttr has wrong size");
static_assert(offsetof (BteStreamCellAttr, hyperlink_length) == BTE_CELL_ATTR_COMMON_BYTES, "BteStreamCellAttr layout is wrong");

#undef CELL_ATTR_BOOL
#undef CELL_ATTR_UINT

/*
 * BteCell: A single cell's data
 */

typedef struct _BTE_GNUC_PACKED _BteCell {
	bteunistr c;
	BteCellAttr attr;
} BteCell;

static_assert(sizeof (BteCell) == 20, "BteCell has wrong size");

static const BteCell basic_cell = {
	0,
	{
                BTE_ATTR_DEFAULT, /* attr */
                BTE_COLOR_TRIPLE_INIT_DEFAULT, /* colors */
                0, /* hyperlink_idx */
	}
};

#ifdef __clang__
#pragma clang diagnostic pop
#endif
