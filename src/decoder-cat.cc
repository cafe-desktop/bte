/*
 * Copyright © 2017, 2018, 2019 Christian Persch
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

#include <fcntl.h>
#include <locale.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <string>

#include "debug.h"
#include "glib-glue.hh"
#include "libc-glue.hh"
#include "utf8.hh"

#ifdef WITH_ICU
#include "icu-decoder.hh"
#include "icu-glue.hh"
#endif

using namespace std::literals;

class Options {
private:
        bool m_benchmark{false};
        bool m_codepoints{false};
        bool m_list{false};
        bool m_quiet{false};
        bool m_statistics{false};
        bool m_utf8{false};
        int m_repeat{1};
        char* m_charset{nullptr};
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
        using StrArg = OptionArg<char*>;
        using StrvArg = OptionArg<char**>;

public:

        Options() noexcept = default;
        Options(Options const&) = delete;
        Options(Options&&) = delete;

        ~Options() {
                if (m_filenames != nullptr)
                        g_strfreev(m_filenames);
        }

        Options& operator=(Options const&) = delete;
        Options& operator=(Options&&) = delete;

        inline constexpr bool benchmark()  const noexcept { return m_benchmark;  }
        inline constexpr bool codepoints() const noexcept { return m_codepoints; }
        inline constexpr bool list()       const noexcept { return m_list;       }
        inline constexpr bool statistics() const noexcept { return m_statistics; }
        inline constexpr int  quiet()      const noexcept { return m_quiet;      }
        inline constexpr bool utf8()       const noexcept { return m_utf8;       }
        inline constexpr int  repeat()     const noexcept { return m_repeat;     }
        inline constexpr char const* charset()          const noexcept { return m_charset;   }
        inline constexpr char const* const* filenames() const noexcept { return m_filenames; }

        bool parse(int argc,
                   char* argv[],
                   GError** error) noexcept
        {
                {
                        auto benchmark = BoolArg{&m_benchmark, false};
                        auto codepoints = BoolArg{&m_codepoints, false};
                        auto list = BoolArg{&m_list, false};
                        auto quiet = BoolArg{&m_quiet, false};
                        auto statistics = BoolArg{&m_statistics, false};
                        auto utf8 = BoolArg{&m_utf8, false};
                        auto repeat = IntArg{&m_repeat, 1};
                        auto charset = StrArg{&m_charset, nullptr};
                        auto filenames = StrvArg{&m_filenames, nullptr};
                        GOptionEntry const entries[] = {
                                { .long_name = "benchmark",        .short_name = 'b', .flags = 0, .arg = G_OPTION_ARG_NONE,           .arg_data = benchmark.ptr(),
                                  .description = "Measure time spent parsing each file",                .arg_description = nullptr },

                                { .long_name = "codepoints",       .short_name = 'u', .flags = 0, .arg = G_OPTION_ARG_NONE,           .arg_data = codepoints.ptr(),
                                  .description = "Output unicode code points by number",                .arg_description = nullptr },

                                { .long_name = "charset",          .short_name = 'f', .flags = 0, .arg = G_OPTION_ARG_STRING,         .arg_data = charset.ptr(),
                                  .description = "Input charset",                                       .arg_description = "CHARSET" },

                                { .long_name = "list-charsets",    .short_name = 'l', .flags = 0, .arg = G_OPTION_ARG_NONE,           .arg_data = list.ptr(),
                                  .description = "List available charsets",                             .arg_description = nullptr },

                                { .long_name = "quiet",            .short_name = 'q', .flags = 0, .arg = G_OPTION_ARG_NONE,           .arg_data = quiet.ptr(),
                                  .description = "Suppress output except for statistics and benchmark", .arg_description = nullptr },

                                { .long_name = "repeat",           .short_name = 'r', .flags = 0, .arg = G_OPTION_ARG_INT,            .arg_data = repeat.ptr(),
                                  .description = "Repeat each file COUNT times",                        .arg_description = "COUNT" },

                                { .long_name = "statistics",       .short_name = 's', .flags = 0, .arg = G_OPTION_ARG_NONE,           .arg_data = statistics.ptr(),
                                  .description = "Output statistics",                                   .arg_description = nullptr },

                                { .long_name = "utf-8",            .short_name = '8', .flags = 0, .arg = G_OPTION_ARG_NONE,           .arg_data = utf8.ptr(),
                                  .description = "UTF-8 input (default)",                               .arg_description = nullptr },

                                { .long_name = G_OPTION_REMAINING, .short_name = 0,   .flags = 0, .arg = G_OPTION_ARG_FILENAME_ARRAY, .arg_data = filenames.ptr(),
                                  .description = nullptr,                                               .arg_description = nullptr },
                        };

                        auto context = g_option_context_new("[FILE…] — decoder cat");
                        g_option_context_set_help_enabled(context, true);
                        g_option_context_add_main_entries(context, entries, nullptr);

                        auto rv = bool{g_option_context_parse(context, &argc, &argv, error) != false};
                        g_option_context_free(context);
                        if (!rv)
                                return rv;
                }

                return true;
        }
}; // class Options

class Printer {
private:
        std::string m_str{};
        bool m_codepoints{false};

        void
        print(char const* buf,
              size_t len) noexcept
        {
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
                auto const len = g_vsnprintf(buf, sizeof(buf), format, args);
                va_end(args);

                m_str.append(buf, len);
        }

        void
        print_u32(uint32_t const c) noexcept
        {
                char ubuf[7];
                auto const len = g_unichar_to_utf8(c, ubuf);

                if (m_codepoints) {
                        ubuf[len] = 0;
                        if (g_unichar_isprint(c)) {
                                print_format("[%04X %s]", c, ubuf);
                        } else {
                                print_format("[%04X]", c);
                        }
                } else {
                        print(ubuf, len);
                }
        }

        void
        printout(bool force_lf = false) noexcept
        {
                if (m_codepoints || force_lf)
                        m_str.push_back('\n');

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
                write(STDOUT_FILENO, m_str.data(), m_str.size());
#pragma GCC diagnostic pop
                m_str.clear();
        }

        static inline auto const k_LF = uint32_t{0xau};

public:

        Printer(bool codepoints = false) noexcept
                : m_codepoints{codepoints}
        {
        }

        ~Printer() noexcept
        {
                printout(true);
        }

        void operator()(uint32_t const c) noexcept
        {
                print_u32(c);
                if (c == k_LF)
                        printout();
        }

}; // class Printer

class Sink {
public:
        void operator()(uint32_t c) noexcept { }

}; // class Sink

#ifdef WITH_ICU

static std::unique_ptr<bte::base::ICUDecoder>
make_decoder(Options const& options)
{
        auto err = icu::ErrorCode{};

        auto converter = std::shared_ptr<UConverter>{ucnv_open(options.charset(), err), &ucnv_close};
        if (err.isFailure()) {
                if (!options.quiet())
                        g_printerr("Failure to open converter for \"%s\": %s\n",
                                   options.charset(), err.errorName());
                return {};
        }

        if (err.get() == U_AMBIGUOUS_ALIAS_WARNING) {
                err.reset();
                auto canonical = ucnv_getName(converter.get(), err);
                if (err.isSuccess() && !options.quiet())
                        g_printerr("Warning: charset \"%s\" is ambigous alias for \"%s\"\n",
                                   options.charset(), canonical);
        }

        err.reset();
        auto u32_converter = std::shared_ptr<UConverter>{ucnv_open("utf32platformendian", err), &ucnv_close};
        if (err.isFailure()) {
                if (!options.quiet())
                        g_printerr("Failure to open converter for \"%s\": %s\n",
                                   "UTF-32", err.errorName());
                return {};
        }

        return std::make_unique<bte::base::ICUDecoder>(converter, u32_converter);
}

#endif /* WITH_ICU */

class Processor {
private:
        gsize m_input_bytes{0};
        gsize m_output_chars{0};
        gsize m_errors{0};
        GArray* m_bench_times{nullptr};

        template<class Functor>
        void
        process_file_utf8(int fd,
                          Functor& func)
        {
                auto decoder = bte::base::UTF8Decoder{};

                auto const buf_size = size_t{16384};
                auto buf = g_new0(uint8_t, buf_size);

                auto start_time = g_get_monotonic_time();

                auto buf_start = size_t{0};
                for (;;) {
                        auto len = read(fd, buf + buf_start, buf_size - buf_start);
                        if (!len)
                                break;
                        if (len == -1) {
                                if (errno == EAGAIN)
                                        continue;
                                break;
                        }

                        m_input_bytes += len;

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
                                case bte::base::UTF8Decoder::ACCEPT:
                                        func(decoder.codepoint());
                                        m_output_chars++;

                                default:
                                        break;
                                }
                        }
                }

                /* Flush remaining output; at most one character */
                if (decoder.flush()) {
                        func(decoder.codepoint());
                        m_output_chars++;
                }

                auto const time_spent = int64_t{g_get_monotonic_time() - start_time};
                g_array_append_val(m_bench_times, time_spent);

                g_free(buf);
        }

#ifdef WITH_ICU
        template<class Functor>
        void
        process_file_icu(int fd,
                         bte::base::ICUDecoder* decoder,
                         Functor& func)
        {
                decoder->reset();

                auto const buf_size = size_t{16384};
                auto buf = g_new0(uint8_t, buf_size);

                auto start_time = g_get_monotonic_time();

                auto buf_start = size_t{0};
                while (true) {
                        auto len = read(fd, buf + buf_start, buf_size - buf_start);
                        if (!len) /* EOF */
                                break;
                        if (len == -1) {
                                if (errno == EAGAIN)
                                        continue;
                                break;
                        }

                        m_input_bytes += len;

                        auto sptr = reinterpret_cast<uint8_t const*>(buf);
                        auto const sptrend = buf + len;
                        while (sptr < sptrend) {
                                /* Note that rewinding will never lead to an infinite loop,
                                 * since when the decoder runs out of output, this input byte
                                 * *will* be consumed.
                                 */
                                switch (decoder->decode(&sptr)) {
                                case bte::base::ICUDecoder::Result::eSomething:
                                        func(decoder->codepoint());
                                        m_output_chars++;
                                        break;

                                case bte::base::ICUDecoder::Result::eNothing:
                                        break;

                                case bte::base::ICUDecoder::Result::eError:
                                        // FIXMEchpe need do ++sptr here?
                                        m_errors++;
                                        decoder->reset();
                                        break;
                                }
                        }
                }

                /* Flush remaining output */
                auto sptr = reinterpret_cast<uint8_t const*>(buf + buf_size);
                auto result = bte::base::ICUDecoder::Result{};
                while ((result = decoder->decode(&sptr, true)) == bte::base::ICUDecoder::Result::eSomething) {
                        func(decoder->codepoint());
                        m_output_chars++;
                }

                auto const time_spent = int64_t{g_get_monotonic_time() - start_time};
                g_array_append_val(m_bench_times, time_spent);

                g_free(buf);
        }
#endif /* WITH_ICU */

        template<class Functor>
        bool
        process_file(int fd,
                     Options const& options,
                     Functor& func)
        {
#ifdef WITH_ICU
                auto decoder = std::unique_ptr<bte::base::ICUDecoder>{};
                if (options.charset()) {
                        decoder = make_decoder(options);
                        if (!decoder)
                                return false;
                }

                assert(decoder != nullptr || options.charset() == nullptr);
#endif

                for (auto i = 0; i < options.repeat(); ++i) {
                        if (i > 0 && lseek(fd, 0, SEEK_SET) != 0) {
                                auto errsv = bte::libc::ErrnoSaver{};
                                g_printerr("Failed to seek: %s\n", g_strerror(errsv));
                                return false;
                        }

#ifdef WITH_ICU
                        if (decoder) {
                                process_file_icu(fd, decoder.get(), func);
                        } else
#endif
                        {
                                process_file_utf8(fd, func);
                        }
                }

                return true;
        }

public:

        Processor() noexcept
        {
                m_bench_times = g_array_new(false, true, sizeof(int64_t));
        }

        ~Processor() noexcept
        {
                g_array_free(m_bench_times, true);
        }

        template<class Functor>
        bool
        process_files(Options const& options,
                      Functor& func)
        {
                auto r = bool{true};
                if (auto filenames = options.filenames(); filenames != nullptr) {
                        for (auto i = 0; filenames[i] != nullptr; i++) {
                                auto filename = filenames[i];

                                auto fd = int{-1};
                                if (g_str_equal(filename, "-")) {
                                        fd = STDIN_FILENO;

                                        if (options.repeat() != 1) {
                                                g_printerr("Cannot consume STDIN more than once\n");
                                                return false;
                                        }
                                } else {
                                        fd = ::open(filename, O_RDONLY);
                                        if (fd == -1) {
                                                auto errsv = bte::libc::ErrnoSaver{};
                                                g_printerr("Error opening file %s: %s\n",
                                                           filename, g_strerror(errsv));
                                        }
                                }
                                if (fd != -1) {
                                        r = process_file(fd, options, func);
                                        if (fd != STDIN_FILENO)
                                                close(fd);
                                        if (!r)
                                                break;
                                }
                        }
                } else {
                        r = process_file(STDIN_FILENO, options, func);
                }

                return r;
        }

        void print_statistics() const noexcept
        {
                g_printerr("%\'16" G_GSIZE_FORMAT " input bytes produced %\'16" G_GSIZE_FORMAT
                           " unichars and %" G_GSIZE_FORMAT " errors\n",
                           m_input_bytes, m_output_chars, m_errors);
        }

        void print_benchmark() const noexcept
        {
                g_array_sort(m_bench_times,
                             [](void const* p1, void const* p2) -> int {
                                     int64_t const t1 = *(int64_t const*)p1;
                                     int64_t const t2 = *(int64_t const*)p2;
                                     return t1 == t2 ? 0 : (t1 < t2 ? -1 : 1);
                             });

                auto total_time = int64_t{0};
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

// main

int
main(int argc,
     char *argv[])
{
        setlocale(LC_ALL, "");
        _bte_debug_init();

        auto options = Options{};
        auto error = bte::glib::Error{};
        if (!options.parse(argc, argv, error)) {
                g_printerr("Failed to parse arguments: %s\n", error.message());
                return EXIT_FAILURE;
        }

        if (options.list()) {
#ifdef WITH_ICU
                auto charsets = bte::base::get_icu_charsets(true);
                for (auto i = 0; charsets[i]; ++i)
                        g_print("%s\n", charsets[i]);
                g_strfreev(charsets);

                return EXIT_SUCCESS;
#else
                g_printerr("ICU support not available.\n");
                return EXIT_FAILURE;
#endif
        }

        auto rv = bool{};
        auto proc = Processor{};
        if (options.quiet()) {
                auto sink = Sink{};
                rv = proc.process_files(options, sink);
        } else {
                auto printer = Printer{options.codepoints()};
                rv = proc.process_files(options, printer);
        }

        if (options.statistics())
                proc.print_statistics();
        if (options.benchmark())
                proc.print_benchmark();

        return rv ? EXIT_SUCCESS : EXIT_FAILURE;
}
