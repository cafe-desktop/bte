/*
 * Copyright © 2015 Christian Persch
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

#include "regex.hh"

static inline auto wrapper_from_regex(bte::base::Regex* regex)
{
        return reinterpret_cast<BteRegex*>(regex);
}

static inline auto regex_from_wrapper(BteRegex* regex)
{
        return reinterpret_cast<bte::base::Regex*>(regex);
}

static inline auto regex_array_from_wrappers(BteRegex** regexes)
{
        return const_cast<bte::base::Regex const**>(reinterpret_cast<bte::base::Regex**>(regexes));
}

bool _bte_regex_has_purpose(BteRegex* regex,
                            bte::base::Regex::Purpose purpose);

bool _bte_regex_has_multiline_compile_flag(BteRegex* regex);
