/*
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

/* NOTE: This file must be included *after all other includes*. */

/* NSIG isn't in POSIX, so if it doesn't exist use this here. See bug #759196 */
#ifndef NSIG
#define NSIG (8 * sizeof(sigset_t))
#endif

#ifndef HAVE_FDWALK
int fdwalk(int (*cb)(void* data, int fd),
           void* data);
#endif

#ifndef HAVE_STRCHRNUL
char* strchrnul(char const* s,
                int c);
#endif
