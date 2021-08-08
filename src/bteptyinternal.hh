/*
 * Copyright © 2019 Christian Persch
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

#pragma once

#include "bte/btepty.h"
#include "pty.hh"

bte::base::Pty* _bte_pty_get_impl(BtePty* pty);

bool _bte_pty_spawn_sync(BtePty* pty,
                         char const* working_directory,
                         char const* const* argv,
                         char const* const* envv,
                         GSpawnFlags spawn_flags,
                         GSpawnChildSetupFunc child_setup,
                         gpointer child_setup_data,
                         GDestroyNotify child_setup_data_destroy,
                         GPid* child_pid /* out */,
                         int timeout,
                         GCancellable* cancellable,
                         GError** error) noexcept;

bool _bte_pty_check_envv(char const* const* envv) noexcept;

bool _bte_pty_set_size(BtePty *pty,
                       int rows,
                       int columns,
                       int cell_height_px,
                       int cell_width_px,
                       GError **error) noexcept;
