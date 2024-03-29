/*
 * Copyright © 2017, 2018 Christian Persch
 *
 * This programme is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This programme is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <glib.h>
#include <locale.h>
#include <unistd.h>
#include <fcntl.h>

#include <cassert>
#include <cstring>
#include <cerrno>
#include <cstdio>
#include <cstdlib>

#include <string>

#include "debug.h"
#include "glib-glue.hh"
#include "libc-glue.hh"
#include "parser.hh"
#include "parser-glue.hh"
#include "utf8.hh"

enum {
#define _BTE_SGR(...)
#define _BTE_NGR(name, value) BTE_SGR_##name = value,
#include "parser-sgr.hh"
#undef _BTE_SGR
#undef _BTE_NGR
};

using namespace std::literals;

char*
bte::parser::Sequence::ucs4_to_utf8(gunichar const* str,
                                    ssize_t len) const noexcept
{
        return g_ucs4_to_utf8(str, len, nullptr, nullptr, nullptr);
}

static constexpr char const*
seq_to_str(unsigned int type) noexcept
{
        switch (type) {
        case BTE_SEQ_NONE: return "NONE";
        case BTE_SEQ_IGNORE: return "IGNORE";
        case BTE_SEQ_GRAPHIC: return "GRAPHIC";
        case BTE_SEQ_CONTROL: return "CONTROL";
        case BTE_SEQ_ESCAPE: return "ESCAPE";
        case BTE_SEQ_CSI: return "CSI";
        case BTE_SEQ_DCS: return "DCS";
        case BTE_SEQ_OSC: return "OSC";
        case BTE_SEQ_SCI: return "SCI";
        case BTE_SEQ_APC: return "APC";
        case BTE_SEQ_PM: return "PM";
        case BTE_SEQ_SOS: return "SOS";
        default:
                assert(false);
        }
}

static constexpr char const*
cmd_to_str(unsigned int command) noexcept
{
        switch (command) {
#define _BTE_CMD(cmd) case BTE_CMD_##cmd: return #cmd;
#define _BTE_NOP(cmd) _BTE_CMD(cmd)
#include "parser-cmd.hh"
#undef _BTE_CMD
#undef _BTE_NOP
        default:
                return nullptr;
        }
}

#if 0
static constexepr char const*
charset_alias_to_str(unsigned int cs) noexcept
{
        switch (cs) {
#define _BTE_CHARSET_PASTE(name)
#define _BTE_CHARSET(name) _BTE_CHARSET_PASTE(name)
#define _BTE_CHARSET_ALIAS_PASTE(name1,name2) case BTE_CHARSET_##name1: return #name1 "(" ## #name2 ## ")";
#define _BTE_CHARSET_ALIAS(name1,name2)
#include "parser-charset.hh"
#undef _BTE_CHARSET_PASTE
#undef _BTE_CHARSET
#undef _BTE_CHARSET_ALIAS_PASTE
#undef _BTE_CHARSET_ALIAS
        default:
                return nullptr; /* not an alias */
        }
}

static constexpr char const*
charset_to_str(unsigned int cs) noexcept
{
        auto alias = charset_alias_to_str(cs);
        if (alias)
                return alias;

        switch (cs) {
#define _BTE_CHARSET_PASTE(name) case BTE_CHARSET_##name: return #name;
#define _BTE_CHARSET(name) _BTE_CHARSET_PASTE(name)
#define _BTE_CHARSET_ALIAS_PASTE(name1,name2)
#define _BTE_CHARSET_ALIAS(name1,name2)
#include "parser-charset.hh"
#undef _BTE_CHARSET_PASTE
#undef _BTE_CHARSET
#undef _BTE_CHARSET_ALIAS_PASTE
#undef _BTE_CHARSET_ALIAS
        default:
                static char buf[32];
                snprintf(buf, sizeof(buf), "UNKOWN(%u)", cs);
                return buf;
        }
}
#endif

class PrettyPrinter {
private:
        std::string m_str;
        bool m_plain;
        bool m_codepoints;

        inline constexpr bool plain() const noexcept { return m_plain; }

        class Attribute {
        public:
                Attribute(PrettyPrinter* printer,
                          std::string const& intro,
                          std::string const& outro) noexcept
                        : m_printer{printer}
                        , m_outro{outro} {
                        if (!m_printer->plain())
                                m_printer->m_str.append(intro);
                        }

                ~Attribute() noexcept
                {
                        if (!m_printer->plain())
                                m_printer->m_str.append(m_outro);
                }

        private:
                PrettyPrinter* m_printer;
                std::string m_outro;
        }; // class Attribute

        class ReverseAttr : private Attribute {
        public:
                ReverseAttr(PrettyPrinter* printer)
                        : Attribute(printer, "\e[7m"s, "\e[27m"s)
                { }
        };

        class RedAttr : private Attribute {
        public:
                RedAttr(PrettyPrinter* printer)
                        : Attribute(printer, "\e[7;31m"s, "\e[27;39m"s)
                { }
        };

        void
        print_params(bte::parser::Sequence const& seq) noexcept
        {
                auto const size = seq.size();
                if (size > 0)
                        m_str.push_back(' ');

                for (unsigned int i = 0; i < size; i++) {
                        if (!seq.param_default(i))
                                print_format("%d", seq.param(i));
                        if (i + 1 < size)
                                m_str.push_back(seq.param_nonfinal(i) ? ':' : ';');
                }
        }

        void
        print_pintro(bte::parser::Sequence const& seq) noexcept
        {
                auto const type = seq.type();
                if (type != BTE_SEQ_CSI &&
                    type != BTE_SEQ_DCS)
                        return;

                auto const p = seq.intermediates() & 0x7;
                if (p == 0)
                        return;

                m_str.push_back(' ');
                m_str.push_back(char(0x40 - p));
        }

        void
        print_intermediates(bte::parser::Sequence const& seq) noexcept
        {
                auto const type = seq.type();
                auto intermediates = seq.intermediates();
                if (type == BTE_SEQ_CSI ||
                    type == BTE_SEQ_DCS)
                        intermediates = intermediates >> 3; /* remove pintro */

                while (intermediates != 0) {
                        unsigned int i = intermediates & 0x1f;
                        char c = 0x20 + i - 1;

                        m_str.push_back(' ');
                        if (c == 0x20)
                                m_str.append("SP"s);
                        else
                                m_str.push_back(c);

                        intermediates = intermediates >> 5;
                }
        }

        void
        print_unichar(uint32_t c) noexcept
        {
                char buf[7];
                auto len = g_unichar_to_utf8(c, buf);
                m_str.append(buf, len);
        }

        G_GNUC_PRINTF(2, 3)
        void
        print_format(char const* format,
                     ...)
        {
                char buf[256];
                va_list args;
                va_start(args, format);
                auto len = g_vsnprintf(buf, sizeof(buf), format, args);
                va_end(args);

                m_str.append(buf, len);
        }

        void
        print_string(bte::parser::Sequence const& seq) noexcept
        {
                auto u8str = seq.string_param();

                m_str.push_back('\"');
                m_str.append(u8str);
                m_str.push_back('\"');

                g_free(u8str);
        }

        void
        print_seq_and_params(bte::parser::Sequence const& seq) noexcept
        {
                ReverseAttr attr(this);

                if (seq.command() != BTE_CMD_NONE) {
                        m_str.push_back('{');
                        m_str.append(cmd_to_str(seq.command()));
                        print_params(seq);
                        m_str.push_back('}');
                } else {
                        m_str.push_back('{');
                        m_str.append(seq_to_str(seq.type()));
                        print_pintro(seq);
                        print_params(seq);
                        print_intermediates(seq);
                        m_str.push_back(' ');
                        m_str.push_back(seq.terminator());
                        m_str.push_back('}');
                }
        }

        void
        print_seq(bte::parser::Sequence const& seq) noexcept
        {
                switch (seq.type()) {
                case BTE_SEQ_NONE: {
                        RedAttr attr(this);
                        m_str.append("{NONE}"s);
                        break;
                }

                case BTE_SEQ_IGNORE: {
                        RedAttr attr(this);
                        m_str.append("{IGNORE}"s);
                        break;
                }

                case BTE_SEQ_GRAPHIC: {
                        auto const terminator = seq.terminator();
                        bool const printable = g_unichar_isprint(terminator);
                        if (m_codepoints || !printable) {
                                if (printable) {
                                        char ubuf[7];
                                        ubuf[g_unichar_to_utf8(terminator, ubuf)] = 0;
                                        print_format("[%04X %s]", terminator, ubuf);
                                } else {
                                        print_format("[%04X]", terminator);
                                }
                        } else {
                                print_unichar(terminator);
                        }
                        break;
                }

                case BTE_SEQ_CONTROL:
                case BTE_SEQ_ESCAPE: {
                        ReverseAttr attr(this);
                        print_format("{%s}", cmd_to_str(seq.command()));
                        break;
                }

                case BTE_SEQ_CSI:
                case BTE_SEQ_DCS: {
                        print_seq_and_params(seq);
                        break;
                }

                case BTE_SEQ_OSC: {
                        ReverseAttr attr(this);
                        m_str.append("{OSC "s);
                        print_string(seq);
                        m_str.push_back('}');
                        break;
                }

                case BTE_SEQ_SCI: {
                        auto const terminator = seq.terminator();
                        if (terminator <= 0x20)
                                print_format("{SCI %d/%d}",
                                             terminator / 16,
                                             terminator % 16);
                        else
                                print_format("{SCI %c}", terminator);
                        break;
                }

                default:
                        assert(false);
                }
        }

        void
        printout() noexcept
        {
                m_str.push_back('\n');
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
                write(STDOUT_FILENO, m_str.data(), m_str.size());
#pragma GCC diagnostic pop
                m_str.clear();
        }

public:

        PrettyPrinter(bool plain,
                      bool codepoints) noexcept
                : m_plain{plain}
                , m_codepoints{codepoints}
        {
        }

        ~PrettyPrinter() noexcept
        {
                printout();
        }

        void operator()(bte::parser::Sequence const& seq) noexcept
        {
                print_seq(seq);
                if (seq.command() == BTE_CMD_LF)
                        printout();
        }

}; // class PrettyPrinter

class Linter {
private:
        G_GNUC_PRINTF(2, 3)
        void
        warn(char const* format,
             ...) const noexcept
        {
                va_list args;
                va_start(args, format);
                char* str = g_strdup_vprintf(format, args);
                va_end(args);
                g_printerr("WARNING: %s\n", str);
                g_free(str);
        }

        void
        warn_deprecated(int cmd,
                        int replacement_cmd) const noexcept
        {
                warn("%s is deprecated; use %s instead",
                     cmd_to_str(cmd),
                     cmd_to_str(replacement_cmd));
        }

        void
        check_sgr_number(int sgr) noexcept
        {
                switch (sgr) {
                case -1:
#define _BTE_SGR(name, value) case value:
#define _BTE_NGR(...)
#include "parser-sgr.hh"
#undef _BTE_SGR
#undef _BTE_NGR
                case BTE_SGR_SET_FORE_LEGACY_START+1 ... BTE_SGR_SET_FORE_LEGACY_END-1:
                case BTE_SGR_SET_FORE_LEGACY_BRIGHT_START+1 ... BTE_SGR_SET_FORE_LEGACY_BRIGHT_END-1:
                case BTE_SGR_SET_BACK_LEGACY_START+1 ... BTE_SGR_SET_BACK_LEGACY_END-1:
                case BTE_SGR_SET_BACK_LEGACY_BRIGHT_START+1 ... BTE_SGR_SET_BACK_LEGACY_BRIGHT_END-1:
                        break;

#define _BTE_SGR(...)
#define _BTE_NGR(name, value) case value:
#include "parser-sgr.hh"
#undef _BTE_SGR
#undef _BTE_NGR
                case BTE_SGR_SET_FONT_FIRST+1 ... BTE_SGR_SET_FONT_LAST-1:
                        warn("SGR %d is unsupported", sgr);
                        break;

                default:
                        warn("SGR %d is unknown", sgr);
                        break;

                }
        }

        void
        check_sgr_color(bte::parser::Sequence const& seq,
                        unsigned int& idx) noexcept
        {
                auto const sgr = seq.param(idx);

                /* Simplified and adapted from Terminal::seq_parse_sgr_color() */
                if (seq.param_nonfinal(idx)) {
                        /* Colon version */
                        auto const param = seq.param(++idx);
                        switch (param) {
                        case 2: {
                                auto const n = seq.next(idx) - idx;
                                if (n < 4)
                                        warn("SGR %d:2 not enough parameters", sgr);
                                else if (n == 4)
                                        warn("SGR %d:2:r:g:b is deprecated; use SGR %d:2::r:g:b instead",
                                             sgr, sgr);
                                break;
                        }
                        case 5: {
                                auto const n = seq.next(idx) - idx;
                                if (n < 2)
                                        warn("SGR %d:5 not enough parameters", sgr);
                                break;
                        }
                        case -1:
                                warn("SGR %d does not admit default parameters", sgr);
                                break;
                        case 0:
                        case 1:
                        case 3:
                        case 4:
                                warn("SGR %d:%d is unsupported", sgr, param);
                                break;
                        default:
                                warn("SGR %d:%d is unknown", sgr, param);
                        }
                } else {
                        /* Semicolon version */
                        idx = seq.next(idx);
                        auto const param = seq.param(idx);
                        switch (param) {
                        case 2:
                                /* Consume 3 more parameters */
                                idx = seq.next(idx);
                                idx = seq.next(idx);
                                idx = seq.next(idx);
                                warn("SGR %d;%d;r;g;b is deprecated; use SGR %d:%d::r:g:b instead",
                                     sgr, param, sgr, param);
                                break;
                        case 5:
                                /* Consume 1 more parameter */
                                idx = seq.next(idx);
                                warn("SGR %d;%d;index is deprecated; use SGR %d:%d:index instead",
                                     sgr, param, sgr, param);
                                break;
                        case -1:
                                warn("SGR %d does not admit default parameters", sgr);
                                break;
                        case 0:
                        case 1:
                        case 3:
                        case 4:
                                warn("SGR %d;%d;... is unsupported; use SGR %d:%d:... instead",
                                     sgr, param, sgr, param);
                                break;
                        default:
                                warn("SGR %d;%d is unknown", sgr, param);
                                break;
                        }
                }
        }

        void
        check_sgr_underline(bte::parser::Sequence const& seq,
                            unsigned int idx) noexcept
        {
                auto const sgr = seq.param(idx);

                int param = 1;
                /* If we have a subparameter, get it */
                if (seq.param_nonfinal(idx))
                        param = seq.param(idx + 1);

                switch (param) {
                case -1:
                case 0:
                case 1:
                case 2:
                case 3:
                        break;
                case 4:
                case 5:
                        warn("SGR %d:%d is unsupported", sgr, param);
                        break;
                default:
                        warn("SGR %d:%d is unknown", sgr, param);
                        break;
                }
        }

        void
        check_sgr(bte::parser::Sequence const& seq) noexcept
        {
                for (unsigned int i = 0; i < seq.size(); i = seq.next(i)) {
                        auto const param = seq.param(i, 0);

                        check_sgr_number(param);

                        switch (param) {
                        case BTE_SGR_SET_UNDERLINE:
                                check_sgr_underline(seq, i);
                                break;

                        case BTE_SGR_SET_FORE_SPEC:
                        case BTE_SGR_SET_BACK_SPEC:
                        case BTE_SGR_SET_DECO_SPEC:
                                check_sgr_color(seq, i);
                                break;

                        default:
                                if (seq.param_nonfinal(i))
                                        warn("SGR %d does not admit subparameters", param);
                                break;
                        }
                }
        }

public:
        constexpr Linter() noexcept = default;
        ~Linter() noexcept = default;

        void operator()(bte::parser::Sequence const& seq) noexcept
        {
                auto cmd = seq.command();
                switch (cmd) {
                case BTE_CMD_OSC:
                        if (seq.terminator() == 7 /* BEL */)
                                warn("OSC terminated by BEL may be ignored; use ST (ESC \\) instead.");
                        break;

                case BTE_CMD_DECSLRM_OR_SCOSC:
                        cmd = BTE_CMD_SCOSC;
                        [[fallthrough]];
                case BTE_CMD_SCOSC:
                        warn_deprecated(cmd, BTE_CMD_DECSC);
                        break;

                case BTE_CMD_SCORC:
                        warn_deprecated(cmd, BTE_CMD_DECRC);
                        break;

                case BTE_CMD_SGR:
                        check_sgr(seq);
                        break;

                default:
                        if (cmd >= BTE_CMD_NOP_FIRST)
                                warn("%s is unimplemented", cmd_to_str(cmd));
                        break;
                }
        }

}; // class Linter

class Sink {
public:
        void operator()(bte::parser::Sequence const& seq) noexcept { }

}; // class Sink

class Processor {
private:
        gsize m_seq_stats[BTE_SEQ_N];
        gsize m_cmd_stats[BTE_CMD_N];
        GArray* m_bench_times;

        template<class Functor>
        void
        process_file_utf8(int fd,
                          Functor& func)
        {
                bte::parser::Parser parser{};
                bte::parser::Sequence seq{parser};

                gsize const buf_size = 16384;
                guchar* buf = g_new0(guchar, buf_size);

                auto start_time = g_get_monotonic_time();

                bte::base::UTF8Decoder decoder;

                gsize buf_start = 0;
                for (;;) {
                        auto len = read(fd, buf + buf_start, buf_size - buf_start);
                        if (!len)
                                break;
                        if (len == -1) {
                                if (errno == EAGAIN)
                                        continue;
                                break;
                        }

                        auto const bufend = buf + len;
                        for (auto sptr = buf; sptr < bufend; ++sptr) {
                                switch (decoder.decode(*sptr)) {
                                case bte::base::UTF8Decoder::REJECT_REWIND:
                                        /* Rewind the stream.
                                         * Note that this will never lead to a loop, since in the
                                         * next round this byte *will* be consumed.
                                         */
                                        --sptr;
                                        [[fallthrough]];
                                case bte::base::UTF8Decoder::REJECT:
                                        decoder.reset();
                                        /* Fall through to insert the U+FFFD replacement character. */
                                        [[fallthrough]];
                                case bte::base::UTF8Decoder::ACCEPT: {
                                        auto ret = parser.feed(decoder.codepoint());
                                        if (G_UNLIKELY(ret < 0)) {
                                                g_printerr("Parser error!\n");
                                                goto out;
                                        }

                                        m_seq_stats[ret]++;
                                        if (ret != BTE_SEQ_NONE) {
                                                m_cmd_stats[seq.command()]++;
                                                func(seq);
                                        }
                                        break;
                                }

                                default:
                                        break;
                                }
                        }
                }

        out:

                int64_t time_spent = g_get_monotonic_time() - start_time;
                g_array_append_val(m_bench_times, time_spent);

                g_free(buf);
        }

        template<class Functor>
        bool
        process_file(int fd,
                     int repeat,
                     Functor& func)
        {
                if (fd == STDIN_FILENO && repeat != 1) {
                        g_printerr("Cannot consume STDIN more than once\n");
                        return false;
                }

                for (auto i = 0; i < repeat; ++i) {
                        if (i > 0 && lseek(fd, 0, SEEK_SET) != 0) {
                                auto errsv = bte::libc::ErrnoSaver{};
                                g_printerr("Failed to seek: %s\n", g_strerror(errsv));
                                return false;
                        }

                        process_file_utf8(fd, func);
                }

                return true;
        }

public:

        Processor() noexcept
        {
                memset(&m_seq_stats, 0, sizeof(m_seq_stats));
                memset(&m_cmd_stats, 0, sizeof(m_cmd_stats));
                m_bench_times = g_array_new(false, true, sizeof(int64_t));
        }

        ~Processor() noexcept
        {
                g_array_free(m_bench_times, true);
        }

        template<class Functor>
        bool
        process_files(char const* const* filenames,
                      int repeat,
                      Functor& func)
        {
                bool r = true;
                if (filenames != nullptr) {
                        for (auto i = 0; filenames[i] != nullptr; i++) {
                                char const* filename = filenames[i];

                                int fd = -1;
                                if (g_str_equal(filename, "-")) {
                                        fd = STDIN_FILENO;
                                } else {
                                        fd = open(filename, O_RDONLY);
                                        if (fd == -1) {
                                                auto errsv = bte::libc::ErrnoSaver{};
                                                g_printerr("Error opening file %s: %s\n",
                                                           filename, g_strerror(errsv));
                                        }
                                }
                                if (fd != -1) {
                                        r = process_file(fd, repeat, func);
                                        if (fd != STDIN_FILENO)
                                                close(fd);
                                        if (!r)
                                                break;
                                }
                        }
                } else {
                        r = process_file(STDIN_FILENO, repeat, func);
                }

                return r;
        }

        void print_statistics() const noexcept
        {
                for (unsigned int s = BTE_SEQ_NONE + 1; s < BTE_SEQ_N; s++) {
                        g_printerr("%\'16" G_GSIZE_FORMAT " %s\n",  m_seq_stats[s], seq_to_str(s));
                }

                g_printerr("\n");
                for (unsigned int s = 0; s < BTE_CMD_N; s++) {
                        if (m_cmd_stats[s] > 0) {
                                g_printerr("%\'16" G_GSIZE_FORMAT " %s%s\n",
                                           m_cmd_stats[s],
                                           cmd_to_str(s),
                                           s >= BTE_CMD_NOP_FIRST ? " [NOP]" : "");
                        }
                }
        }

        void print_benchmark() const noexcept
        {
                g_array_sort(m_bench_times,
                             [](void const* p1, void const* p2) -> int {
                                     int64_t const t1 = *(int64_t const*)p1;
                                     int64_t const t2 = *(int64_t const*)p2;
                                     return t1 == t2 ? 0 : (t1 < t2 ? -1 : 1);
                             });

                int64_t total_time = 0;
                for (unsigned int i = 0; i < m_bench_times->len; ++i)
                        total_time += g_array_index(m_bench_times, int64_t, i);

                g_printerr("\nTimes: best %\'" G_GINT64_FORMAT "µs "
                           "worst %\'" G_GINT64_FORMAT "µs "
                           "average %\'" G_GINT64_FORMAT "µs\n",
                           g_array_index(m_bench_times, int64_t, 0),
                           g_array_index(m_bench_times, int64_t, m_bench_times->len - 1),
                           total_time / (int64_t)m_bench_times->len);
                for (unsigned int i = 0; i < m_bench_times->len; ++i)
                        g_printerr("  %\'" G_GINT64_FORMAT "µs\n",
                                   g_array_index(m_bench_times, int64_t, i));
        }

}; // class Processor

class Options {
private:
        bool m_benchmark{false};
        bool m_codepoints{false};
        bool m_lint{false};
        bool m_plain{false};
        bool m_quiet{false};
        bool m_statistics{false};
        int m_repeat{1};
        char** m_filenames{nullptr};

        template<typename T1, typename T2 = T1>
        class OptionArg {
        private:
                T1* m_return_ptr;
                T2 m_value;
        public:
                OptionArg(T1* ptr, T2 v) : m_return_ptr{ptr}, m_value{v} { }
                ~OptionArg() { *m_return_ptr = m_value; }

                inline constexpr T2* ptr() noexcept { return &m_value; }
        };

        using BoolArg = OptionArg<bool, gboolean>;
        using IntArg = OptionArg<int>;
        using StrvArg = OptionArg<char**>;

public:

        Options() noexcept = default;
        Options(Options const&) = delete;
        Options(Options&&) = delete;

        ~Options() {
                if (m_filenames != nullptr)
                        g_strfreev(m_filenames);
        }

        inline constexpr bool benchmark()  const noexcept { return m_benchmark;  }
        inline constexpr bool codepoints() const noexcept { return m_codepoints; }
        inline constexpr bool lint()       const noexcept { return m_lint;       }
        inline constexpr bool plain()      const noexcept { return m_plain;      }
        inline constexpr bool quiet()      const noexcept { return m_quiet;      }
        inline constexpr bool statistics() const noexcept { return m_statistics; }
        inline constexpr int  repeat()     const noexcept { return m_repeat;     }
        inline constexpr char const* const* filenames() const noexcept { return m_filenames; }

        bool parse(int argc,
                   char* argv[],
                   GError** error) noexcept
        {
                BoolArg benchmark{&m_benchmark, false};
                BoolArg codepoints{&m_codepoints, false};
                BoolArg lint{&m_lint, false};
                BoolArg plain{&m_plain, false};
                BoolArg quiet{&m_quiet, false};
                BoolArg statistics{&m_statistics, false};
                IntArg repeat{&m_repeat, 1};
                StrvArg filenames{&m_filenames, nullptr};
                GOptionEntry const entries[] = {
                        { .long_name = "benchmark",        .short_name = 'b', .flags = 0, .arg = G_OPTION_ARG_NONE,                 .arg_data = benchmark.ptr(),
                          .description = "Measure time spent parsing each file",                .arg_description = nullptr },

                        { .long_name = "codepoints",       .short_name = 'u', .flags = 0, .arg = G_OPTION_ARG_NONE,                 .arg_data = codepoints.ptr(),
                          .description = "Output unicode code points by number",                .arg_description = nullptr },

                        { .long_name = "lint",             .short_name = 'l', .flags = 0, .arg = G_OPTION_ARG_NONE,                 .arg_data = lint.ptr(),
                          .description = "Check input",                                         .arg_description = nullptr },

                        { .long_name = "plain",            .short_name = 'p', .flags = 0, .arg = G_OPTION_ARG_NONE,                 .arg_data = plain.ptr(),
                          .description = "Output plain text without attributes",                .arg_description = nullptr },

                        { .long_name = "quiet",            .short_name = 'q', .flags = 0, .arg = G_OPTION_ARG_NONE,                 .arg_data = quiet.ptr(),
                          .description = "Suppress output except for statistics and benchmark", .arg_description = nullptr },

                        { .long_name = "repeat",           .short_name = 'r', .flags = 0,       .arg = G_OPTION_ARG_INT,            .arg_data = repeat.ptr(),
                          .description = "Repeat each file COUNT times", .arg_description = "COUNT" },

                        { .long_name = "statistics",       .short_name = 's', .flags = 0,       .arg = G_OPTION_ARG_NONE,           .arg_data = statistics.ptr(),
                          .description = "Output statistics", .arg_description = nullptr },

                        { .long_name = G_OPTION_REMAINING, .short_name = 0,   .flags = 0,       .arg = G_OPTION_ARG_FILENAME_ARRAY, .arg_data = filenames.ptr(),
                          .description = nullptr, .arg_description = nullptr },
                };

                auto context = g_option_context_new("[FILE…] — parser cat");
                g_option_context_set_help_enabled(context, true);
                g_option_context_add_main_entries(context, entries, nullptr);

                bool rv = g_option_context_parse(context, &argc, &argv, error);
                g_option_context_free(context);
                return rv;
        }
}; // class Options

int
main(int argc,
     char *argv[])
{
        setlocale(LC_ALL, "");
        _bte_debug_init();

        Options options{};
        auto error = bte::glib::Error{};
        if (!options.parse(argc, argv, error)) {
                g_printerr("Failed to parse arguments: %s\n", error.message());
                return EXIT_FAILURE;
        }

        bool rv;
        Processor proc{};
        if (options.lint()) {
                Linter linter{};
                rv = proc.process_files(options.filenames(), 1, linter);
        } else if (options.quiet()) {
                Sink sink{};
                rv = proc.process_files(options.filenames(), options.repeat(), sink);
        } else {
                PrettyPrinter pp{options.plain(), options.codepoints()};
                rv = proc.process_files(options.filenames(), options.repeat(), pp);
        }

        if (options.statistics())
                proc.print_statistics();
        if (options.benchmark())
                proc.print_benchmark();

        return rv ? EXIT_SUCCESS : EXIT_FAILURE;
}
