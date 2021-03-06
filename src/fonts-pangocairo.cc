/*
 * Copyright (C) 2003,2008 Red Hat, Inc.
 * Copyright © 2019, 2020 Christian Persch
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

#include "config.h"

#include "fonts-pangocairo.hh"

#include "debug.h"
#include "btedefines.hh"

/* Have a space between letters to make sure ligatures aren't used when caching the glyphs: bug 793391. */
#define BTE_DRAW_SINGLE_WIDE_CHARACTERS	\
					"  ! \" # $ % & ' ( ) * + , - . / " \
					"0 1 2 3 4 5 6 7 8 9 " \
					": ; < = > ? @ " \
					"A B C D E F G H I J K L M N O P Q R S T U V W X Y Z " \
					"[ \\ ] ^ _ ` " \
					"a b c d e f g h i j k l m n o p q r s t u v w x y z " \
					"{ | } ~ " \
					""

static inline bool
_bte_double_equal(double a,
                  double b)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
        return a == b;
#pragma GCC diagnostic pop
}

#define FONT_CACHE_TIMEOUT (30) /* seconds */

namespace bte {
namespace view {

FontInfo::UnistrInfo*
FontInfo::find_unistr_info(bteunistr c)
{
	if (G_LIKELY (c < G_N_ELEMENTS(m_ascii_unistr_info)))
		return &m_ascii_unistr_info[c];

	if (G_UNLIKELY (m_other_unistr_info == nullptr))
		m_other_unistr_info = g_hash_table_new_full(nullptr, nullptr, nullptr, (GDestroyNotify)unistr_info_destroy);

	auto uinfo = reinterpret_cast<UnistrInfo*>(g_hash_table_lookup(m_other_unistr_info, GINT_TO_POINTER(c)));
	if (G_LIKELY (uinfo))
		return uinfo;

	uinfo = new UnistrInfo{};
	g_hash_table_insert(m_other_unistr_info, GINT_TO_POINTER (c), uinfo);
	return uinfo;
}

void
FontInfo::cache_ascii()
{
	PangoLayoutLine *line;
	PangoGlyphItemIter iter;
	PangoGlyphItem *glyph_item;
	PangoGlyphString *glyph_string;
	PangoFont *pango_font;
	cairo_scaled_font_t *scaled_font;
	const char *text;
	gboolean more;
	PangoLanguage *language;
	gboolean latin_uses_default_language;

	/* We have m_layout holding most ASCII characters.  We want to
	 * cache as much info as we can about the ASCII letters so we don't
	 * have to look them up again later */

	/* Don't cache if unknown glyphs found in layout */
	if (pango_layout_get_unknown_glyphs_count(m_layout.get()) != 0)
		return;

	language = pango_context_get_language(pango_layout_get_context(m_layout.get()));
	if (language == nullptr)
		language = pango_language_get_default ();
	latin_uses_default_language = pango_language_includes_script (language, PANGO_SCRIPT_LATIN);

	text = pango_layout_get_text(m_layout.get());

	line = pango_layout_get_line_readonly(m_layout.get(), 0);

	/* Don't cache if more than one font used for the line */
	if (G_UNLIKELY (!line || !line->runs || line->runs->next))
		return;

	glyph_item = (PangoGlyphItem *)line->runs->data;
	glyph_string = glyph_item->glyphs;
	pango_font = glyph_item->item->analysis.font;
	if (!pango_font)
		return;
	scaled_font = pango_cairo_font_get_scaled_font ((PangoCairoFont *) pango_font);
	if (!scaled_font)
		return;

	for (more = pango_glyph_item_iter_init_start (&iter, glyph_item, text);
	     more;
	     more = pango_glyph_item_iter_next_cluster (&iter))
	{
		PangoGlyphGeometry *geometry;
		PangoGlyph glyph;
		bteunistr c;

		/* Only cache simple clusters */
		if (iter.start_char +1 != iter.end_char  ||
		    iter.start_index+1 != iter.end_index ||
		    iter.start_glyph+1 != iter.end_glyph)
			continue;

		c = text[iter.start_index];
		glyph = glyph_string->glyphs[iter.start_glyph].glyph;
		geometry = &glyph_string->glyphs[iter.start_glyph].geometry;

		/* If not using the default locale language, only cache non-common
		 * characters as common characters get their font from their neighbors
		 * and we don't want to force Latin on them. */
		if (!latin_uses_default_language &&
                    g_unichar_get_script (c) <= G_UNICODE_SCRIPT_INHERITED)
			continue;

		/* Only cache simple glyphs */
		if (!(glyph <= 0xFFFF) || (geometry->x_offset | geometry->y_offset) != 0)
			continue;

		auto uinfo = find_unistr_info(c);
		if (G_UNLIKELY (uinfo->coverage() != UnistrInfo::Coverage::UNKNOWN))
			continue;

		auto ufi = &uinfo->m_ufi;

		uinfo->width = PANGO_PIXELS_CEIL (geometry->width);
		uinfo->has_unknown_chars = false;

		uinfo->set_coverage(UnistrInfo::Coverage::USE_CAIRO_GLYPH);

		ufi->using_cairo_glyph.scaled_font = cairo_scaled_font_reference (scaled_font);
		ufi->using_cairo_glyph.glyph_index = glyph;

#ifdef BTE_DEBUG
		m_coverage_count[0]++;
		m_coverage_count[(unsigned)uinfo->coverage()]++;
#endif
	}

#ifdef BTE_DEBUG
	_bte_debug_print (BTE_DEBUG_PANGOCAIRO,
			  "btepangocairo: %p cached %d ASCII letters\n",
			  (void*)this, m_coverage_count[0]);
#endif
}

void
FontInfo::measure_font()
{
	PangoRectangle logical;

        /* Measure U+0021..U+007E individually instead of all together and then
         * averaging. For monospace fonts, the results should be the same, but
         * if the user (by design, or trough mis-configuration) uses a proportional
         * font, the latter method will greatly underestimate the required width,
         * leading to unreadable, overlapping characters.
         * https://gitlab.gnome.org/GNOME/bte/issues/138
         */
        int max_width{1};
        int max_height{1};
        for (char c = 0x21; c < 0x7f; ++c) {
                pango_layout_set_text(m_layout.get(), &c, 1);
                pango_layout_get_extents(m_layout.get(), nullptr, &logical);
                max_width = std::max(max_width, PANGO_PIXELS_CEIL(logical.width));
                max_height = std::max(max_height, PANGO_PIXELS_CEIL(logical.height));
        }

        /* Use the sample text to get the baseline */
	pango_layout_set_text(m_layout.get(), BTE_DRAW_SINGLE_WIDE_CHARACTERS, -1);
	pango_layout_get_extents(m_layout.get(), nullptr, &logical);
	/* We don't do CEIL for width since we are averaging;
	 * rounding is more accurate */
	m_ascent = PANGO_PIXELS_CEIL(pango_layout_get_baseline(m_layout.get()));

        m_height = max_height;
        m_width = max_width;

	/* Now that we shaped the entire ASCII character string, cache glyph
	 * info for them */
	cache_ascii();

	if (m_height == 0) {
		m_height = PANGO_PIXELS_CEIL (logical.height);
	}
	if (m_ascent == 0) {
		m_ascent = PANGO_PIXELS_CEIL(pango_layout_get_baseline(m_layout.get()));
	}

	_bte_debug_print (BTE_DEBUG_MISC,
			  "btepangocairo: %p font metrics = %dx%d (%d)\n",
			  (void*)this, m_width, m_height, m_ascent);
}

FontInfo::FontInfo(PangoContext *context)
{
	_bte_debug_print (BTE_DEBUG_PANGOCAIRO,
			  "btepangocairo: %p allocating FontInfo\n",
			  (void*)this);

	m_layout = bte::glib::take_ref(pango_layout_new(context));

	auto tabs = pango_tab_array_new_with_positions(1, FALSE, PANGO_TAB_LEFT, 1);
	pango_layout_set_tabs(m_layout.get(), tabs);
	pango_tab_array_free(tabs);

        // FIXME!!!
	m_string = g_string_sized_new(BTE_UTF8_BPC+1);

	measure_font();

	g_hash_table_insert(s_font_info_for_context,
                            pango_layout_get_context(m_layout.get()),
                            this);

}

FontInfo::~FontInfo()
{
	g_hash_table_remove(s_font_info_for_context,
                            pango_layout_get_context(m_layout.get()));

#ifdef BTE_DEBUG
	_bte_debug_print (BTE_DEBUG_PANGOCAIRO,
			  "btepangocairo: %p freeing font_info.  coverages %d = %d + %d + %d\n",
			  (void*)this,
			  m_coverage_count[0],
			  m_coverage_count[1],
			  m_coverage_count[2],
			  m_coverage_count[3]);
#endif

	g_string_free(m_string, true);

	if (m_other_unistr_info) {
		g_hash_table_destroy(m_other_unistr_info);
	}
}

static GQuark
fontconfig_timestamp_quark (void)
{
	static GQuark quark;

	if (G_UNLIKELY (!quark))
		quark = g_quark_from_static_string ("bte-fontconfig-timestamp");

	return quark;
}

static void
bte_pango_context_set_fontconfig_timestamp (PangoContext *context,
					    guint         fontconfig_timestamp)
{
	g_object_set_qdata ((GObject *) context,
			    fontconfig_timestamp_quark (),
			    GUINT_TO_POINTER (fontconfig_timestamp));
}

static guint
bte_pango_context_get_fontconfig_timestamp (PangoContext *context)
{
	return GPOINTER_TO_UINT (g_object_get_qdata ((GObject *) context,
						     fontconfig_timestamp_quark ()));
}

static guint
context_hash (PangoContext *context)
{
	return pango_units_from_double (pango_cairo_context_get_resolution (context))
	     ^ pango_font_description_hash (pango_context_get_font_description (context))
	     ^ cairo_font_options_hash (pango_cairo_context_get_font_options (context))
	     ^ GPOINTER_TO_UINT (pango_context_get_language (context))
	     ^ bte_pango_context_get_fontconfig_timestamp (context);
}

static gboolean
context_equal (PangoContext *a,
	       PangoContext *b)
{
	return _bte_double_equal(pango_cairo_context_get_resolution(a), pango_cairo_context_get_resolution (b))
	    && pango_font_description_equal (pango_context_get_font_description (a), pango_context_get_font_description (b))
	    && cairo_font_options_equal (pango_cairo_context_get_font_options (a), pango_cairo_context_get_font_options (b))
	    && pango_context_get_language (a) == pango_context_get_language (b)
	    && bte_pango_context_get_fontconfig_timestamp (a) == bte_pango_context_get_fontconfig_timestamp (b);
}

// FIXMEchpe return bte::base::RefPtr<FontInfo>
/* assumes ownership/reference of context */
FontInfo*
FontInfo::find_for_context(bte::glib::RefPtr<PangoContext>& context)
{
	if (G_UNLIKELY (s_font_info_for_context == nullptr))
		s_font_info_for_context = g_hash_table_new((GHashFunc) context_hash, (GEqualFunc) context_equal);

	auto info = reinterpret_cast<FontInfo*>(g_hash_table_lookup(s_font_info_for_context, context.get()));
	if (G_LIKELY(info)) {
		_bte_debug_print (BTE_DEBUG_PANGOCAIRO,
				  "btepangocairo: %p found font_info in cache\n",
				  info);
		info = info->ref();
	} else {
                info = new FontInfo{context.release()};
	}

	return info;
}

/* assumes ownership/reference of context */
FontInfo*
FontInfo::create_for_context(bte::glib::RefPtr<PangoContext> context,
                             PangoFontDescription const* desc,
                             PangoLanguage* language,
                             guint fontconfig_timestamp)
{
	if (!PANGO_IS_CAIRO_FONT_MAP(pango_context_get_font_map(context.get()))) {
		/* Ouch, Ctk+ switched over to some drawing system?
		 * Lets just create one from the default font map.
		 */
		context = bte::glib::take_ref(pango_font_map_create_context(pango_cairo_font_map_get_default()));
	}

	bte_pango_context_set_fontconfig_timestamp(context.get(), fontconfig_timestamp);

	pango_context_set_base_dir(context.get(), PANGO_DIRECTION_LTR);

	if (desc)
		pango_context_set_font_description(context.get(), desc);

	pango_context_set_language(context.get(), language);

        /* Make sure our contexts have a font_options set.  We use
          * this invariant in our context hash and equal functions.
          */
        if (!pango_cairo_context_get_font_options(context.get())) {
                cairo_font_options_t *font_options;

                font_options = cairo_font_options_create ();
                pango_cairo_context_set_font_options(context.get(), font_options);
                cairo_font_options_destroy (font_options);
        }

	return find_for_context(context);
}

FontInfo*
FontInfo::create_for_screen(CdkScreen* screen,
                            PangoFontDescription const* desc,
                            PangoLanguage* language)
{
	auto settings = ctk_settings_get_for_screen(screen);
	auto fontconfig_timestamp = guint{};
	g_object_get (settings, "ctk-fontconfig-timestamp", &fontconfig_timestamp, nullptr);
	return create_for_context(bte::glib::take_ref(cdk_pango_context_get_for_screen(screen)),
                                  desc, language, fontconfig_timestamp);
}

FontInfo*
FontInfo::create_for_widget(CtkWidget* widget,
                            PangoFontDescription const* desc)
{
	auto screen = ctk_widget_get_screen(widget);
	auto language = pango_context_get_language(ctk_widget_get_pango_context(widget));

	return create_for_screen(screen, desc, language);
}

FontInfo::UnistrInfo*
FontInfo::get_unistr_info(bteunistr c)
{
	PangoRectangle logical;
	PangoLayoutLine *line;

	auto uinfo = find_unistr_info(c);
	if (G_LIKELY (uinfo->coverage() != UnistrInfo::Coverage::UNKNOWN))
		return uinfo;

	auto ufi = &uinfo->m_ufi;

	g_string_set_size(m_string, 0);
	_bte_unistr_append_to_string(c, m_string);
	pango_layout_set_text(m_layout.get(), m_string->str, m_string->len);
	pango_layout_get_extents(m_layout.get(), NULL, &logical);

	uinfo->width = PANGO_PIXELS_CEIL (logical.width);

	line = pango_layout_get_line_readonly(m_layout.get(), 0);

	uinfo->has_unknown_chars = pango_layout_get_unknown_glyphs_count(m_layout.get()) != 0;
	/* we use PangoLayoutRun rendering unless there is exactly one run in the line. */
	if (G_UNLIKELY (!line || !line->runs || line->runs->next))
	{
		uinfo->set_coverage(UnistrInfo::Coverage::USE_PANGO_LAYOUT_LINE);

		ufi->using_pango_layout_line.line = pango_layout_line_ref (line);
		/* we hold a manual reference on layout.  pango currently
		 * doesn't work if line->layout is NULL.  ugh! */
		pango_layout_set_text(m_layout.get(), "", -1); /* make layout disassociate from the line */
		ufi->using_pango_layout_line.line->layout = (PangoLayout *)g_object_ref(m_layout.get());

	} else {
		PangoGlyphItem *glyph_item = (PangoGlyphItem *)line->runs->data;
		PangoFont *pango_font = glyph_item->item->analysis.font;
		PangoGlyphString *glyph_string = glyph_item->glyphs;

		/* we use fast cairo path if glyph string has only one real
		 * glyph and at origin */
		if (!uinfo->has_unknown_chars &&
		    glyph_string->num_glyphs == 1 && glyph_string->glyphs[0].glyph <= 0xFFFF &&
		    (glyph_string->glyphs[0].geometry.x_offset |
		     glyph_string->glyphs[0].geometry.y_offset) == 0)
		{
			cairo_scaled_font_t *scaled_font = pango_cairo_font_get_scaled_font ((PangoCairoFont *) pango_font);

			if (scaled_font) {
				uinfo->set_coverage(UnistrInfo::Coverage::USE_CAIRO_GLYPH);

				ufi->using_cairo_glyph.scaled_font = cairo_scaled_font_reference (scaled_font);
				ufi->using_cairo_glyph.glyph_index = glyph_string->glyphs[0].glyph;
			}
		}

		/* use pango fast path otherwise */
		if (G_UNLIKELY (uinfo->coverage() == UnistrInfo::Coverage::UNKNOWN)) {
			uinfo->set_coverage(UnistrInfo::Coverage::USE_PANGO_GLYPH_STRING);

			ufi->using_pango_glyph_string.font = pango_font ? (PangoFont *)g_object_ref (pango_font) : NULL;
			ufi->using_pango_glyph_string.glyph_string = pango_glyph_string_copy (glyph_string);
		}
	}

	/* release internal layout resources */
	pango_layout_set_text(m_layout.get(), "", -1);

#ifdef BTE_DEBUG
	m_coverage_count[0]++;
	m_coverage_count[uinfo->m_coverage]++;
#endif

	return uinfo;
}

} // namespace view
} // namespace bte
