/*
 * Copyright © 2018–2019 Egmont Koblinger
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

#pragma once

#include <glib.h>

#include "ring.hh"
#include "ringview.hh"
#include "bterowdata.hh"
#include "btetypes.hh"
#include "bteunistr.h"

namespace bte {

namespace base {

class RingView;

/* BidiRow contains the BiDi transformation of a single row. */
class BidiRow {
        friend class BidiRunner;

public:
        BidiRow() { }
        ~BidiRow();

        // prevent accidents
        BidiRow(BidiRow& o) = delete;
        BidiRow(BidiRow const& o) = delete;
        BidiRow(BidiRow&& o) = delete;
        BidiRow& operator= (BidiRow& o) = delete;
        BidiRow& operator= (BidiRow const& o) = delete;
        BidiRow& operator= (BidiRow&& o) = delete;

        bte::grid::column_t log2vis(bte::grid::column_t col) const;
        bte::grid::column_t vis2log(bte::grid::column_t col) const;
        bool log_is_rtl(bte::grid::column_t col) const;
        bool vis_is_rtl(bte::grid::column_t col) const;
        bteunistr vis_get_shaped_char(bte::grid::column_t col, bteunistr s) const;

        /* Whether the line's base direction is RTL. */
        inline constexpr bool base_is_rtl() const noexcept { return m_base_rtl; }

        /* Whether the implicit paragraph contains a foreign directionality character.
         * This is used in the cursor, showing the character's directionality. */
        inline constexpr bool has_foreign() const noexcept { return m_has_foreign; }

private:
        void set_width(bte::grid::column_t width);

        /* The value of m_width == 0 is a valid representation of the trivial LTR mapping. */
        uint16_t m_width{0};
        uint16_t m_width_alloc{0};

        /* These will be initialized / allocated on demand, when some shuffling or shaping is needed. */
        uint16_t *m_log2vis{nullptr};
        uint16_t *m_vis2log{nullptr};
        uint8_t  *m_vis_rtl{nullptr};  /* FIXME use a bitset */
        gunichar *m_vis_shaped_base_char{nullptr};  /* without combining accents */

        bool m_base_rtl{false};
        bool m_has_foreign{false};
};


/* BidiRunner is not a "real" class, rather the collection of methods that run the BiDi algorithm. */
class BidiRunner {
public:
        constexpr BidiRunner(RingView *ringview) : m_ringview{ringview} { }
        ~BidiRunner() { }

        // prevent accidents
        BidiRunner(BidiRunner& o) = delete;
        BidiRunner(BidiRunner const& o) = delete;
        BidiRunner(BidiRunner&& o) = delete;
        BidiRunner& operator= (BidiRunner& o) = delete;
        BidiRunner& operator= (BidiRunner const& o) = delete;
        BidiRunner& operator= (BidiRunner&& o) = delete;

        void paragraph(bte::grid::row_t start, bte::grid::row_t end,
                       bool do_bidi, bool do_shaping);

private:
        RingView *m_ringview;

#ifdef WITH_FRIBIDI
        void explicit_line_shape(bte::grid::row_t row);
#endif

        void explicit_line(bte::grid::row_t row, bool rtl, bool do_shaping);
        void explicit_paragraph(bte::grid::row_t start, bte::grid::row_t end, bool rtl, bool do_shaping);
#ifdef WITH_FRIBIDI
        bool implicit_paragraph(bte::grid::row_t start, bte::grid::row_t end, bool do_shaping);
#endif
};

}; /* namespace base */

}; /* namespace bte */


gboolean bte_bidi_get_mirror_char (bteunistr unistr, gboolean mirror_box_drawing, bteunistr *unistr_mirrored);
