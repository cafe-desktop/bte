/*
 * Copyright © 2019 Christian Persch
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

#include <memory>

#include <unicode/errorcode.h>
#include <unicode/ucnv.h>

namespace bte::base {

/*
 * bte::base::Decoder:
 *
 * Converts input from any ICU-supported charset to UTF-32, one input byte at a time.
 */
class ICUDecoder {
public:

        using converter_shared_type = std::shared_ptr<UConverter>;

        ICUDecoder(converter_shared_type charset_converter,
                   converter_shared_type u32_converter)
                : m_charset_converter{charset_converter},
                  m_u32_converter{u32_converter}
        { }

        ~ICUDecoder() noexcept { }

        ICUDecoder(ICUDecoder const&) = delete;
        ICUDecoder(ICUDecoder&&) = delete;

        ICUDecoder& operator=(ICUDecoder const&) = delete;
        ICUDecoder& operator=(ICUDecoder&&) = delete;

        /*
         * eNothing: there is no output character available
         * eSomething: there is an output character available
         * eError: an error occurred; you must call reset()
         */
        enum class Result {
                eNothing   = 0,
                eSomething = 1,
                eError     = 2,
        };

        constexpr auto const& error() const noexcept { return m_err; }

        constexpr auto codepoint() const noexcept { return m_u32_buffer[m_index]; }

        Result decode(uint8_t const** sptr,
                      bool flush = false) noexcept;

        void reset() noexcept;

private:
        enum class State {
                eInput  = 0,
                eOutput = 1,
                eError  = 2,
        };

        State m_state{State::eInput};

        converter_shared_type m_charset_converter;
        converter_shared_type m_u32_converter;

        icu::ErrorCode m_err{};

        int m_available{0}; /* how many output characters are available */
        int m_index{0};     /* index of current output character in m_u32_buffer */

        /* 32 is large enough to avoid UCNV_EXT_MAX_UCHARS and UCNV_ERROR_BUFFER_LENGTH,
         * see comment in icu4c/source/common/ucnv.cpp:ucnv_convertEx().
         */
        char32_t m_u32_buffer[32];
        char16_t m_u16_buffer[32];

        constexpr auto u16_buffer() noexcept { return &m_u16_buffer[0]; }
        constexpr auto u32_buffer() noexcept { return &m_u32_buffer[0]; }

        constexpr auto u16_buffer_end() const noexcept { return &m_u16_buffer[0] + 32; }
        constexpr auto u32_buffer_end() const noexcept { return &m_u32_buffer[0] + 32; }

}; // class ICUDecoder

} // namespace bte::base
