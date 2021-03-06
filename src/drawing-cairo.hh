/*
 * Copyright (C) 2003 Red Hat, Inc.
 * Copyright © 2020 Christian Persch
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <cstdint>

#include <memory>

#include <cairo.h>

#include <glib.h>
#include <ctk/ctk.h>

#include "cairo-glue.hh"
#include "fwd.hh"
#include "minifont.hh"
#include "btetypes.hh"
#include "bteunistr.h"

namespace bte {
namespace view {

class DrawingContext {
public:

        /* A request to draw a particular character spanning a given number of columns
           at the given location.  Unlike most APIs, (x,y) specifies the top-left
           corner of the cell into which the character will be drawn instead of the
           left end of the baseline. */
        struct TextRequest {
                bteunistr c;
                int16_t x, y, columns;

                /* Char has RTL resolved directionality, mirror if mirrorable. */
                uint8_t mirror : 1;

                /* Add box drawing chars to the set of mirrorable characters. */
                uint8_t box_mirror : 1;
        };

        DrawingContext() noexcept = default;
        ~DrawingContext();

        DrawingContext(DrawingContext const&) = delete;
        DrawingContext(DrawingContext&&) = delete;
        DrawingContext& operator=(DrawingContext const&) = delete;
        DrawingContext& operator=(DrawingContext&&) = delete;

        void set_cairo(cairo_t* cr) noexcept;
        auto cairo() const noexcept { return m_cr; }

        void clip(cairo_rectangle_int_t const* rect);
        void unclip();

        void clear(int x,
                   int y,
                   int width,
                   int height,
                   bte::color::rgb const* color,
                   double alpha);
        void clear_font_cache();
        void set_text_font(CtkWidget* widget,
                           PangoFontDescription const* fontdesc,
                           double cell_width_scale,
                           double cell_height_scale);
        void get_text_metrics(int* cell_width,
                              int* cell_height,
                              int* char_ascent,
                              int* char_descent,
                              CtkBorder* char_spacing);
        void get_char_edges(bteunistr c,
                            int columns,
                            uint32_t attr,
                            int& left,
                            int& right);
        void draw_text(TextRequest* requests,
                       gsize n_requests,
                       uint32_t attr,
                       bte::color::rgb const* color,
                       double alpha);
        bool draw_char(TextRequest* request,
                       uint32_t attr,
                       bte::color::rgb const* color,
                       double alpha);
        bool has_char(bteunistr c,
                      uint32_t attr);
        void fill_rectangle(int x,
                            int y,
                            int width,
                            int height,
                            bte::color::rgb const* color,
                            double alpha);
        void draw_rectangle(int x,
                            int y,
                            int width,
                            int height,
                            bte::color::rgb const* color,
                            double alpha);
        void draw_line(int x,
                       int y,
                       int xp,
                       int yp,
                       int line_width,
                       bte::color::rgb const *color,
                       double alpha);

        void draw_undercurl(int x,
                            double y,
                            double line_width,
                            int count,
                            bte::color::rgb const* color,
                            double alpha);

        auto cell_width()  const noexcept { return m_cell_width; }
        auto cell_height() const noexcept { return m_cell_height; }

private:
        void set_source_color_alpha (bte::color::rgb const* color,
                                     double alpha);
        void draw_graphic(bteunistr c,
                          uint32_t attr,
                          bte::color::rgb const* fg,
                          int x,
                          int y,
                          int font_width,
                          int columns,
                          int font_height);
        void draw_text_internal(TextRequest* requests,
                                gsize n_requests,
                                uint32_t attr,
                                bte::color::rgb const* color,
                                double alpha);

        //        std::array<bte::base::RefPtr<FontInfo>, 4> m_fonts{};
	FontInfo* m_fonts[4]{nullptr, nullptr, nullptr, nullptr};
        int m_cell_width{1};
        int m_cell_height{1};
        CtkBorder m_char_spacing{1, 1, 1, 1};

	cairo_t *m_cr{nullptr}; // unowned

        Minifont m_minifont{};

        /* Cache the undercurl's rendered look. */
        bte::cairo::Surface m_undercurl_surface{};

}; // class DrawingContext

} // namespace view
} // namespace bte

double
_bte_draw_get_undercurl_height(gint width, double line_width);

class _bte_draw_autoclip_t {
private:
        bte::view::DrawingContext& m_draw;
public:
        _bte_draw_autoclip_t(bte::view::DrawingContext& draw,
                             cairo_rectangle_int_t const* rect)
                : m_draw{draw}
        {
                m_draw.clip(rect);
        }

        ~_bte_draw_autoclip_t()
        {
                m_draw.unclip();
        }
};
