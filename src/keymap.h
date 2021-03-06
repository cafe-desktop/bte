/*
 * Copyright (C) 2002 Red Hat, Inc.
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

#ifndef bte_keymap_h_included
#define bte_keymap_h_included

#include <glib.h>
#include <cdk/cdk.h>

G_BEGIN_DECLS

#define BTE_ALT_MASK		CDK_MOD1_MASK
#define BTE_NUMLOCK_MASK	CDK_MOD2_MASK

/* Map the specified keyval/modifier setup, dependent on the mode, to either
 * a literal string or a capability name. */
void _bte_keymap_map(guint keyval,
		     guint modifiers,
		     gboolean app_cursor_keys,
		     gboolean app_keypad_keys,
		     char **normal,
		     gsize *normal_length);

/* Return TRUE if a keyval is just a modifier key. */
gboolean _bte_keymap_key_is_modifier(guint keyval);

/* Add modifiers to the sequence if they're needed. */
void _bte_keymap_key_add_key_modifiers(guint keyval,
				       guint modifiers,
				       gboolean app_cursor_keys,
				       char **normal,
				       gsize *normal_length);

G_END_DECLS

#endif
