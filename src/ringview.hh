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

#include "bidi.hh"
#include "ring.hh"
#include "bterowdata.hh"
#include "btetypes.hh"
#include "bteunistr.h"

namespace bte {

namespace base {

class BidiRow;
class BidiRunner;

/*
 * RingView provides a "view" to a continuous segment of the Ring (or stream),
 * typically the user visible area.
 *
 * It computes additional data that are needed to display the contents (or handle
 * user events such as mouse click), but not needed for the terminal emulation logic.
 * In order to save tons of resources, these data are computed when the Ring's
 * contents are about to be displayed, rather than whenever they change.
 *
 * For computing these data, context lines (outside of the specified region of the
 * Ring) are also taken into account up to the next hard newline or a safety limit.
 *
 * Currently RingView is used for BiDi: to figure out which logical character is
 * mapped to which visual position.
 *
 * Future possible uses include "highlight all" for the search match, and
 * syntax highlighting. URL autodetection might also be ported to this
 * infrastructure one day.
 */
class RingView {
        friend class BidiRunner;

public:
        RingView();
        ~RingView();

        // prevent accidents
        RingView(RingView& o) = delete;
        RingView(RingView const& o) = delete;
        RingView(RingView&& o) = delete;
        RingView& operator= (RingView& o) = delete;
        RingView& operator= (RingView const& o) = delete;
        RingView& operator= (RingView&& o) = delete;

        void set_ring(Ring *ring);
        void set_rows(bte::grid::row_t start, bte::grid::row_t len);
        void set_width(bte::grid::column_t width);
        inline constexpr bte::grid::column_t get_width() const noexcept { return m_width; }
        void set_enable_bidi(bool enable_bidi);
        void set_enable_shaping(bool enable_shaping);

        inline void invalidate() { m_invalid = true; }
        inline constexpr bool is_updated() const noexcept { return !m_invalid; }
        void update();
        void pause();

        BteRowData const* get_row(bte::grid::row_t row) const;

        BidiRow const* get_bidirow(bte::grid::row_t row) const;

private:
        Ring *m_ring{nullptr};

        BteRowData **m_rows{nullptr};
        int m_rows_len{0};
        int m_rows_alloc_len{0};

        bool m_enable_bidi{true};      /* These two are the most convenient defaults */
        bool m_enable_shaping{false};  /* for short-lived ringviews. */
        BidiRow **m_bidirows{nullptr};
        int m_bidirows_alloc_len{0};

        std::unique_ptr<BidiRunner> m_bidirunner;

        bte::grid::row_t m_top{0};  /* the row of the Ring corresponding to m_rows[0] */

        bte::grid::row_t m_start{0};
        bte::grid::row_t m_len{0};
        bte::grid::column_t m_width{0};

        bool m_invalid{true};
        bool m_paused{true};

        void resume();

        BidiRow* get_bidirow_writable(bte::grid::row_t row) const;
};

}; /* namespace base */

}; /* namespace bte */
