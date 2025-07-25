/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2002,2003 Red Hat, Inc.
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

#include "config.h"

#include <cassert>

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <glib.h>
#include <ctk/ctk.h>
#include "caps.hh"
#include "debug.h"
#include "keymap.h"

#ifdef BTE_DEBUG
static void
_bte_keysym_print(guint keyval,
                  guint modifiers)
{
	g_printerr("Mapping ");
	if (modifiers & CDK_CONTROL_MASK) {
		g_printerr("Control+");
	}
	if (modifiers & BTE_ALT_MASK) {
		g_printerr("Alt+");
	}
	if (modifiers & BTE_NUMLOCK_MASK) {
		g_printerr("NumLock+");
	}
	if (modifiers & CDK_SHIFT_MASK) {
		g_printerr("Shift+");
	}
	g_printerr("%s" , cdk_keyval_name(keyval));
}
#else
static void
_bte_keysym_print(guint keyval,
                  guint modifiers)
{
}
#endif

enum _bte_cursor_mode {
	cursor_default =	1u << 0,
	cursor_app =		1u << 1
};

enum _bte_keypad_mode {
	keypad_default =	1u << 0,
	keypad_app =		1u << 1
};

#define cursor_all	(cursor_default | cursor_app)
#define keypad_all	(keypad_default | keypad_app)

struct _bte_keymap_entry {
	guint cursor_mode;
	guint keypad_mode;
	guint mod_mask;
	const char normal[8];
        int8_t normal_length;
};

#define X_NULL ""

enum _bte_modifier_encoding_method {
	MODIFIER_ENCODING_NONE,
	MODIFIER_ENCODING_SHORT,
	MODIFIER_ENCODING_LONG,
};

static const struct _bte_keymap_entry _bte_keymap_CDK_space[] = {
	/* Control+Alt+space = ESC+NUL */
        {cursor_all, keypad_all, CDK_CONTROL_MASK | BTE_ALT_MASK, _BTE_CAP_ESC "\0", 2},
	/* Alt+space = ESC+" " */
        {cursor_all, keypad_all, BTE_ALT_MASK, _BTE_CAP_ESC " ", 2},
	/* Control+space = NUL */
        {cursor_all, keypad_all, CDK_CONTROL_MASK, "\0", 1},
	/* Regular space. */
        {cursor_all, keypad_all, 0, " ", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_Tab[] = {
	/* Shift+Tab = Back-Tab */
        {cursor_all, keypad_all, CDK_SHIFT_MASK, _BTE_CAP_CSI "Z", -1},
	/* Alt+Tab = Esc+Tab */
        {cursor_all, keypad_all, BTE_ALT_MASK, _BTE_CAP_ESC "\t", -1},
	/* Regular tab. */
        {cursor_all, keypad_all, 0, "\t", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_Return[] = {
        {cursor_all, keypad_all, BTE_ALT_MASK, _BTE_CAP_ESC "\r", 2},
        {cursor_all, keypad_all, 0, "\r", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_Escape[] = {
        {cursor_all, keypad_all, BTE_ALT_MASK, _BTE_CAP_ESC _BTE_CAP_ESC, 2},
        {cursor_all, keypad_all, 0, _BTE_CAP_ESC, 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_Insert[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_CSI "2~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_ISO_Left_Tab[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_CSI "Z", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_slash[] = {
        {cursor_all, keypad_all, BTE_ALT_MASK, _BTE_CAP_ESC "/", 2},
        {cursor_all, keypad_all, CDK_CONTROL_MASK, "\037", 1},
        {cursor_all, keypad_all, 0, "/", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_question[] = {
        {cursor_all, keypad_all, BTE_ALT_MASK, _BTE_CAP_ESC "?", 2},
        {cursor_all, keypad_all, CDK_CONTROL_MASK, "\177", 1},
        {cursor_all, keypad_all, 0, "?", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

/* Various numeric keys enter control characters. */
static const struct _bte_keymap_entry _bte_keymap_CDK_2[] = {
        {cursor_all, keypad_all, CDK_CONTROL_MASK, "\0", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};
static const struct _bte_keymap_entry _bte_keymap_CDK_3[] = {
        {cursor_all, keypad_all, CDK_CONTROL_MASK, "\033", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};
static const struct _bte_keymap_entry _bte_keymap_CDK_4[] = {
        {cursor_all, keypad_all, CDK_CONTROL_MASK, "\034", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};
static const struct _bte_keymap_entry _bte_keymap_CDK_5[] = {
        {cursor_all, keypad_all, CDK_CONTROL_MASK, "\035", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};
static const struct _bte_keymap_entry _bte_keymap_CDK_6[] = {
        {cursor_all, keypad_all, CDK_CONTROL_MASK, "\036", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};
static const struct _bte_keymap_entry _bte_keymap_CDK_7[] = {
        {cursor_all, keypad_all, CDK_CONTROL_MASK, "\037", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};
static const struct _bte_keymap_entry _bte_keymap_CDK_8[] = {
        {cursor_all, keypad_all, CDK_CONTROL_MASK, "\177", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};
static const struct _bte_keymap_entry _bte_keymap_CDK_Minus[] = {
        {cursor_all, keypad_all, CDK_CONTROL_MASK, "\037", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

/* Keys (potentially) affected by the cursor key mode. */
static const struct _bte_keymap_entry _bte_keymap_CDK_Home[] = {
        {cursor_default, keypad_all, 0, _BTE_CAP_CSI "H", -1},
        {cursor_app, keypad_all, 0, _BTE_CAP_SS3 "H", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_End[] = {
        {cursor_default, keypad_all, 0, _BTE_CAP_CSI "F", -1},
        {cursor_app, keypad_all, 0, _BTE_CAP_SS3 "F", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_Page_Up[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_CSI "5~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_Page_Down[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_CSI "6~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_Up[] = {
        {cursor_default, keypad_all, 0, _BTE_CAP_CSI "A", -1},
        {cursor_app, keypad_all, 0, _BTE_CAP_SS3 "A", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_Down[] = {
        {cursor_default, keypad_all, 0, _BTE_CAP_CSI "B", -1},
        {cursor_app, keypad_all, 0, _BTE_CAP_SS3 "B", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_Right[] = {
        {cursor_default, keypad_all, 0, _BTE_CAP_CSI "C", -1},
        {cursor_app, keypad_all, 0, _BTE_CAP_SS3 "C", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_Left[] = {
        {cursor_default, keypad_all, 0, _BTE_CAP_CSI "D", -1},
        {cursor_app, keypad_all, 0, _BTE_CAP_SS3 "D", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

/* Keys (potentially) affected by the keypad key mode. */
static const struct _bte_keymap_entry _bte_keymap_CDK_KP_Space[] = {
        {cursor_all, keypad_default, 0, " ", 1},
        {cursor_all, keypad_app, 0, _BTE_CAP_SS3 " ", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_KP_Tab[] = {
        {cursor_all, keypad_default, 0, "\t", 1},
        {cursor_all, keypad_app, 0, _BTE_CAP_SS3 "I", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_KP_Enter[] = {
        {cursor_all, keypad_app, BTE_NUMLOCK_MASK, "\r", 1},
        {cursor_all, keypad_app, 0, _BTE_CAP_SS3 "M", -1},
        {cursor_all, keypad_all, 0, "\r", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_KP_F1[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_SS3 "P", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_KP_F2[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_SS3 "Q", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_KP_F3[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_SS3 "R", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_KP_F4[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_SS3 "S", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_KP_Multiply[] = {
        {cursor_all, keypad_default, 0, "*", 1},
        {cursor_all, keypad_app, BTE_NUMLOCK_MASK, "*", 1},
        {cursor_all, keypad_app, 0, _BTE_CAP_SS3 "j", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_KP_Add[] = {
        {cursor_all, keypad_default, 0, "+", 1},
        {cursor_all, keypad_app, BTE_NUMLOCK_MASK, "+", 1},
        {cursor_all, keypad_app, 0, _BTE_CAP_SS3 "k", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_KP_Separator[] = {
        {cursor_all, keypad_default, 0, ",", 1},
        {cursor_all, keypad_app, 0, _BTE_CAP_SS3 "l", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_KP_Subtract[] = {
        {cursor_all, keypad_default, 0, "-", 1},
        {cursor_all, keypad_app, BTE_NUMLOCK_MASK, "-", 1},
        {cursor_all, keypad_app, 0, _BTE_CAP_SS3 "m", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_KP_Decimal_Delete[] = {
        {cursor_all, keypad_default, 0, ".", 1},
        {cursor_all, keypad_app, 0, _BTE_CAP_SS3 "3~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_KP_Divide[] = {
        {cursor_all, keypad_default, 0, "/", 1},
        {cursor_all, keypad_app, BTE_NUMLOCK_MASK, "/", 1},
        {cursor_all, keypad_app, 0, _BTE_CAP_SS3 "o", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

/* CDK already separates keypad "0" from keypad "Insert", so the only time
 * we'll see this key is when NumLock is on. */
static const struct _bte_keymap_entry _bte_keymap_CDK_KP_0[] = {
        {cursor_all, keypad_all, 0, "0", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_KP_1[] = {
        {cursor_all, keypad_all, 0, "1", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_KP_2[] = {
        {cursor_all, keypad_all, 0, "2", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_KP_3[] = {
        {cursor_all, keypad_all, 0, "3", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_KP_4[] = {
        {cursor_all, keypad_all, 0, "4", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_KP_5[] = {
        {cursor_all, keypad_all, 0, "5", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_KP_6[] = {
        {cursor_all, keypad_all, 0, "6", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_KP_7[] = {
        {cursor_all, keypad_all, 0, "7", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_KP_8[] = {
        {cursor_all, keypad_all, 0, "8", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_KP_9[] = {
        {cursor_all, keypad_all, 0, "9", 1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

/* These are the same keys as above, but without numlock. */
static const struct _bte_keymap_entry _bte_keymap_CDK_KP_Insert[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_CSI "2~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_KP_End[] = {
        {cursor_app, keypad_all, 0, _BTE_CAP_SS3 "F", -1},
        {cursor_default, keypad_all, 0, _BTE_CAP_CSI "F", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_KP_Down[] = {
        {cursor_app, keypad_all, 0, _BTE_CAP_SS3 "B", -1},
        {cursor_default, keypad_all, 0, _BTE_CAP_CSI "B", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_KP_Page_Down[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_CSI "6~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_KP_Left[] = {
        {cursor_app, keypad_all, 0, _BTE_CAP_SS3 "D", -1},
        {cursor_default, keypad_all, 0, _BTE_CAP_CSI "D", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_KP_Begin[] = {
        {cursor_app, keypad_all, 0, _BTE_CAP_SS3 "E", -1},
        {cursor_default, keypad_all, 0, _BTE_CAP_CSI "E", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_KP_Right[] = {
        {cursor_app, keypad_all, 0, _BTE_CAP_SS3 "C", -1},
        {cursor_default, keypad_all, 0, _BTE_CAP_CSI "C", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_KP_Home[] = {
        {cursor_app, keypad_all, 0, _BTE_CAP_SS3 "H", -1},
        {cursor_default, keypad_all, 0, _BTE_CAP_CSI "H", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_KP_Up[] = {
        {cursor_app, keypad_all, 0, _BTE_CAP_SS3 "A", -1},
        {cursor_default, keypad_all, 0, _BTE_CAP_CSI "A", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_KP_Page_Up[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_CSI "5~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_F1[] = {
        {cursor_all, keypad_all, CDK_CONTROL_MASK, _BTE_CAP_CSI "P", -1},
        {cursor_all, keypad_all, CDK_SHIFT_MASK, _BTE_CAP_CSI "P", -1},
        {cursor_all, keypad_all, BTE_ALT_MASK, _BTE_CAP_CSI "P", -1},
        {cursor_all, keypad_all, 0, _BTE_CAP_SS3 "P", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_F2[] = {
        {cursor_all, keypad_all, CDK_CONTROL_MASK, _BTE_CAP_CSI "Q", -1},
        {cursor_all, keypad_all, CDK_SHIFT_MASK, _BTE_CAP_CSI "Q", -1},
        {cursor_all, keypad_all, BTE_ALT_MASK, _BTE_CAP_CSI "Q", -1},
        {cursor_all, keypad_all, 0, _BTE_CAP_SS3 "Q", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_F3[] = {
        {cursor_all, keypad_all, CDK_CONTROL_MASK, _BTE_CAP_CSI "R", -1},
        {cursor_all, keypad_all, CDK_SHIFT_MASK, _BTE_CAP_CSI "R", -1},
        {cursor_all, keypad_all, BTE_ALT_MASK, _BTE_CAP_CSI "R", -1},
        {cursor_all, keypad_all, 0, _BTE_CAP_SS3 "R", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_F4[] = {
        {cursor_all, keypad_all, CDK_CONTROL_MASK, _BTE_CAP_CSI "S", -1},
        {cursor_all, keypad_all, CDK_SHIFT_MASK, _BTE_CAP_CSI "S", -1},
        {cursor_all, keypad_all, BTE_ALT_MASK, _BTE_CAP_CSI "S", -1},
        {cursor_all, keypad_all, 0, _BTE_CAP_SS3 "S", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_F5[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_CSI "15~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_F6[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_CSI "17~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_F7[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_CSI "18~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_F8[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_CSI "19~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_F9[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_CSI "20~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_F10[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_CSI "21~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_F11[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_CSI "23~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_F12[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_CSI "24~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_F13[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_CSI "25~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_F14[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_CSI "26~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_F15[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_CSI "28~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_F16[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_CSI "29~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_F17[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_CSI "31~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_F18[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_CSI "32~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_F19[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_CSI "33~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_F20[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_CSI "34~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_F21[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_CSI "42~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_F22[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_CSI "43~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_F23[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_CSI "44~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_F24[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_CSI "45~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_F25[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_CSI "46~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_F26[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_CSI "47~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_F27[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_CSI "48~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_F28[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_CSI "49~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_F29[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_CSI "50~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_F30[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_CSI "51~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_F31[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_CSI "52~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_F32[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_CSI "53~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_F33[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_CSI "54~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_F34[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_CSI "55~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_entry _bte_keymap_CDK_F35[] = {
        {cursor_all, keypad_all, 0, _BTE_CAP_CSI "56~", -1},
        {cursor_all, keypad_all, 0, X_NULL, 0},
};

static const struct _bte_keymap_group {
	guint keyval;
	const struct _bte_keymap_entry *entries;
} _bte_keymap[] = {
	{CDK_KEY_space,			_bte_keymap_CDK_space},
	{CDK_KEY_Return,		_bte_keymap_CDK_Return},
	{CDK_KEY_Escape,		_bte_keymap_CDK_Escape},
	{CDK_KEY_Tab,			_bte_keymap_CDK_Tab},
	{CDK_KEY_ISO_Left_Tab,		_bte_keymap_CDK_ISO_Left_Tab},
	{CDK_KEY_Home,			_bte_keymap_CDK_Home},
	{CDK_KEY_End,			_bte_keymap_CDK_End},
	{CDK_KEY_Insert,		_bte_keymap_CDK_Insert},
	{CDK_KEY_slash,			_bte_keymap_CDK_slash},
	{CDK_KEY_question,		_bte_keymap_CDK_question},
	/* CDK_Delete is all handled in code, due to funkiness. */
	{CDK_KEY_Page_Up,		_bte_keymap_CDK_Page_Up},
	{CDK_KEY_Page_Down,		_bte_keymap_CDK_Page_Down},

	{CDK_KEY_2,			_bte_keymap_CDK_2},
	{CDK_KEY_3,			_bte_keymap_CDK_3},
	{CDK_KEY_4,			_bte_keymap_CDK_4},
	{CDK_KEY_5,			_bte_keymap_CDK_5},
	{CDK_KEY_6,			_bte_keymap_CDK_6},
	{CDK_KEY_7,			_bte_keymap_CDK_7},
	{CDK_KEY_8,			_bte_keymap_CDK_8},
	{CDK_KEY_minus,			_bte_keymap_CDK_Minus},

	{CDK_KEY_Up,			_bte_keymap_CDK_Up},
	{CDK_KEY_Down,			_bte_keymap_CDK_Down},
	{CDK_KEY_Right,			_bte_keymap_CDK_Right},
	{CDK_KEY_Left,			_bte_keymap_CDK_Left},

	{CDK_KEY_KP_Space,		_bte_keymap_CDK_KP_Space},
	{CDK_KEY_KP_Tab,		_bte_keymap_CDK_KP_Tab},
	{CDK_KEY_KP_Enter,		_bte_keymap_CDK_KP_Enter},
	{CDK_KEY_KP_F1,			_bte_keymap_CDK_KP_F1},
	{CDK_KEY_KP_F2,			_bte_keymap_CDK_KP_F2},
	{CDK_KEY_KP_F3,			_bte_keymap_CDK_KP_F3},
	{CDK_KEY_KP_F4,			_bte_keymap_CDK_KP_F4},
	{CDK_KEY_KP_Multiply,		_bte_keymap_CDK_KP_Multiply},
	{CDK_KEY_KP_Add,		_bte_keymap_CDK_KP_Add},
	{CDK_KEY_KP_Separator,		_bte_keymap_CDK_KP_Separator},
	{CDK_KEY_KP_Subtract,		_bte_keymap_CDK_KP_Subtract},
	{CDK_KEY_KP_Decimal,		_bte_keymap_CDK_KP_Decimal_Delete},
	{CDK_KEY_KP_Delete,		_bte_keymap_CDK_KP_Decimal_Delete},
	{CDK_KEY_KP_Divide,		_bte_keymap_CDK_KP_Divide},
	{CDK_KEY_KP_0,			_bte_keymap_CDK_KP_0},
	{CDK_KEY_KP_Insert,		_bte_keymap_CDK_KP_Insert},
	{CDK_KEY_KP_1,			_bte_keymap_CDK_KP_1},
	{CDK_KEY_KP_End,		_bte_keymap_CDK_KP_End},
	{CDK_KEY_KP_2,			_bte_keymap_CDK_KP_2},
	{CDK_KEY_KP_Down,		_bte_keymap_CDK_KP_Down},
	{CDK_KEY_KP_3,			_bte_keymap_CDK_KP_3},
	{CDK_KEY_KP_Page_Down,		_bte_keymap_CDK_KP_Page_Down},
	{CDK_KEY_KP_4,			_bte_keymap_CDK_KP_4},
	{CDK_KEY_KP_Left,		_bte_keymap_CDK_KP_Left},
	{CDK_KEY_KP_5,			_bte_keymap_CDK_KP_5},
	{CDK_KEY_KP_Begin,		_bte_keymap_CDK_KP_Begin},
	{CDK_KEY_KP_6,			_bte_keymap_CDK_KP_6},
	{CDK_KEY_KP_Right,		_bte_keymap_CDK_KP_Right},
	{CDK_KEY_KP_7,			_bte_keymap_CDK_KP_7},
	{CDK_KEY_KP_Home,		_bte_keymap_CDK_KP_Home},
	{CDK_KEY_KP_8,			_bte_keymap_CDK_KP_8},
	{CDK_KEY_KP_Up,			_bte_keymap_CDK_KP_Up},
	{CDK_KEY_KP_9,			_bte_keymap_CDK_KP_9},
	{CDK_KEY_KP_Page_Up,		_bte_keymap_CDK_KP_Page_Up},

	{CDK_KEY_F1,			_bte_keymap_CDK_F1},
	{CDK_KEY_F2,			_bte_keymap_CDK_F2},
	{CDK_KEY_F3,			_bte_keymap_CDK_F3},
	{CDK_KEY_F4,			_bte_keymap_CDK_F4},
	{CDK_KEY_F5,			_bte_keymap_CDK_F5},
	{CDK_KEY_F6,			_bte_keymap_CDK_F6},
	{CDK_KEY_F7,			_bte_keymap_CDK_F7},
	{CDK_KEY_F8,			_bte_keymap_CDK_F8},
	{CDK_KEY_F9,			_bte_keymap_CDK_F9},
	{CDK_KEY_F10,			_bte_keymap_CDK_F10},
	{CDK_KEY_F11,			_bte_keymap_CDK_F11},
	{CDK_KEY_F12,			_bte_keymap_CDK_F12},
	{CDK_KEY_F13,			_bte_keymap_CDK_F13},
	{CDK_KEY_F14,			_bte_keymap_CDK_F14},
	{CDK_KEY_F15,			_bte_keymap_CDK_F15},
	{CDK_KEY_F16,			_bte_keymap_CDK_F16},
	{CDK_KEY_F17,			_bte_keymap_CDK_F17},
	{CDK_KEY_F18,			_bte_keymap_CDK_F18},
	{CDK_KEY_F19,			_bte_keymap_CDK_F19},
	{CDK_KEY_F20,			_bte_keymap_CDK_F20},
	{CDK_KEY_F21,			_bte_keymap_CDK_F21},
	{CDK_KEY_F22,			_bte_keymap_CDK_F22},
	{CDK_KEY_F23,			_bte_keymap_CDK_F23},
	{CDK_KEY_F24,			_bte_keymap_CDK_F24},
	{CDK_KEY_F25,			_bte_keymap_CDK_F25},
	{CDK_KEY_F26,			_bte_keymap_CDK_F26},
	{CDK_KEY_F27,			_bte_keymap_CDK_F27},
	{CDK_KEY_F28,			_bte_keymap_CDK_F28},
	{CDK_KEY_F29,			_bte_keymap_CDK_F29},
	{CDK_KEY_F30,			_bte_keymap_CDK_F30},
	{CDK_KEY_F31,			_bte_keymap_CDK_F31},
	{CDK_KEY_F32,			_bte_keymap_CDK_F32},
	{CDK_KEY_F33,			_bte_keymap_CDK_F33},
	{CDK_KEY_F34,			_bte_keymap_CDK_F34},
	{CDK_KEY_F35,			_bte_keymap_CDK_F35},
};

/* Map the specified keyval/modifier setup, dependent on the mode, to
 * a literal string. */
void
_bte_keymap_map(guint keyval,
		guint modifiers,
		gboolean app_cursor_keys,
		gboolean app_keypad_keys,
		char **normal,
		gsize *normal_length)
{
	gsize i;
	const struct _bte_keymap_entry *entries;
	enum _bte_cursor_mode cursor_mode;
	enum _bte_keypad_mode keypad_mode;

	g_return_if_fail(normal != NULL);
	g_return_if_fail(normal_length != NULL);

	_BTE_DEBUG_IF(BTE_DEBUG_KEYBOARD) 
		_bte_keysym_print(keyval, modifiers);

	/* Start from scratch. */
	*normal = NULL;
	*normal_length = 0;

	/* Search for the list for this key. */
	entries = NULL;
	for (i = 0; i < G_N_ELEMENTS(_bte_keymap); i++) {
		if (_bte_keymap[i].keyval == keyval) {
			/* Found it! */
			entries = _bte_keymap[i].entries;
			break;
		}
	}
	if (entries == NULL) {
		_bte_debug_print(BTE_DEBUG_KEYBOARD,
				" (ignoring, no map for key).\n");
		return;
	}

	/* Build mode masks. */
	cursor_mode = app_cursor_keys ? cursor_app : cursor_default;
	keypad_mode = app_keypad_keys ? keypad_app : keypad_default;
	modifiers &= CDK_SHIFT_MASK | CDK_CONTROL_MASK | BTE_ALT_MASK | BTE_NUMLOCK_MASK;

	/* Search for the conditions. */
	for (i = 0; entries[i].normal_length; i++)
	if ((entries[i].cursor_mode & cursor_mode) &&
	    (entries[i].keypad_mode & keypad_mode))
	if ((modifiers & entries[i].mod_mask) == entries[i].mod_mask) {
                if (entries[i].normal_length != -1) {
                        *normal_length = entries[i].normal_length;
                        assert(entries[i].normal_length < G_MAXINT);
                        *normal = (char*)g_memdup2(entries[i].normal,
                                                   entries[i].normal_length);
                } else {
                        *normal_length = strlen(entries[i].normal);
                        *normal = g_strdup(entries[i].normal);
                }
                _bte_keymap_key_add_key_modifiers(keyval,
                                                  modifiers,
                                                  cursor_mode & cursor_app,
                                                  normal,
                                                  normal_length);
                _bte_debug_print(BTE_DEBUG_KEYBOARD,
                                 " to '%s'.\n",
                                 _bte_debug_sequence_to_string(*normal, *normal_length));
                return;
	}

	_bte_debug_print(BTE_DEBUG_KEYBOARD,
			" (ignoring, no match for modifier state).\n");
}

gboolean
_bte_keymap_key_is_modifier(guint keyval)
{
	gboolean modifier = FALSE;
	/* Determine if this is just a modifier key. */
	switch (keyval) {
	case CDK_KEY_Alt_L:
	case CDK_KEY_Alt_R:
	case CDK_KEY_Caps_Lock:
	case CDK_KEY_Control_L:
	case CDK_KEY_Control_R:
	case CDK_KEY_Eisu_Shift:
	case CDK_KEY_Hyper_L:
	case CDK_KEY_Hyper_R:
	case CDK_KEY_ISO_First_Group:
	case CDK_KEY_ISO_First_Group_Lock:
	case CDK_KEY_ISO_Group_Latch:
	case CDK_KEY_ISO_Group_Lock:
	case CDK_KEY_ISO_Group_Shift:
	case CDK_KEY_ISO_Last_Group:
	case CDK_KEY_ISO_Last_Group_Lock:
	case CDK_KEY_ISO_Level2_Latch:
	case CDK_KEY_ISO_Level3_Latch:
	case CDK_KEY_ISO_Level3_Lock:
	case CDK_KEY_ISO_Level3_Shift:
	case CDK_KEY_ISO_Level5_Latch:
	case CDK_KEY_ISO_Level5_Lock:
	case CDK_KEY_ISO_Level5_Shift:
	case CDK_KEY_ISO_Lock:
	case CDK_KEY_ISO_Next_Group:
	case CDK_KEY_ISO_Next_Group_Lock:
	case CDK_KEY_ISO_Prev_Group:
	case CDK_KEY_ISO_Prev_Group_Lock:
	case CDK_KEY_Kana_Lock:
	case CDK_KEY_Kana_Shift:
	case CDK_KEY_Meta_L:
	case CDK_KEY_Meta_R:
        case CDK_KEY_ModeLock:
	case CDK_KEY_Num_Lock:
	case CDK_KEY_Scroll_Lock:
	case CDK_KEY_Shift_L:
	case CDK_KEY_Shift_Lock:
	case CDK_KEY_Shift_R:
	case CDK_KEY_Super_L:
	case CDK_KEY_Super_R:
		modifier = TRUE;
		break;
	default:
		modifier = FALSE;
		break;
	}
	return modifier;
}

static enum _bte_modifier_encoding_method
_bte_keymap_key_get_modifier_encoding_method(guint keyval)
{
	enum _bte_modifier_encoding_method method = MODIFIER_ENCODING_NONE;
	/* Determine if this key gets modifiers. */
	switch (keyval) {
	case CDK_KEY_Up:
	case CDK_KEY_Down:
	case CDK_KEY_Left:
	case CDK_KEY_Right:
	case CDK_KEY_Insert:
	case CDK_KEY_Delete:
	case CDK_KEY_Home:
	case CDK_KEY_End:
	case CDK_KEY_Page_Up:
	case CDK_KEY_Page_Down:
	case CDK_KEY_KP_Up:
	case CDK_KEY_KP_Down:
	case CDK_KEY_KP_Left:
	case CDK_KEY_KP_Right:
	case CDK_KEY_KP_Insert:
	case CDK_KEY_KP_Delete:
	case CDK_KEY_KP_Home:
	case CDK_KEY_KP_End:
	case CDK_KEY_KP_Page_Up:
	case CDK_KEY_KP_Page_Down:
	case CDK_KEY_KP_Begin:
	case CDK_KEY_F1:
	case CDK_KEY_F2:
	case CDK_KEY_F3:
	case CDK_KEY_F4:
	case CDK_KEY_F5:
	case CDK_KEY_F6:
	case CDK_KEY_F7:
	case CDK_KEY_F8:
	case CDK_KEY_F9:
	case CDK_KEY_F10:
	case CDK_KEY_F11:
	case CDK_KEY_F12:
	case CDK_KEY_F13:
	case CDK_KEY_F14:
	case CDK_KEY_F15:
	case CDK_KEY_F16:
	case CDK_KEY_F17:
	case CDK_KEY_F18:
	case CDK_KEY_F19:
	case CDK_KEY_F20:
	case CDK_KEY_F21:
	case CDK_KEY_F22:
	case CDK_KEY_F23:
	case CDK_KEY_F24:
	case CDK_KEY_F25:
	case CDK_KEY_F26:
	case CDK_KEY_F27:
	case CDK_KEY_F28:
	case CDK_KEY_F29:
	case CDK_KEY_F30:
	case CDK_KEY_F31:
	case CDK_KEY_F32:
	case CDK_KEY_F33:
	case CDK_KEY_F34:
	case CDK_KEY_F35:
		method = MODIFIER_ENCODING_LONG;
		break;
	case CDK_KEY_KP_Divide:
	case CDK_KEY_KP_Multiply:
	case CDK_KEY_KP_Subtract:
	case CDK_KEY_KP_Add:
	case CDK_KEY_KP_Enter:
		method = MODIFIER_ENCODING_SHORT;
		break;
	default:
		method = MODIFIER_ENCODING_NONE;
		break;
	}
	return method;
}

/* Prior and Next are ommitted for the SS3 to CSI switch below */
static gboolean
is_cursor_key(guint keyval)
{
	switch (keyval) {
	case CDK_KEY_Home:
	case CDK_KEY_Left:
	case CDK_KEY_Up:
	case CDK_KEY_Right:
	case CDK_KEY_Down:
	case CDK_KEY_End:
        case CDK_KEY_Begin:

	case CDK_KEY_KP_Home:
	case CDK_KEY_KP_Left:
	case CDK_KEY_KP_Up:
	case CDK_KEY_KP_Right:
	case CDK_KEY_KP_Down:
	case CDK_KEY_KP_End:
	case CDK_KEY_KP_Begin:
		return TRUE;
	default:
		return FALSE;
	}
}


void
_bte_keymap_key_add_key_modifiers(guint keyval,
				  guint modifiers,
				  gboolean cursor_app_mode,
				  char **normal,
				  gsize *normal_length)
{
	int modifier;
	char *nnormal;
	enum _bte_modifier_encoding_method modifier_encoding_method;
	guint significant_modifiers;

	significant_modifiers = CDK_SHIFT_MASK |
				CDK_CONTROL_MASK |
				BTE_ALT_MASK;

	modifier_encoding_method = _bte_keymap_key_get_modifier_encoding_method(keyval);
	if (modifier_encoding_method == MODIFIER_ENCODING_NONE) {
		return;
	}

	switch (modifiers & significant_modifiers) {
	case 0:
		modifier = 0;
		break;
	case CDK_SHIFT_MASK:
		modifier = 2;
		break;
	case BTE_ALT_MASK:
		modifier = 3;
		break;
	case CDK_SHIFT_MASK | BTE_ALT_MASK:
		modifier = 4;
		break;
	case CDK_CONTROL_MASK:
		modifier = 5;
		break;
	case CDK_SHIFT_MASK | CDK_CONTROL_MASK:
		modifier = 6;
		break;
	case BTE_ALT_MASK | CDK_CONTROL_MASK:
		modifier = 7;
		break;
	case CDK_SHIFT_MASK | BTE_ALT_MASK | CDK_CONTROL_MASK:
		modifier = 8;
		break;
	default:
		modifier = 8;
		break;
	}

	if (modifier == 0) {
		return;
	}

	nnormal = g_new0(char, *normal_length + 4);
	memcpy(nnormal, *normal, *normal_length);
	if (strlen(nnormal) > 1) {
		/* SS3 should have no modifiers so make it CSI instead. See
		 * http://cvsweb.xfree86.org/cvsweb/xc/programs/xterm/input.c.diff?r1=3.57&r2=3.58
		 */
		if (cursor_app_mode &&
			g_str_has_prefix(nnormal, _BTE_CAP_SS3)
			&& is_cursor_key(keyval)) {
			nnormal[1] = '[';
		}

		/* Get the offset of the last character. */
		int offset = strlen(nnormal) - 1;
		if (g_ascii_isdigit(nnormal[offset - 1])) {
			/* Stuff a semicolon and the modifier in right before
			 * that last character. */
			nnormal[offset + 2] = nnormal[offset];
			nnormal[offset + 1] = modifier + '0';
			nnormal[offset + 0] = ';';
			*normal_length += 2;
		} else if (modifier_encoding_method == MODIFIER_ENCODING_LONG) {
			/* Stuff a "1", a semicolon and the modifier in right
			 * before that last character, matching Xterm most of the time. */
			nnormal[offset + 3] = nnormal[offset];
			nnormal[offset + 2] = modifier + '0';
			nnormal[offset + 1] = ';';
			nnormal[offset + 0] = '1';
			*normal_length += 3;
		} else {
			/* Stuff the modifier in right before that last
			 * character, matching what people expect,
			 * and what Xterm does with numpad math operators */
			nnormal[offset + 1] = nnormal[offset];
			nnormal[offset + 0] = modifier + '0';
			*normal_length += 1;
		}
		g_free(*normal);
		*normal = nnormal;
	} else {
		g_free(nnormal);
	}
}
