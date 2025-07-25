/*
 * Copyright © 2001,2002 Red Hat, Inc.
 * Copyright © 2014, 2017 Christian Persch
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <string.h>
#include <locale.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>
#include <glib-unix.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <ctk/ctk.h>
#include <cairo/cairo-gobject.h>
#include <bte/bte.h>
#include "btepcre2.h"

#include <algorithm>
#include <vector>

#include "glib-glue.hh"
#include "libc-glue.hh"
#include "refptr.hh"

/* options */

class Options {
public:
        gboolean allow_window_ops{false};
        gboolean audible_bell{false};
        gboolean backdrop{false};
        gboolean bold_is_bright{false};
        gboolean console{false};
        gboolean debug{false};
        gboolean feed_stdin{false};
        gboolean icon_title{false};
        gboolean keep{false};
        gboolean no_argb_visual{false};
        gboolean no_bidi{false};
        gboolean no_bold{false};
        gboolean no_builtin_dingus{false};
        gboolean no_context_menu{false};
        gboolean no_decorations{false};
        gboolean no_double_buffer{false};
        gboolean no_geometry_hints{false};
        gboolean no_hyperlink{false};
        gboolean no_pty{false};
        gboolean no_rewrap{false};
        gboolean no_scrollbar{false};
        gboolean no_shaping{false};
        gboolean no_shell{false};
        gboolean no_systemd_scope{false};
        gboolean object_notifications{false};
        gboolean require_systemd_scope{false};
        gboolean reverse{false};
        gboolean test_mode{false};
        gboolean use_theme_colors{false};
        gboolean version{false};
        gboolean whole_window_transparent{false};
        bool bg_color_set{false};
        bool fg_color_set{false};
        bool cursor_bg_color_set{false};
        bool cursor_fg_color_set{false};
        bool hl_bg_color_set{false};
        bool hl_fg_color_set{false};
        cairo_extend_t background_extend{CAIRO_EXTEND_NONE};
        char* command{nullptr};
        char* encoding{nullptr};
        char* font_string{nullptr};
        char* geometry{nullptr};
        char* output_filename{nullptr};
        char* word_char_exceptions{nullptr};
        char* working_directory{nullptr};
        char** dingus{nullptr};
        char** exec_argv{nullptr};
        char** environment{nullptr};
        GdkPixbuf* background_pixbuf{nullptr};
        CdkRGBA bg_color{1.0, 1.0, 1.0, 1.0};
        CdkRGBA fg_color{0.0, 0.0, 0.0, 1.0};
        CdkRGBA cursor_bg_color{};
        CdkRGBA cursor_fg_color{};
        CdkRGBA hl_bg_color{};
        CdkRGBA hl_fg_color{};
        int cjk_ambiguous_width{1};
        int extra_margin{-1};
        int scrollback_lines{-1 /* infinite */};
        int transparency_percent{-1};
        int verbosity{0};
        double cell_height_scale{1.0};
        double cell_width_scale{1.0};
        BteCursorBlinkMode cursor_blink_mode{BTE_CURSOR_BLINK_SYSTEM};
        BteCursorShape cursor_shape{BTE_CURSOR_SHAPE_BLOCK};
        BteTextBlinkMode text_blink_mode{BTE_TEXT_BLINK_ALWAYS};
        bte::glib::RefPtr<CtkCssProvider> css{};

        ~Options() {
                g_clear_object(&background_pixbuf);
                g_free(command);
                g_free(encoding);
                g_free(font_string);
                g_free(geometry);
                g_free(output_filename);
                g_free(word_char_exceptions);
                g_free(working_directory);
                g_strfreev(dingus);
                g_strfreev(exec_argv);
                g_strfreev(environment);
        }

        auto fds()
        {
                auto fds = std::vector<int>{};
                fds.reserve(m_fds.size());
                for (auto& fd : m_fds)
                        fds.emplace_back(fd.get());

                return fds;
        }

        auto map_fds()
        {
                return m_map_fds;
        }

private:

        std::vector<bte::libc::FD> m_fds{};
        std::vector<int> m_map_fds{};

        bool parse_enum(GType type,
                        char const* str,
                        int& value,
                        GError** error)
        {
                GEnumClass* enum_klass = reinterpret_cast<GEnumClass*>(g_type_class_ref(type));
                GEnumValue* enum_value = g_enum_get_value_by_nick(enum_klass, str);

                if (enum_value == nullptr) {
                        g_type_class_unref(enum_klass);
                        g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                                    "Failed to parse enum value \"%s\" as type \"%s\"",
                                    str, g_quark_to_string(g_type_qname(type)));
                        return false;
                }

                value = enum_value->value;
                g_type_class_unref(enum_klass);
                return true;
        }

        bool parse_width_enum(char const* str,
                              int& value,
                              GError** error)
        {
                int v = 1;
                if (str == nullptr || g_str_equal(str, "narrow"))
                        v = 1;
                else if (g_str_equal(str, "wide"))
                        v = 2;
                else {
                        g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                                    "Failed to parse \"%s\" as width (allowed values are \"narrow\" or \"wide\")", str);
                        return false;
                }

                value = v;
                return true;
        }

        bool parse_color(char const* str,
                         CdkRGBA* value,
                         bool* value_set,
                         GError** error)
        {
                CdkRGBA color;
                if (!cdk_rgba_parse(&color, str)) {
                        g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                                    "Failed to parse \"%s\" as color", str);
                        *value_set = false;
                        return false;
                }

                *value = color;
                *value_set = true;
                return true;
        }

        int parse_fd_arg(char const* arg,
                         char** end_ptr,
                         GError** error)
        {
                errno = 0;
                char* end = nullptr;
                auto const v = g_ascii_strtoll(arg, &end, 10);
                if (errno || end == arg || v < G_MININT || v > G_MAXINT) {
                        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                                     "Failed to parse \"%s\" as file descriptor number", arg);
                        return -1;
                }
                if (v == -1) {
                        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                                     "\"%s\" is not a valid file descriptor number", arg);
                        return -1;
                }

                if (end_ptr) {
                        *end_ptr = end;
                } else if (*end) {
                        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                                     "Extra characters after number in \"%s\"", arg);
                        return -1;
                }

                return int(v);
        }

        bool parse_fd_arg(char const* str,
                          GError** error)
        {
                char *end = nullptr;
                auto fd = parse_fd_arg(str, &end, error);
                if (fd == -1)
                        return FALSE;

                auto map_to = int{};
                if (*end == '=' || *end == ':') {
                        map_to = parse_fd_arg(end + 1, nullptr, error);
                        if (map_to == -1)
                                return false;

                        if (map_to == STDIN_FILENO ||
                            map_to == STDOUT_FILENO ||
                            map_to == STDERR_FILENO) {
                                g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                                             "Cannot map file descriptor to %d (reserved)", map_to);
                                return false;
                        }
                } else if (*end) {
                        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                                     "Failed to parse \"%s\" as file descriptor assignment", str);
                        return false;
                } else {
                        map_to = fd;
                }

                /* N:M assigns, N=M assigns a dup of N. Always dup stdin/out/err since
                 * we need to output messages ourself there, too.
                 */
                auto new_fd = int{};
                if (*end == '=' || fd < 3) {
                        new_fd = bte::libc::fd_dup_cloexec(fd, 3);
                        if (new_fd == -1) {
                                auto errsv = bte::libc::ErrnoSaver{};
                                g_set_error (error, G_IO_ERROR, g_io_error_from_errno(errsv),
                                             "Failed to duplicate file descriptor %d: %s",
                                             fd, g_strerror(errsv));
                                return false;
                        }
                } else {
                        new_fd = fd;
                        if (bte::libc::fd_set_cloexec(fd) == -1) {
                                auto errsv = bte::libc::ErrnoSaver{};
                                g_set_error (error, G_IO_ERROR, g_io_error_from_errno(errsv),
                                             "Failed to set cloexec on file descriptor %d: %s",
                                             fd, g_strerror(errsv));
                                return false;
                        }
                }

                m_fds.emplace_back(new_fd);
                m_map_fds.emplace_back(map_to);
                return true;
        }

        bool parse_geometry(char const* str,
                            GError** error)
        {
                g_free(geometry);
                geometry = g_strdup(str);
                return true;
        }

        static gboolean
        parse_background_image(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                g_clear_object(&that->background_pixbuf);
                that->background_pixbuf = gdk_pixbuf_new_from_file(value, error);
                return that->background_pixbuf != nullptr;
        }

        static gboolean
        parse_background_extend(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                int v;
                auto rv = that->parse_enum(CAIRO_GOBJECT_TYPE_EXTEND, value, v, error);
                if (rv)
                        that->background_extend = cairo_extend_t(v);
                return rv;
        }

        static gboolean
        parse_cjk_width(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                return that->parse_width_enum(value, that->cjk_ambiguous_width, error);
        };

        static gboolean
        parse_cursor_blink(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                int v;
                auto rv = that->parse_enum(BTE_TYPE_CURSOR_BLINK_MODE, value, v, error);
                if (rv)
                        that->cursor_blink_mode = BteCursorBlinkMode(v);
                return rv;
        }

        static gboolean
        parse_cursor_shape(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                int v;
                auto rv = that->parse_enum(BTE_TYPE_CURSOR_SHAPE, value, v, error);
                if (rv)
                        that->cursor_shape = BteCursorShape(v);
                return rv;
        }

        static gboolean
        parse_bg_color(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                bool set;
                return that->parse_color(value, &that->bg_color, &set, error);
        }

        static gboolean
        parse_css_file(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);

                auto css = bte::glib::take_ref(ctk_css_provider_new());
                if (!ctk_css_provider_load_from_path(css.get(), value, error))
                    return false;

                that->css = std::move(css);
                return true;
        }

        static gboolean
        parse_fd(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                return that->parse_fd_arg(value, error);
        }

        static gboolean
        parse_fg_color(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                bool set;
                return that->parse_color(value, &that->fg_color, &set, error);
        }

        static gboolean
        parse_cursor_bg_color(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                return that->parse_color(value, &that->cursor_bg_color, &that->cursor_bg_color_set, error);
        }

        static gboolean
        parse_cursor_fg_color(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                return that->parse_color(value, &that->cursor_fg_color, &that->cursor_fg_color_set, error);
        }

        static gboolean
        parse_hl_bg_color(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                return that->parse_color(value, &that->hl_bg_color, &that->hl_bg_color_set, error);
        }

        static gboolean
        parse_hl_fg_color(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                return that->parse_color(value, &that->hl_fg_color, &that->hl_fg_color_set, error);
        }

        static gboolean
        parse_text_blink(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                int v;
                auto rv = that->parse_enum(BTE_TYPE_TEXT_BLINK_MODE, value, v, error);
                if (rv)
                        that->text_blink_mode = BteTextBlinkMode(v);
                return rv;
        }

        static gboolean
        parse_verbosity(char const* option, char const* value, void* data, GError** error)
        {
                Options* that = static_cast<Options*>(data);
                that->verbosity++;
                return true;
        }

public:

        double get_alpha() const
        {
                return double(100 - CLAMP(transparency_percent, 0, 100)) / 100.0;
        }

        double get_alpha_bg() const
        {
                double alpha;
                if (background_pixbuf != nullptr)
                        alpha = 0.0;
                else if (whole_window_transparent)
                        alpha = 1.0;
                else
                        alpha = get_alpha();

                return alpha;
        }

        double get_alpha_bg_for_draw() const
        {
                double alpha;
                if (whole_window_transparent)
                        alpha = 1.0;
                else
                        alpha = get_alpha();

                return alpha;
        }

        CdkRGBA get_color_bg() const
        {
                CdkRGBA color{bg_color};
                color.alpha = get_alpha_bg();
                return color;
        }

        CdkRGBA get_color_fg() const
        {
                return fg_color;
        }

        bool parse_argv(int argc,
                        char* argv[],
                        GError** error)
        {
                bool dummy_bool;
                char* dummy_string = nullptr;
                GOptionEntry const entries[] = {
                        { .long_name = "allow-window-ops", .short_name = 0, .flags = 0, .arg = G_OPTION_ARG_NONE, .arg_data = &allow_window_ops,
                          .description = "Allow window operations (resize, move, raise/lower, (de)iconify)", .arg_description = nullptr },
                        { .long_name = "audible-bell", .short_name = 'a', .flags = 0, .arg = G_OPTION_ARG_NONE, .arg_data = &audible_bell,
                          .description = "Use audible terminal bell", .arg_description = nullptr },
                        { .long_name = "backdrop", .short_name = 0, .flags = 0, .arg = G_OPTION_ARG_NONE, .arg_data = &backdrop,
                          .description = "Dim when toplevel unfocused", .arg_description = nullptr },
                        { .long_name = "background-color", .short_name = 0, .flags = 0, .arg = G_OPTION_ARG_CALLBACK, .arg_data = (void*)parse_bg_color,
                          .description = "Set default background color", .arg_description = "COLOR" },
                        { .long_name = "background-image", .short_name = 0, .flags = 0, .arg = G_OPTION_ARG_CALLBACK, .arg_data = (void*)parse_background_image,
                          .description = "Set background image from file", .arg_description = "FILE" },
                        { .long_name = "background-extend", .short_name = 0, .flags = 0, .arg = G_OPTION_ARG_CALLBACK, .arg_data = (void*)parse_background_extend,
                          .description = "Set background image extend", .arg_description = "EXTEND" },
                        { .long_name = "blink", .short_name = 0, .flags = 0, .arg = G_OPTION_ARG_CALLBACK, .arg_data = (void*)parse_text_blink,
                          .description = "Text blink mode (never|focused|unfocused|always)", .arg_description = "MODE" },
                        { .long_name = "bold-is-bright", .short_name = 'B', .flags = 0, .arg = G_OPTION_ARG_NONE, .arg_data = &bold_is_bright,
                          .description = "Bold also brightens colors", .arg_description = nullptr },
                        { .long_name = "cell-height-scale", .short_name = 0, .flags = 0, .arg = G_OPTION_ARG_DOUBLE, .arg_data = &cell_height_scale,
                          .description = "Add extra line spacing", .arg_description = "1.0..2.0" },
                        { .long_name = "cell-width-scale", .short_name = 0, .flags = 0, .arg = G_OPTION_ARG_DOUBLE, .arg_data = &cell_width_scale,
                          .description = "Add extra letter spacing", .arg_description = "1.0..2.0" },
                        { .long_name = "cjk-width", .short_name = 0, .flags = 0, .arg = G_OPTION_ARG_CALLBACK, .arg_data = (void*)parse_cjk_width,
                          .description = "Specify the cjk ambiguous width to use for UTF-8 encoding", .arg_description = "NARROW|WIDE" },
                        { .long_name = "cursor-blink", .short_name = 0, .flags = 0, .arg = G_OPTION_ARG_CALLBACK, .arg_data = (void*)parse_cursor_blink,
                          .description = "Cursor blink mode (system|on|off)", .arg_description = "MODE" },
                        { .long_name = "cursor-background-color", .short_name = 0, .flags = 0, .arg = G_OPTION_ARG_CALLBACK, .arg_data = (void*)parse_cursor_bg_color,
                          .description = "Enable a colored cursor background", .arg_description = "COLOR" },
                        { .long_name = "cursor-foreground-color", .short_name = 0, .flags = 0, .arg = G_OPTION_ARG_CALLBACK, .arg_data = (void*)parse_cursor_fg_color,
                          .description = "Enable a colored cursor foreground", .arg_description = "COLOR" },
                        { .long_name = "cursor-shape", .short_name = 0, .flags = 0, .arg = G_OPTION_ARG_CALLBACK, .arg_data = (void*)parse_cursor_shape,
                          .description = "Set cursor shape (block|underline|ibeam)", .arg_description = nullptr },
                        { .long_name = "css-file", .short_name = 0, .flags = G_OPTION_FLAG_FILENAME, .arg = G_OPTION_ARG_CALLBACK, .arg_data = (void*)parse_css_file,
                          .description = "Load CSS from FILE", .arg_description = "FILE" },
                        { .long_name = "dingu", .short_name = 'D', .flags = 0, .arg = G_OPTION_ARG_STRING_ARRAY, .arg_data = &dingus,
                          .description = "Add regex highlight", .arg_description = nullptr },
                        { .long_name = "debug", .short_name = 'd', .flags = 0, .arg = G_OPTION_ARG_NONE, .arg_data = &debug,
                          .description = "Enable various debugging checks", .arg_description = nullptr },
                        { .long_name = "encoding", .short_name = 0, .flags = 0, .arg = G_OPTION_ARG_STRING, .arg_data = &encoding,
                          .description = "Specify the terminal encoding to use", .arg_description = "ENCODING" },
                        { .long_name = "env", .short_name = 0, .flags = 0, .arg = G_OPTION_ARG_FILENAME_ARRAY, .arg_data = &environment,
                          .description = "Add environment variable to the child\'s environment", .arg_description = "VAR=VALUE" },
                        { .long_name = "extra-margin", .short_name = 0, .flags = 0, .arg = G_OPTION_ARG_INT, .arg_data = &extra_margin,
                          .description = "Add extra margin around the terminal widget", .arg_description = "MARGIN" },
                        { .long_name = "fd", .short_name = 0, .flags = 0, .arg = G_OPTION_ARG_CALLBACK, .arg_data = (void*)parse_fd,
                          .description = "Pass file descriptor N (as M) to the child process", .arg_description = "N[=M]" },
                        { .long_name = "feed-stdin", .short_name = 'B', .flags = 0, .arg = G_OPTION_ARG_NONE, .arg_data = &feed_stdin,
                          .description = "Feed input to the terminal", .arg_description = nullptr },
                        { .long_name = "font", .short_name = 'f', .flags = 0, .arg = G_OPTION_ARG_STRING, .arg_data = &font_string,
                          .description = "Specify a font to use", .arg_description = nullptr },
                        { .long_name = "foreground-color", .short_name = 0, .flags = 0, .arg = G_OPTION_ARG_CALLBACK, .arg_data = (void*)parse_fg_color,
                          .description = "Set default foreground color", .arg_description = "COLOR" },
                        { .long_name = "geometry", .short_name = 'g', .flags = 0, .arg = G_OPTION_ARG_STRING, .arg_data = &geometry,
                          .description = "Set the size (in characters) and position", .arg_description = "GEOMETRY" },
                        { .long_name = "highlight-background-color", .short_name = 0, .flags = 0, .arg = G_OPTION_ARG_CALLBACK, .arg_data = (void*)parse_hl_bg_color,
                          .description = "Enable distinct highlight background color for selection", .arg_description = "COLOR" },
                        { .long_name = "highlight-foreground-color", .short_name = 0, .flags = 0, .arg = G_OPTION_ARG_CALLBACK, .arg_data = (void*)parse_hl_fg_color,
                          .description = "Enable distinct highlight foreground color for selection", .arg_description = "COLOR" },
                        { .long_name = "icon-title", .short_name = 'i', .flags = 0, .arg = G_OPTION_ARG_NONE, .arg_data = &icon_title,
                          .description = "Enable the setting of the icon title", .arg_description = nullptr },
                        { .long_name = "keep", .short_name = 'k', .flags = 0, .arg = G_OPTION_ARG_NONE, .arg_data = &keep,
                          .description = "Live on after the command exits", .arg_description = nullptr },
                        { .long_name = "no-argb-visual", .short_name = 0, .flags = 0, .arg = G_OPTION_ARG_NONE, .arg_data = &no_argb_visual,
                          .description = "Don't use an ARGB visual", .arg_description = nullptr },
                        { .long_name = "no-bidi", .short_name = 0, .flags = 0, .arg = G_OPTION_ARG_NONE, .arg_data = &no_bidi,
                          .description = "Disable BiDi", .arg_description = nullptr },
                        { .long_name = "no-bold", .short_name = 0, .flags = 0, .arg = G_OPTION_ARG_NONE, .arg_data = &no_bold,
                          .description = "Disable bold", .arg_description = nullptr },
                        { .long_name = "no-builtin-dingus", .short_name = 0, .flags = 0, .arg = G_OPTION_ARG_NONE, .arg_data = &no_builtin_dingus,
                          .description = "Highlight URLs inside the terminal", .arg_description = nullptr },
                        { .long_name = "no-context-menu", .short_name = 0, .flags = 0, .arg = G_OPTION_ARG_NONE, .arg_data = &no_context_menu,
                          .description = "Disable context menu", .arg_description = nullptr },
                        { .long_name = "no-decorations", .short_name = 0, .flags = 0, .arg = G_OPTION_ARG_NONE, .arg_data = &no_decorations,
                          .description = "Disable window decorations", .arg_description = nullptr },
                        { .long_name = "no-double-buffer", .short_name = '2', .flags = 0, .arg = G_OPTION_ARG_NONE, .arg_data = &no_double_buffer,
                          .description = "Disable double-buffering", .arg_description = nullptr },
                        { .long_name = "no-geometry-hints", .short_name = 'G', .flags = 0, .arg = G_OPTION_ARG_NONE, .arg_data = &no_geometry_hints,
                          .description = "Allow the terminal to be resized to any dimension, not constrained to fit to an integer multiple of characters", .arg_description = nullptr },
                        { .long_name = "no-hyperlink", .short_name = 'H', .flags = 0, .arg = G_OPTION_ARG_NONE, .arg_data = &no_hyperlink,
                          .description = "Disable hyperlinks", .arg_description = nullptr },
                        { .long_name = "no-pty", .short_name = 0, .flags = 0, .arg = G_OPTION_ARG_NONE, .arg_data = &no_pty,
                          .description = "Disable PTY creation with --no-shell", .arg_description = nullptr },
                        { .long_name = "no-rewrap", .short_name = 'R', .flags = 0, .arg = G_OPTION_ARG_NONE, .arg_data = &no_rewrap,
                          .description = "Disable rewrapping on resize", .arg_description = nullptr },
                        { .long_name = "no-scrollbar", .short_name = 0, .flags = 0, .arg = G_OPTION_ARG_NONE, .arg_data = &no_scrollbar,
                          .description = "Disable scrollbar", .arg_description = nullptr },
                        { .long_name = "no-shaping", .short_name = 0, .flags = 0, .arg = G_OPTION_ARG_NONE, .arg_data = &no_shaping,
                          .description = "Disable Arabic shaping", .arg_description = nullptr },
                        { .long_name = "no-shell", .short_name = 'S', .flags = 0, .arg = G_OPTION_ARG_NONE, .arg_data = &no_shell,
                          .description = "Disable spawning a shell inside the terminal", .arg_description = nullptr },
                        { .long_name = "no-systemd-scope", .short_name = 0, .flags = 0, .arg = G_OPTION_ARG_NONE, .arg_data = &no_systemd_scope,
                          .description = "Don't use systemd user scope", .arg_description = nullptr },
                        { .long_name = "object-notifications", .short_name = 'N', .flags = 0, .arg = G_OPTION_ARG_NONE, .arg_data = &object_notifications,
                          .description = "Print BteTerminal object notifications", .arg_description = nullptr },
                        { .long_name = "output-file", .short_name = 0, .flags = 0, .arg = G_OPTION_ARG_FILENAME, .arg_data = &output_filename,
                          .description = "Save terminal contents to file at exit", .arg_description = nullptr },
                        { .long_name = "reverse", .short_name = 0, .flags = 0, .arg = G_OPTION_ARG_NONE, .arg_data = &reverse,
                          .description = "Reverse foreground/background colors", .arg_description = nullptr },
                        { .long_name = "require-systemd-scope", .short_name = 0, .flags = 0, .arg = G_OPTION_ARG_NONE, .arg_data = &require_systemd_scope,
                          .description = "Require use of a systemd user scope", .arg_description = nullptr },
                        { .long_name = "scrollback-lines", .short_name = 'n', .flags = 0, .arg = G_OPTION_ARG_INT, .arg_data = &scrollback_lines,
                          .description = "Specify the number of scrollback-lines (-1 for infinite)", .arg_description = nullptr },
                        { .long_name = "transparent", .short_name = 'T', .flags = 0, .arg = G_OPTION_ARG_INT, .arg_data = &transparency_percent,
                          .description = "Enable the use of a transparent background", .arg_description = "0..100" },
                        { .long_name = "verbose", .short_name = 'v', .flags = G_OPTION_FLAG_NO_ARG, .arg = G_OPTION_ARG_CALLBACK,
                          .arg_data = (void*)parse_verbosity,
                          .description = "Enable verbose debug output", .arg_description = nullptr },
                        { .long_name = "version", .short_name = 0, .flags = 0, .arg = G_OPTION_ARG_NONE, .arg_data = &version,
                          .description = "Show version", .arg_description = nullptr },
                        { .long_name = "whole-window-transparent", .short_name = 0, .flags = 0, .arg = G_OPTION_ARG_NONE, .arg_data = &whole_window_transparent,
                          .description = "Make the whole window transparent", .arg_description = NULL },
                        { .long_name = "word-char-exceptions", .short_name = 0, .flags = 0, .arg = G_OPTION_ARG_STRING, .arg_data = &word_char_exceptions,
                          .description = "Specify the word char exceptions", .arg_description = "CHARS" },
                        { .long_name = "working-directory", .short_name = 'w', .flags = 0, .arg = G_OPTION_ARG_FILENAME, .arg_data = &working_directory,
                          .description = "Specify the initial working directory of the terminal", .arg_description = nullptr },

                        /* Options for compatibility with the old bteapp test application */
                        { .long_name = "border-width", .short_name = 0, .flags = G_OPTION_FLAG_HIDDEN, .arg = G_OPTION_ARG_INT, .arg_data = &extra_margin,
                          .description = nullptr, .arg_description = nullptr },
                        { .long_name = "command", .short_name = 'c', .flags = G_OPTION_FLAG_HIDDEN, .arg = G_OPTION_ARG_FILENAME, .arg_data = &command,
                          .description = nullptr, .arg_description = nullptr },
                        { .long_name = "console", .short_name = 'C', .flags = G_OPTION_FLAG_HIDDEN, .arg = G_OPTION_ARG_NONE, .arg_data = &console,
                          .description = nullptr, .arg_description = nullptr },
                        { .long_name = "double-buffer", .short_name = '2', .flags = G_OPTION_FLAG_REVERSE | G_OPTION_FLAG_HIDDEN,
                          .arg = G_OPTION_ARG_NONE, .arg_data = &no_double_buffer, .description = nullptr, .arg_description = nullptr },
                        { .long_name = "pty-flags", .short_name = 0, .flags = G_OPTION_FLAG_HIDDEN, .arg = G_OPTION_ARG_STRING, .arg_data = &dummy_string,
                          .description = nullptr, .arg_description = nullptr },
                        { .long_name = "scrollbar-policy", .short_name = 'P', .flags = G_OPTION_FLAG_HIDDEN, .arg = G_OPTION_ARG_STRING,
                          .arg_data = &dummy_string, .description = nullptr, .arg_description = nullptr },
                        { .long_name = "scrolled-window", .short_name = 'W', .flags = G_OPTION_FLAG_HIDDEN, .arg = G_OPTION_ARG_NONE,
                          .arg_data = &dummy_bool, .description = nullptr, .arg_description = nullptr },
                        { .long_name = "shell", .short_name = 'S', .flags = G_OPTION_FLAG_REVERSE | G_OPTION_FLAG_HIDDEN,
                          .arg = G_OPTION_ARG_NONE, .arg_data = &no_shell, .description = nullptr, .arg_description = nullptr },
#ifdef BTE_DEBUG
                        { .long_name = "test-mode", .short_name = 0, .flags = 0, .arg = G_OPTION_ARG_NONE, .arg_data = &test_mode,
                          .description = "Enable test mode", .arg_description = nullptr },
#endif
                        { .long_name = "use-theme-colors", .short_name = 0, .flags = 0, .arg = G_OPTION_ARG_NONE, .arg_data = &use_theme_colors,
                          .description = "Use foreground and background colors from the ctk+ theme", .arg_description = nullptr }
                };

                /* Look for '--' */
                for (int i = 0; i < argc; i++) {
                        if (!g_str_equal(argv[i], "--"))
                                continue;

                        i++; /* skip it */
                        if (i == argc) {
                                g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
                                            "No command specified after -- terminator");
                                return false;
                        }

                        exec_argv = g_new(char*, argc - i + 1);
                        int j;
                        for (j = 0; i < argc; i++, j++)
                                exec_argv[j] = g_strdup(argv[i]);
                        exec_argv[j] = nullptr;
                        break;
                }

                auto context = g_option_context_new("[-- COMMAND …] — BTE test application");
                g_option_context_set_help_enabled(context, true);
                g_option_context_set_translation_domain(context, GETTEXT_PACKAGE);

                auto group = g_option_group_new(nullptr, nullptr, nullptr, this, nullptr);
                g_option_group_set_translation_domain(group, GETTEXT_PACKAGE);
                g_option_group_add_entries(group, entries);
                g_option_context_set_main_group(context, group);

                g_option_context_add_group(context, ctk_get_option_group(true));

                bool rv = g_option_context_parse(context, &argc, &argv, error);

                g_option_context_free(context);
                g_free(dummy_string);

                if (reverse)
                        std::swap(fg_color, bg_color);

                return rv;
        }
};

Options options{}; /* global */

/* debug output */

static void G_GNUC_PRINTF(2, 3)
verbose_fprintf(FILE* fp,
                char const* format,
                ...)
{
        if (options.verbosity == 0)
                return;

        va_list args;
        va_start(args, format);
        g_vfprintf(fp, format, args);
        va_end(args);
}

#define verbose_print(...) verbose_fprintf(stdout, __VA_ARGS__)
#define verbose_printerr(...) verbose_fprintf(stderr, __VA_ARGS__)

/* regex */

static void
jit_regex(BteRegex* regex,
          char const* pattern)
{
        auto error = bte::glib::Error{};
        if (!bte_regex_jit(regex, PCRE2_JIT_COMPLETE, error) ||
            !bte_regex_jit(regex, PCRE2_JIT_PARTIAL_SOFT, error)) {
                if (!error.matches(BTE_REGEX_ERROR, -45 /* PCRE2_ERROR_JIT_BADOPTION: JIT not supported */))
                        verbose_printerr("JITing regex \"%s\" failed: %s\n", pattern, error.message());
        }
}

static BteRegex*
compile_regex_for_search(char const* pattern,
                         bool caseless,
                         GError** error)
{
        uint32_t flags = PCRE2_UTF | PCRE2_NO_UTF_CHECK | PCRE2_MULTILINE;
        if (caseless)
                flags |= PCRE2_CASELESS;

        auto regex = bte_regex_new_for_search(pattern, strlen(pattern), flags, error);
        if (regex != nullptr)
                jit_regex(regex, pattern);

        return regex;
}

static BteRegex*
compile_regex_for_match(char const* pattern,
                        bool caseless,
                        GError** error)
{
        uint32_t flags = PCRE2_UTF | PCRE2_NO_UTF_CHECK | PCRE2_MULTILINE;
        if (caseless)
                flags |= PCRE2_CASELESS;

        auto regex = bte_regex_new_for_match(pattern, strlen(pattern), flags, error);
        if (regex != nullptr)
                jit_regex(regex, pattern);

        return regex;
}

/* search popover */

#define BTEAPP_TYPE_SEARCH_POPOVER         (bteapp_search_popover_get_type())
#define BTEAPP_SEARCH_POPOVER(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), BTEAPP_TYPE_SEARCH_POPOVER, BteappSearchPopover))
#define BTEAPP_SEARCH_POPOVER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BTEAPP_TYPE_SEARCH_POPOVER, BteappSearchPopoverClass))
#define BTEAPP_IS_SEARCH_POPOVER(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), BTEAPP_TYPE_SEARCH_POPOVER))
#define BTEAPP_IS_SEARCH_POPOVER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), BTEAPP_TYPE_SEARCH_POPOVER))
#define BTEAPP_SEARCH_POPOVER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), BTEAPP_TYPE_SEARCH_POPOVER, BteappSearchPopoverClass))

typedef struct _BteappSearchPopover        BteappSearchPopover;
typedef struct _BteappSearchPopoverClass   BteappSearchPopoverClass;

struct _BteappSearchPopover {
        CtkPopover parent;

        /* from CtkWidget template */
        CtkWidget* search_entry;
        CtkWidget* search_prev_button;
        CtkWidget* search_next_button;
        CtkWidget* close_button;
        CtkToggleButton* match_case_checkbutton;
        CtkToggleButton* entire_word_checkbutton;
        CtkToggleButton* regex_checkbutton;
        CtkToggleButton* wrap_around_checkbutton;
        CtkWidget* reveal_button;
        CtkWidget* revealer;
        /* end */

        BteTerminal *terminal;
        bool regex_caseless;
        bool has_regex;
        char* regex_pattern;
};

struct _BteappSearchPopoverClass {
        CtkPopoverClass parent;
};

static GType bteapp_search_popover_get_type(void);

enum {
        SEARCH_POPOVER_PROP0,
        SEARCH_POPOVER_PROP_TERMINAL,
        SEARCH_POPOVER_N_PROPS
};

static GParamSpec* search_popover_pspecs[SEARCH_POPOVER_N_PROPS];

static void
bteapp_search_popover_update_sensitivity(BteappSearchPopover* popover)
{
        bool can_search = popover->has_regex;

        ctk_widget_set_sensitive(popover->search_next_button, can_search);
        ctk_widget_set_sensitive(popover->search_prev_button, can_search);
}

static void
bteapp_search_popover_update_regex(BteappSearchPopover* popover)
{
        char const* search_text = ctk_entry_get_text(CTK_ENTRY(popover->search_entry));
        bool caseless = ctk_toggle_button_get_active(popover->match_case_checkbutton) == FALSE;

        char* pattern;
        if (ctk_toggle_button_get_active(popover->regex_checkbutton))
                pattern = g_strdup(search_text);
        else
                pattern = g_regex_escape_string(search_text, -1);

        if (ctk_toggle_button_get_active(popover->regex_checkbutton)) {
                char* tmp = g_strdup_printf("\\b%s\\b", pattern);
                g_free(pattern);
                pattern = tmp;
        }

        if (caseless == popover->regex_caseless &&
            g_strcmp0(pattern, popover->regex_pattern) == 0)
                return;

        popover->regex_caseless = caseless;
        g_free(popover->regex_pattern);
        popover->regex_pattern = nullptr;

        if (search_text[0] != '\0') {
                auto error = bte::glib::Error{};
                auto regex = compile_regex_for_search(pattern, caseless, error);
                bte_terminal_search_set_regex(popover->terminal, regex, 0);
                if (regex != nullptr)
                        bte_regex_unref(regex);

                if (error.error()) {
                        popover->has_regex = false;
                        popover->regex_pattern = pattern; /* adopt */
                        pattern = nullptr; /* adopted */
                        ctk_widget_set_tooltip_text(popover->search_entry, nullptr);
                } else {
                        popover->has_regex = true;
                        ctk_widget_set_tooltip_text(popover->search_entry, error.message());
                }
        }

        g_free(pattern);

        bteapp_search_popover_update_sensitivity(popover);
}

static void
bteapp_search_popover_wrap_around_toggled(CtkToggleButton* button,
                                          BteappSearchPopover* popover)
{
        bte_terminal_search_set_wrap_around(popover->terminal, ctk_toggle_button_get_active(button));
}

static void
bteapp_search_popover_search_forward(BteappSearchPopover* popover)
{
        if (!popover->has_regex)
                return;
        bte_terminal_search_find_next(popover->terminal);
}

static void
bteapp_search_popover_search_backward(BteappSearchPopover* popover)
{
        if (!popover->has_regex)
                return;
        bte_terminal_search_find_previous(popover->terminal);
}

G_DEFINE_TYPE(BteappSearchPopover, bteapp_search_popover, CTK_TYPE_POPOVER)

static void
bteapp_search_popover_init(BteappSearchPopover* popover)
{
        ctk_widget_init_template(CTK_WIDGET(popover));

        popover->regex_caseless = false;
        popover->has_regex = false;
        popover->regex_pattern = nullptr;

        g_signal_connect_swapped(popover->close_button, "clicked", G_CALLBACK(ctk_widget_hide), popover);

        g_object_bind_property(popover->reveal_button, "active",
                               popover->revealer, "reveal-child",
                               GBindingFlags(G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE));

        g_signal_connect_swapped(popover->search_entry, "next-match", G_CALLBACK(bteapp_search_popover_search_forward), popover);
        g_signal_connect_swapped(popover->search_entry, "previous-match", G_CALLBACK(bteapp_search_popover_search_backward), popover);
        g_signal_connect_swapped(popover->search_entry, "search-changed", G_CALLBACK(bteapp_search_popover_update_regex), popover);

        g_signal_connect_swapped(popover->search_next_button,"clicked", G_CALLBACK(bteapp_search_popover_search_forward), popover);
        g_signal_connect_swapped(popover->search_prev_button,"clicked", G_CALLBACK(bteapp_search_popover_search_backward), popover);

        g_signal_connect_swapped(popover->match_case_checkbutton,"toggled", G_CALLBACK(bteapp_search_popover_update_regex), popover);
        g_signal_connect_swapped(popover->entire_word_checkbutton,"toggled", G_CALLBACK(bteapp_search_popover_update_regex), popover);
        g_signal_connect_swapped(popover->regex_checkbutton,"toggled", G_CALLBACK(bteapp_search_popover_update_regex), popover);
        g_signal_connect_swapped(popover->match_case_checkbutton, "toggled", G_CALLBACK(bteapp_search_popover_update_regex), popover);

        g_signal_connect(popover->wrap_around_checkbutton, "toggled", G_CALLBACK(bteapp_search_popover_wrap_around_toggled), popover);

        bteapp_search_popover_update_sensitivity(popover);
}

static void
bteapp_search_popover_finalize(GObject *object)
{
        BteappSearchPopover* popover = BTEAPP_SEARCH_POPOVER(object);

        g_free(popover->regex_pattern);

        G_OBJECT_CLASS(bteapp_search_popover_parent_class)->finalize(object);
}

static void
bteapp_search_popover_set_property(GObject* object,
                                   guint prop_id,
                                   const GValue* value,
                                   GParamSpec* pspec)
{
        BteappSearchPopover* popover = BTEAPP_SEARCH_POPOVER(object);

        switch (prop_id) {
        case SEARCH_POPOVER_PROP_TERMINAL:
                popover->terminal = reinterpret_cast<BteTerminal*>(g_value_get_object(value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
                break;
        }
}

static void
bteapp_search_popover_class_init(BteappSearchPopoverClass* klass)
{
        GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
        gobject_class->finalize = bteapp_search_popover_finalize;
        gobject_class->set_property = bteapp_search_popover_set_property;

        search_popover_pspecs[SEARCH_POPOVER_PROP_TERMINAL] =
                g_param_spec_object("terminal", nullptr, nullptr,
                                    BTE_TYPE_TERMINAL,
                                    GParamFlags(G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY));

        g_object_class_install_properties(gobject_class, G_N_ELEMENTS(search_popover_pspecs), search_popover_pspecs);

        CtkWidgetClass* widget_class = CTK_WIDGET_CLASS(klass);
        ctk_widget_class_set_template_from_resource(widget_class, "/org/gnome/bte/app/ui/search-popover.ui");
        ctk_widget_class_bind_template_child(widget_class, BteappSearchPopover, search_entry);
        ctk_widget_class_bind_template_child(widget_class, BteappSearchPopover, search_prev_button);
        ctk_widget_class_bind_template_child(widget_class, BteappSearchPopover, search_next_button);
        ctk_widget_class_bind_template_child(widget_class, BteappSearchPopover, reveal_button);
        ctk_widget_class_bind_template_child(widget_class, BteappSearchPopover, close_button);
        ctk_widget_class_bind_template_child(widget_class, BteappSearchPopover, revealer);
        ctk_widget_class_bind_template_child(widget_class, BteappSearchPopover, match_case_checkbutton);
        ctk_widget_class_bind_template_child(widget_class, BteappSearchPopover, entire_word_checkbutton);
        ctk_widget_class_bind_template_child(widget_class, BteappSearchPopover, regex_checkbutton);
        ctk_widget_class_bind_template_child(widget_class, BteappSearchPopover, wrap_around_checkbutton);

        ctk_widget_class_set_css_name(widget_class, "bteapp-search-popover");
}

static CtkWidget*
bteapp_search_popover_new(BteTerminal* terminal,
                          CtkWidget* relative_to)
{
        return reinterpret_cast<CtkWidget*>(g_object_new(BTEAPP_TYPE_SEARCH_POPOVER,
                                                         "terminal", terminal,
                                                         "relative-to", relative_to,
                                                         nullptr));
}

/* terminal */

#define BTEAPP_TYPE_TERMINAL         (bteapp_terminal_get_type())
#define BTEAPP_TERMINAL(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), BTEAPP_TYPE_TERMINAL, BteappTerminal))
#define BTEAPP_TERMINAL_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BTEAPP_TYPE_TERMINAL, BteappTerminalClass))
#define BTEAPP_IS_TERMINAL(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), BTEAPP_TYPE_TERMINAL))
#define BTEAPP_IS_TERMINAL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), BTEAPP_TYPE_TERMINAL))
#define BTEAPP_TERMINAL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), BTEAPP_TYPE_TERMINAL, BteappTerminalClass))

typedef struct _BteappTerminal       BteappTerminal;
typedef struct _BteappTerminalClass  BteappTerminalClass;

struct _BteappTerminal {
        BteTerminal parent;

        cairo_pattern_t* background_pattern;
        bool has_backdrop;
        bool use_backdrop;
};

struct _BteappTerminalClass {
        BteTerminalClass parent;
};

static GType bteapp_terminal_get_type(void);

G_DEFINE_TYPE(BteappTerminal, bteapp_terminal, BTE_TYPE_TERMINAL)

#define BACKDROP_ALPHA (0.2)

static void
bteapp_terminal_realize(CtkWidget* widget)
{
        CTK_WIDGET_CLASS(bteapp_terminal_parent_class)->realize(widget);

        BteappTerminal* terminal = BTEAPP_TERMINAL(widget);
        if (options.background_pixbuf != nullptr) {
                auto surface = cdk_cairo_surface_create_from_pixbuf(options.background_pixbuf,
                                                                    0 /* take scale from window */,
                                                                    ctk_widget_get_window(widget));
                terminal->background_pattern = cairo_pattern_create_for_surface(surface);
                cairo_surface_destroy(surface);

                cairo_pattern_set_extend(terminal->background_pattern, options.background_extend);
        }
}

static void
bteapp_terminal_unrealize(CtkWidget* widget)
{
        BteappTerminal* terminal = BTEAPP_TERMINAL(widget);
        if (terminal->background_pattern != nullptr) {
                cairo_pattern_destroy(terminal->background_pattern);
                terminal->background_pattern = nullptr;
        }

        CTK_WIDGET_CLASS(bteapp_terminal_parent_class)->unrealize(widget);
}

static gboolean
bteapp_terminal_draw(CtkWidget* widget,
                     cairo_t* cr)
{
        BteappTerminal* terminal = BTEAPP_TERMINAL(widget);
        if (terminal->background_pattern != nullptr) {
                cairo_push_group(cr);

                /* Draw background colour */
                cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
                cairo_rectangle(cr, 0.0, 0.0,
                                ctk_widget_get_allocated_width(widget),
                                ctk_widget_get_allocated_height(widget));
                CdkRGBA bg;
                bte_terminal_get_color_background_for_draw(BTE_TERMINAL(terminal), &bg);
                cairo_set_source_rgba(cr, bg.red, bg.green, bg.blue, 1.0);
                cairo_paint(cr);

                /* Draw background image */
                cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
                cairo_set_source(cr, terminal->background_pattern);
                cairo_paint(cr);

                cairo_pop_group_to_source(cr);
                cairo_paint_with_alpha(cr, options.get_alpha_bg_for_draw());

        }

        auto rv = CTK_WIDGET_CLASS(bteapp_terminal_parent_class)->draw(widget, cr);

        if (terminal->use_backdrop && terminal->has_backdrop) {
                cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
                cairo_set_source_rgba(cr, 0, 0, 0, BACKDROP_ALPHA);
                cairo_rectangle(cr, 0.0, 0.0,
                                ctk_widget_get_allocated_width(widget),
                                ctk_widget_get_allocated_height(widget));
                cairo_paint(cr);
        }

        return rv;
}

static auto dti(double d) -> unsigned { return CLAMP((d*255), 0, 255); }

static void
bteapp_terminal_style_updated(CtkWidget* widget)
{
        CTK_WIDGET_CLASS(bteapp_terminal_parent_class)->style_updated(widget);

        auto context = ctk_widget_get_style_context(widget);
        auto flags = ctk_style_context_get_state(context);

        BteappTerminal* terminal = BTEAPP_TERMINAL(widget);
        terminal->has_backdrop = (flags & CTK_STATE_FLAG_BACKDROP) != 0;

        if (options.use_theme_colors) {
                auto theme_fg = CdkRGBA{};
                ctk_style_context_get_color(context, flags, &theme_fg);
                auto theme_bg = CdkRGBA{};
                ctk_style_context_get_background_color(context, flags, &theme_bg);

                verbose_print("Theme colors: foreground is #%02X%02X%02X, background is #%02X%02X%02X\n",
                              dti(theme_fg.red), dti(theme_fg.green), dti(theme_fg.blue),
                              dti(theme_bg.red), dti(theme_bg.green), dti(theme_bg.blue));

                theme_fg.alpha = 1.;
                theme_bg.alpha = options.get_alpha_bg();
                bte_terminal_set_colors(BTE_TERMINAL(terminal), &theme_fg, &theme_bg, nullptr, 0);
        }
}

static void
bteapp_terminal_class_init(BteappTerminalClass *klass)
{
        CtkWidgetClass *widget_class = CTK_WIDGET_CLASS(klass);
        widget_class->realize = bteapp_terminal_realize;
        widget_class->unrealize = bteapp_terminal_unrealize;
        widget_class->draw = bteapp_terminal_draw;
        widget_class->style_updated = bteapp_terminal_style_updated;

        ctk_widget_class_set_css_name(widget_class, "bteapp-terminal");
}

static void
bteapp_terminal_init(BteappTerminal *terminal)
{
        terminal->background_pattern = nullptr;
        terminal->has_backdrop = false;
        terminal->use_backdrop = options.backdrop;

        if (options.background_pixbuf != nullptr)
                bte_terminal_set_clear_background(BTE_TERMINAL(terminal), false);
}

static CtkWidget *
bteapp_terminal_new(void)
{
        return reinterpret_cast<CtkWidget*>(g_object_new(BTEAPP_TYPE_TERMINAL, nullptr));
}

/* terminal window */

#define BTEAPP_TYPE_WINDOW         (bteapp_window_get_type())
#define BTEAPP_WINDOW(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), BTEAPP_TYPE_WINDOW, BteappWindow))
#define BTEAPP_WINDOW_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BTEAPP_TYPE_WINDOW, BteappWindowClass))
#define BTEAPP_IS_WINDOW(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), BTEAPP_TYPE_WINDOW))
#define BTEAPP_IS_WINDOW_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), BTEAPP_TYPE_WINDOW))
#define BTEAPP_WINDOW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), BTEAPP_TYPE_WINDOW, BteappWindowClass))

typedef struct _BteappWindow       BteappWindow;
typedef struct _BteappWindowClass  BteappWindowClass;

struct _BteappWindow {
        CtkApplicationWindow parent;

        /* from CtkWidget template */
        CtkWidget* window_box;
        CtkScrollbar* scrollbar;
        /* CtkBox* notifications_box; */
        CtkWidget* readonly_emblem;
        /* CtkButton* copy_button; */
        /* CtkButton* paste_button; */
        CtkToggleButton* find_button;
        CtkMenuButton* gear_button;
        /* end */

        BteTerminal* terminal;
        CtkClipboard* clipboard;
        GPid child_pid;
        CtkWidget* search_popover;

        bool fullscreen{false};

        /* used for updating the geometry hints */
        int cached_cell_width{0};
        int cached_cell_height{0};
        int cached_chrome_width{0};
        int cached_chrome_height{0};
        int cached_csd_width{0};
        int cached_csd_height{0};
};

struct _BteappWindowClass {
        CtkApplicationWindowClass parent;
};

static GType bteapp_window_get_type(void);

static char const* const builtin_dingus[] = {
        "(((gopher|news|telnet|nntp|file|http|ftp|https)://)|(www|ftp)[-A-Za-z0-9]*\\.)[-A-Za-z0-9\\.]+(:[0-9]*)?",
        "(((gopher|news|telnet|nntp|file|http|ftp|https)://)|(www|ftp)[-A-Za-z0-9]*\\.)[-A-Za-z0-9\\.]+(:[0-9]*)?/[-A-Za-z0-9_\\$\\.\\+\\!\\*\\(\\),;:@&=\\?/~\\#\\%]*[^]'\\.}>\\) ,\\\"]",
        nullptr,
};

/* Just some arbitrary minimum values */
#define MIN_COLUMNS (16)
#define MIN_ROWS    (2)

static void
bteapp_window_add_dingus(BteappWindow* window,
                         char const* const* dingus)
{
        for (auto i = 0; dingus[i] != nullptr; i++) {
                auto tag = int{-1};
                auto error = bte::glib::Error{};
                auto regex = compile_regex_for_match(dingus[i], true, error);
                if (regex) {
                        tag = bte_terminal_match_add_regex(window->terminal, regex, 0);
                        bte_regex_unref(regex);
                }

                if (error.error()) {
                        verbose_printerr("Failed to compile regex \"%s\": %s\n",
                                         dingus[i], error.message());
                }

                if (tag != -1)
                        bte_terminal_match_set_cursor_name(window->terminal, tag, "pointer");
        }
}

static void
bteapp_window_update_geometry(BteappWindow* window)
{
        CtkWidget* window_widget = CTK_WIDGET(window);
        CtkWidget* terminal_widget = CTK_WIDGET(window->terminal);

        int columns = bte_terminal_get_column_count(window->terminal);
        int rows = bte_terminal_get_row_count(window->terminal);
        int cell_width = bte_terminal_get_char_width(window->terminal);
        int cell_height = bte_terminal_get_char_height(window->terminal);

        /* Calculate the chrome size as difference between the content's
         * natural size requisition and the terminal grid's size.
         * This includes the terminal's padding in the chrome.
         */
        CtkRequisition contents_req;
        ctk_widget_get_preferred_size(window->window_box, nullptr, &contents_req);
        int chrome_width = contents_req.width - cell_width * columns;
        int chrome_height = contents_req.height - cell_height * rows;
        g_assert_cmpint(chrome_width, >=, 0);
        g_assert_cmpint(chrome_height, >=, 0);

        int csd_width = 0;
        int csd_height = 0;
        if (ctk_widget_get_realized(terminal_widget)) {
                /* Calculate the CSD size as difference between the toplevel's
                 * and content's allocation.
                 */
                CtkAllocation toplevel, contents;
                ctk_widget_get_allocation(window_widget, &toplevel);
                ctk_widget_get_allocation(window->window_box, &contents);

                csd_width = toplevel.width - contents.width;
                csd_height = toplevel.height - contents.height;
                g_assert_cmpint(csd_width, >=, 0);
                g_assert_cmpint(csd_height, >=, 0);

                /* Only actually set the geometry hints once the window is realized,
                 * since only then we know the CSD size. Only set the geometry when
                 * anything has changed.
                 */
                if (!options.no_geometry_hints &&
                    (cell_height != window->cached_cell_height ||
                     cell_width != window->cached_cell_width ||
                     chrome_width != window->cached_chrome_width ||
                     chrome_height != window->cached_chrome_height ||
                     csd_width != window->cached_csd_width ||
                     csd_width != window->cached_csd_height)) {
                        CdkGeometry geometry;

                        geometry.base_width = csd_width + chrome_width;
                        geometry.base_height = csd_height + chrome_height;
                        geometry.width_inc = cell_width;
                        geometry.height_inc = cell_height;
                        geometry.min_width = geometry.base_width + cell_width * MIN_COLUMNS;
                        geometry.min_height = geometry.base_height + cell_height * MIN_ROWS;

                        ctk_window_set_geometry_hints(CTK_WINDOW(window),
                                                      nullptr,
                                                      &geometry,
                                                      CdkWindowHints(CDK_HINT_RESIZE_INC |
                                                                     CDK_HINT_MIN_SIZE |
                                                                     CDK_HINT_BASE_SIZE));

                        verbose_print("Updating geometry hints base %dx%d inc %dx%d min %dx%d\n",
                                      geometry.base_width, geometry.base_height,
                                      geometry.width_inc, geometry.height_inc,
                                      geometry.min_width, geometry.min_height);
                }
        }

        window->cached_csd_width = csd_width;
        window->cached_csd_height = csd_height;
        window->cached_cell_width = cell_width;
        window->cached_cell_height = cell_height;
        window->cached_chrome_width = chrome_width;
        window->cached_chrome_height = chrome_height;

        verbose_print("Cached grid %dx%d cell-size %dx%d chrome %dx%d csd %dx%d\n",
                      columns, rows,
                      window->cached_cell_width, window->cached_cell_height,
                      window->cached_chrome_width, window->cached_chrome_height,
                      window->cached_csd_width, window->cached_csd_height);
}

static void
bteapp_window_resize(BteappWindow* window)
{
        /* Don't do this for maximised or tiled windows. */
        auto win = ctk_widget_get_window(CTK_WIDGET(window));
        if (win != nullptr &&
            (cdk_window_get_state(win) & (CDK_WINDOW_STATE_MAXIMIZED |
                                          CDK_WINDOW_STATE_FULLSCREEN |
                                          CDK_WINDOW_STATE_TILED)) != 0)
                return;

        /* First, update the geometry hints, so that the cached_* members are up-to-date */
        bteapp_window_update_geometry(window);

        /* Calculate the window's pixel size corresponding to the terminal's grid size */
        int columns = bte_terminal_get_column_count(window->terminal);
        int rows = bte_terminal_get_row_count(window->terminal);
        int pixel_width = window->cached_chrome_width + window->cached_cell_width * columns;
        int pixel_height = window->cached_chrome_height + window->cached_cell_height * rows;

        verbose_print("BteappWindow resize grid %dx%d pixel %dx%d\n",
                      columns, rows, pixel_width, pixel_height);

        ctk_window_resize(CTK_WINDOW(window), pixel_width, pixel_height);
}

static void
bteapp_window_parse_geometry(BteappWindow* window)
{
        /* First update the geometry hints, so that ctk_window_parse_geometry()
         * knows the char width/height and base size increments.
         */
        bteapp_window_update_geometry(window);

        if (options.geometry != nullptr) {
                auto rv = ctk_window_parse_geometry(CTK_WINDOW(window), options.geometry);

                if (!rv)
                        verbose_printerr("Failed to parse geometry spec \"%s\"\n", options.geometry);
                else if (!options.no_geometry_hints) {
                        /* After parse_geometry(), we can get the default size in
                         * width/height increments, i.e. in grid size.
                         */
                        int columns, rows;
                        ctk_window_get_default_size(CTK_WINDOW(window), &columns, &rows);
                        bte_terminal_set_size(window->terminal, columns, rows);
                        ctk_window_resize_to_geometry(CTK_WINDOW(window), columns, rows);
                } else {
                        /* Approximate the grid width from the passed pixel size. */
                        int width, height;
                        ctk_window_get_default_size(CTK_WINDOW(window), &width, &height);
                        width -= window->cached_csd_width + window->cached_chrome_width;
                        height -= window->cached_csd_height + window->cached_chrome_height;
                        int columns = width / window->cached_cell_width;
                        int rows = height / window->cached_cell_height;
                        bte_terminal_set_size(window->terminal,
                                              MAX(columns, MIN_COLUMNS),
                                              MAX(rows, MIN_ROWS));
                }
        } else {
                /* In CTK+ 3.0, the default size of a window comes from its minimum
                 * size not its natural size, so we need to set the right default size
                 * explicitly */
                if (!options.no_geometry_hints) {
                        /* Grid based */
                        ctk_window_set_default_geometry(CTK_WINDOW(window),
                                                        bte_terminal_get_column_count(window->terminal),
                                                        bte_terminal_get_row_count(window->terminal));
                } else {
                        /* Pixel based */
                        bteapp_window_resize(window);
                }
        }
}

static void
bteapp_window_adjust_font_size(BteappWindow* window,
                               double factor)
{
        bte_terminal_set_font_scale(window->terminal,
                                    bte_terminal_get_font_scale(window->terminal) * factor);

        bteapp_window_resize(window);
}

static void
window_spawn_cb(BteTerminal* terminal,
                GPid child_pid,
                GError* error,
                void* data)
{
        if (terminal == nullptr) /* terminal destroyed while spawning */
                return;

        BteappWindow* window = BTEAPP_WINDOW(data);
        window->child_pid = child_pid;

        if (child_pid != -1)
                verbose_printerr("Spawning succeded, PID=%ld\n", (long)child_pid);

        if (error != nullptr) {
                verbose_printerr("Spawning failed: %s\n", error->message);

                auto msg = bte::glib::take_string(g_strdup_printf("Spawning failed: %s", error->message));
                if (options.keep)
                        bte_terminal_feed(window->terminal, msg.get(), -1);
                else
                        ctk_widget_destroy(CTK_WIDGET(window));
        }
}

static bool
bteapp_window_launch_argv(BteappWindow* window,
                          char** argv,
                          GError** error)
{
        auto const spawn_flags = GSpawnFlags(G_SPAWN_SEARCH_PATH_FROM_ENVP |
                                             (options.no_systemd_scope ? BTE_SPAWN_NO_SYSTEMD_SCOPE : 0) |
                                             (options.require_systemd_scope ? BTE_SPAWN_REQUIRE_SYSTEMD_SCOPE : 0));
        auto fds = options.fds();
        auto map_fds = options.map_fds();
        bte_terminal_spawn_with_fds_async(window->terminal,
                                          BTE_PTY_DEFAULT,
                                          options.working_directory,
                                          argv,
                                          options.environment,
                                          fds.data(), fds.size(),
                                          map_fds.data(), map_fds.size(),
                                          spawn_flags,
                                          nullptr, nullptr, nullptr, /* child setup, data and destroy */
                                          -1 /* default timeout of 30s */,
                                          nullptr /* cancellable */,
                                          window_spawn_cb, window);
        return true;
}

static bool
bteapp_window_launch_commandline(BteappWindow* window,
                                 char* commandline,
                                 GError** error)
{
        int argc;
        char** argv;
        if (!g_shell_parse_argv(commandline, &argc, &argv, error))
            return false;

        bool rv = bteapp_window_launch_argv(window, argv, error);

        g_strfreev(argv);
        return rv;
}

static bool
bteapp_window_launch_shell(BteappWindow* window,
                           GError** error)
{
        char* shell = bte_get_user_shell();
        if (shell == nullptr || shell[0] == '\0') {
                g_free(shell);
                shell = g_strdup(g_getenv("SHELL"));
        }
        if (shell == nullptr || shell[0] == '\0') {
                g_free(shell);
                shell = g_strdup("/bin/sh");
        }

        char* argv[2] = { shell, nullptr };

        bool rv = bteapp_window_launch_argv(window, argv, error);

        g_free(shell);
        return rv;
}

static bool
bteapp_window_fork(BteappWindow* window,
                   GError** error)
{
        auto pty = bte_pty_new_sync(BTE_PTY_DEFAULT, nullptr, error);
        if (pty == nullptr)
                return false;

        auto pid = fork();
        switch (pid) {
        case -1: { /* error */
                auto errsv = bte::libc::ErrnoSaver{};
                g_set_error(error, G_IO_ERROR, g_io_error_from_errno(errsv),
                            "Error forking: %s", g_strerror(errsv));
                return false;
        }

        case 0: /* child */ {
                bte_pty_child_setup(pty);

                for (auto i = 0; ; i++) {
                        switch (i % 3) {
                        case 0:
                        case 1:
                                g_print("%d\n", i);
                                break;
                        case 2:
                                g_printerr("%d\n", i);
                                break;
                        }
                        g_usleep(G_USEC_PER_SEC);
                }
        }
        default: /* parent */
                bte_terminal_set_pty(window->terminal, pty);
                bte_terminal_watch_child(window->terminal, pid);
                verbose_print("Child PID is %d (mine is %d).\n", (int)pid, (int)getpid());
                break;
        }

        g_object_unref(pty);
        return true;
}

static gboolean
tick_cb(BteappWindow* window)
{
        static int i = 0;
        char buf[256];
        auto s = g_snprintf(buf, sizeof(buf), "%d\r\n", i++);
        bte_terminal_feed(window->terminal, buf, s);
        return G_SOURCE_CONTINUE;
}

static bool
bteapp_window_tick(BteappWindow* window,
                   GError** error)
{
        g_timeout_add_seconds(1, (GSourceFunc) tick_cb, window);
        return true;
}

static void
bteapp_window_launch(BteappWindow* window)
{
        auto rv = bool{};
        auto error = bte::glib::Error{};

        if (options.exec_argv != nullptr)
                rv = bteapp_window_launch_argv(window, options.exec_argv, error);
        else if (options.command != nullptr)
                rv = bteapp_window_launch_commandline(window, options.command, error);
        else if (!options.no_shell)
                rv = bteapp_window_launch_shell(window, error);
        else if (!options.no_pty)
                rv = bteapp_window_fork(window, error);
        else
                rv = bteapp_window_tick(window, error);

        if (!rv)
                verbose_printerr("Error launching: %s\n", error.message());
}

static void
window_update_copy_sensitivity(BteappWindow* window)
{
        auto action = g_action_map_lookup_action(G_ACTION_MAP(window), "copy");
        g_simple_action_set_enabled(G_SIMPLE_ACTION(action),
                                    bte_terminal_get_has_selection(window->terminal));
}

static void
window_update_paste_sensitivity(BteappWindow* window)
{
        CdkAtom* targets;
        int n_targets;

        bool can_paste = false;
        if (ctk_clipboard_wait_for_targets(window->clipboard, &targets, &n_targets)) {
                can_paste = ctk_targets_include_text(targets, n_targets);
                g_free(targets);
        }

        auto action = g_action_map_lookup_action(G_ACTION_MAP(window), "copy");
        g_simple_action_set_enabled(G_SIMPLE_ACTION(action), can_paste);
}

/* Callbacks */

static void
window_action_copy_cb(GSimpleAction* action,
                      GVariant* parameter,
                      void* data)
{
        BteappWindow* window = BTEAPP_WINDOW(data);
        char const* str = g_variant_get_string(parameter, nullptr);

        bte_terminal_copy_clipboard_format(window->terminal,
                                           g_str_equal(str, "html") ? BTE_FORMAT_HTML : BTE_FORMAT_TEXT);

}

static void
window_action_copy_match_cb(GSimpleAction* action,
                            GVariant* parameter,
                            void* data)
{
        BteappWindow* window = BTEAPP_WINDOW(data);
        gsize len;
        char const* str = g_variant_get_string(parameter, &len);
        ctk_clipboard_set_text(window->clipboard, str, len);
}

static void
window_action_paste_cb(GSimpleAction* action,
                       GVariant* parameter,
                       void* data)
{
        BteappWindow* window = BTEAPP_WINDOW(data);
        bte_terminal_paste_clipboard(window->terminal);
}

static void
window_action_reset_cb(GSimpleAction* action,
                       GVariant* parameter,
                       void* data)
{
        BteappWindow* window = BTEAPP_WINDOW(data);
        bool clear;
        CdkModifierType modifiers;

        if (parameter != nullptr)
                clear = g_variant_get_boolean(parameter);
        else if (ctk_get_current_event_state(&modifiers))
                clear = (modifiers & CDK_CONTROL_MASK) != 0;
        else
                clear = false;

        bte_terminal_reset(window->terminal, true, clear);
}

static void
window_action_find_cb(GSimpleAction* action,
                      GVariant* parameter,
                      void* data)
{
        BteappWindow* window = BTEAPP_WINDOW(data);
        ctk_toggle_button_set_active(window->find_button, true);
}


static void
window_action_fullscreen_state_cb (GSimpleAction *action,
                                   GVariant *state,
                                   void* data)
{
        BteappWindow* window = BTEAPP_WINDOW(data);

        if (!ctk_widget_get_realized(CTK_WIDGET(window)))
                return;

        if (g_variant_get_boolean(state))
                ctk_window_fullscreen(CTK_WINDOW(window));
        else
                ctk_window_unfullscreen(CTK_WINDOW(window));

        /* The window-state-changed callback will update the action's actual state */
}

static bool
bteapp_window_show_context_menu(BteappWindow* window,
                                guint button,
                                guint32 timestamp,
                                CdkEvent* event)
{
        if (options.no_context_menu)
                return false;

        auto menu = g_menu_new();
        g_menu_append(menu, "_Copy", "win.copy::text");
        g_menu_append(menu, "Copy As _HTML", "win.copy::html");

        if (event != nullptr) {
                auto hyperlink = bte_terminal_hyperlink_check_event(window->terminal, event);
                if (hyperlink != nullptr) {
                        verbose_print("Hyperlink: %s\n", hyperlink);
                        GVariant* target = g_variant_new_string(hyperlink);
                        auto item = g_menu_item_new("Copy _Hyperlink", nullptr);
                        g_menu_item_set_action_and_target_value(item, "win.copy-match", target);
                        g_menu_append_item(menu, item);
                        g_object_unref(item);
                }

                auto match = bte_terminal_match_check_event(window->terminal, event, nullptr);
                if (match != nullptr) {
                        verbose_print("Match: %s\n", match);
                        GVariant* target = g_variant_new_string(match);
                        auto item = g_menu_item_new("Copy _Match", nullptr);
                        g_menu_item_set_action_and_target_value(item, "win.copy-match", target);
                        g_menu_append_item(menu, item);
                        g_object_unref(item);
                }

                /* Test extra match API */
                static const char extra_pattern[] = "(\\d+)\\s*(\\w+)";
                char* extra_match = nullptr;
                char *extra_subst = nullptr;
                auto error = bte::glib::Error{};
                auto regex = compile_regex_for_match(extra_pattern, false, error);
                error.assert_no_error();
                bte_terminal_event_check_regex_simple(window->terminal, event,
                                                      &regex, 1, 0,
                                                      &extra_match);

                if (extra_match != nullptr &&
                    (extra_subst = bte_regex_substitute(regex, extra_match, "$2 $1",
                                                        PCRE2_SUBSTITUTE_EXTENDED |
                                                        PCRE2_SUBSTITUTE_GLOBAL,
                                                        error)) == nullptr) {
                        verbose_printerr("Substitution failed: %s\n", error.message());
                }

                bte_regex_unref(regex);

                if (extra_match != nullptr) {
                        if (extra_subst != nullptr)
                                verbose_print("%s match: %s => %s\n", extra_pattern, extra_match, extra_subst);
                        else
                                verbose_print("%s match: %s\n", extra_pattern, extra_match);
                }
                g_free(hyperlink);
                g_free(match);
                g_free(extra_match);
                g_free(extra_subst);
        }

        g_menu_append(menu, "_Paste", "win.paste");

        if (window->fullscreen)
                g_menu_append(menu, "_Fullscreen", "win.fullscreen");

        auto popup = ctk_menu_new_from_model(G_MENU_MODEL(menu));
        ctk_menu_attach_to_widget(CTK_MENU(popup), CTK_WIDGET(window->terminal), nullptr);
        ctk_menu_popup(CTK_MENU(popup), nullptr, nullptr, nullptr, nullptr, button, timestamp);
        if (button == 0)
                ctk_menu_shell_select_first(CTK_MENU_SHELL(popup), true);

        return true;
}

static gboolean
window_popup_menu_cb(CtkWidget* widget,
                     BteappWindow* window)
{
        return bteapp_window_show_context_menu(window, 0, ctk_get_current_event_time(), nullptr);
}

static gboolean
window_button_press_cb(CtkWidget* widget,
                       CdkEventButton* event,
                       BteappWindow* window)
{
        if (event->button != CDK_BUTTON_SECONDARY)
                return false;

        return bteapp_window_show_context_menu(window, event->button, event->time,
                                               reinterpret_cast<CdkEvent*>(event));
}

static void
window_cell_size_changed_cb(BteTerminal* term,
                            guint width,
                            guint height,
                            BteappWindow* window)
{
        bteapp_window_update_geometry(window);
}

static void
window_child_exited_cb(BteTerminal* term,
                       int status,
                       BteappWindow* window)
{
        if (WIFEXITED(status))
                verbose_printerr("Child exited with status %x\n", WEXITSTATUS(status));
        else if (WIFSIGNALED(status))
                verbose_printerr("Child terminated by signal %d\n", WTERMSIG(status));
        else
                verbose_printerr("Child terminated\n");

        if (options.output_filename != nullptr) {
                auto file = g_file_new_for_commandline_arg(options.output_filename);
                auto error = bte::glib::Error{};
                auto stream = g_file_replace(file, nullptr, false, G_FILE_CREATE_NONE, nullptr, error);
                g_object_unref(file);
                if (stream != nullptr) {
                        bte_terminal_write_contents_sync(window->terminal,
                                                         G_OUTPUT_STREAM(stream),
                                                         BTE_WRITE_DEFAULT,
                                                         nullptr, error);
                        g_object_unref(stream);
                }

                if (error.error()) {
                        verbose_printerr("Failed to write output to \"%s\": %s\n",
                                         options.output_filename, error.message());
                }
        }

        window->child_pid = -1;

        if (options.keep)
                return;

        ctk_widget_destroy(CTK_WIDGET(window));
}

static void
window_clipboard_owner_change_cb(CtkClipboard* clipboard,
                                 CdkEvent* event,
                                 BteappWindow* window)
{
        window_update_paste_sensitivity(window);
}

static void
window_decrease_font_size_cb(BteTerminal* terminal,
                             BteappWindow* window)
{
        bteapp_window_adjust_font_size(window, 1.0 / 1.2);
}

static void
window_increase_font_size_cb(BteTerminal* terminal,
                             BteappWindow* window)
{
        bteapp_window_adjust_font_size(window, 1.2);
}

static void
window_deiconify_window_cb(BteTerminal* terminal,
                           BteappWindow* window)
{
        if (!options.allow_window_ops)
                return;

        ctk_window_deiconify(CTK_WINDOW(window));
}

static void
window_iconify_window_cb(BteTerminal* terminal,
                         BteappWindow* window)
{
        if (!options.allow_window_ops)
                return;

        ctk_window_iconify(CTK_WINDOW(window));
}

static void
window_icon_title_changed_cb(BteTerminal* terminal,
                         BteappWindow* window)
{
        if (!options.icon_title)
                return;

        cdk_window_set_icon_name(ctk_widget_get_window(CTK_WIDGET(window)),
                                 bte_terminal_get_icon_title(window->terminal));
}

static void
window_window_title_changed_cb(BteTerminal* terminal,
                               BteappWindow* window)
{
        ctk_window_set_title(CTK_WINDOW(window),
                             bte_terminal_get_window_title(window->terminal));
}

static void
window_lower_window_cb(BteTerminal* terminal,
                       BteappWindow* window)
{
        if (!options.allow_window_ops)
                return;
        if (!ctk_widget_get_realized(CTK_WIDGET(window)))
                return;

        cdk_window_lower(ctk_widget_get_window(CTK_WIDGET(window)));
}

static void
window_raise_window_cb(BteTerminal* terminal,
                       BteappWindow* window)
{
        if (!options.allow_window_ops)
                return;
        if (!ctk_widget_get_realized(CTK_WIDGET(window)))
                return;

        cdk_window_raise(ctk_widget_get_window(CTK_WIDGET(window)));
}

static void
window_maximize_window_cb(BteTerminal* terminal,
                          BteappWindow* window)
{
        if (!options.allow_window_ops)
                return;

        ctk_window_maximize(CTK_WINDOW(window));
}

static void
window_restore_window_cb(BteTerminal* terminal,
                         BteappWindow* window)
{
        if (!options.allow_window_ops)
                return;

        ctk_window_unmaximize(CTK_WINDOW(window));
}

static void
window_move_window_cb(BteTerminal* terminal,
                      guint x,
                      guint y,
                      BteappWindow* window)
{
        if (!options.allow_window_ops)
                return;

        ctk_window_move(CTK_WINDOW(window), x, y);
}

static void
window_notify_cb(GObject* object,
                 GParamSpec* pspec,
                 BteappWindow* window)
{
        if (pspec->owner_type != BTE_TYPE_TERMINAL)
                return;

        GValue value G_VALUE_INIT;
        g_object_get_property(object, pspec->name, &value);
        auto str = g_strdup_value_contents(&value);
        g_value_unset(&value);

        verbose_print("NOTIFY property \"%s\" value %s\n", pspec->name, str);
        g_free(str);
}

static void
window_refresh_window_cb(BteTerminal* terminal,
                         BteappWindow* window)
{
        ctk_widget_queue_draw(CTK_WIDGET(window));
}

static void
window_resize_window_cb(BteTerminal* terminal,
                        guint columns,
                        guint rows,
                        BteappWindow* window)
{
        if (!options.allow_window_ops)
                return;
        if (columns < MIN_COLUMNS || rows < MIN_ROWS)
                return;

        bte_terminal_set_size(window->terminal, columns, rows);
        bteapp_window_resize(window);
}

static void
window_selection_changed_cb(BteTerminal* terminal,
                            BteappWindow* window)
{
        window_update_copy_sensitivity(window);
}

static void
window_input_enabled_state_cb(GAction* action,
                              GParamSpec* pspec,
                              BteappWindow* window)
{
        ctk_widget_set_visible(window->readonly_emblem,
                               !g_variant_get_boolean(g_action_get_state(action)));
}

static void
window_search_popover_closed_cb(CtkPopover* popover,
                                BteappWindow* window)
{
        if (ctk_toggle_button_get_active(window->find_button))
                ctk_toggle_button_set_active(window->find_button, false);
}

static void
window_find_button_toggled_cb(CtkToggleButton* button,
                              BteappWindow* window)
{
        auto active = ctk_toggle_button_get_active(button);

        if (ctk_widget_get_visible(CTK_WIDGET(window->search_popover)) != active)
                ctk_widget_set_visible(CTK_WIDGET(window->search_popover), active);
}

G_DEFINE_TYPE(BteappWindow, bteapp_window, CTK_TYPE_APPLICATION_WINDOW)

static void
bteapp_window_init(BteappWindow* window)
{
        ctk_widget_init_template(CTK_WIDGET(window));

        window->child_pid = -1;
}

static void
bteapp_window_constructed(GObject *object)
{
        BteappWindow* window = BTEAPP_WINDOW(object);

        G_OBJECT_CLASS(bteapp_window_parent_class)->constructed(object);

        ctk_window_set_title(CTK_WINDOW(window), "Terminal");

        if (options.no_decorations)
                ctk_window_set_decorated(CTK_WINDOW(window), false);

        /* Create terminal and connect scrollbar */
        window->terminal = reinterpret_cast<BteTerminal*>(bteapp_terminal_new());

        auto margin = options.extra_margin;
        if (margin >= 0) {
                ctk_widget_set_margin_start(CTK_WIDGET(window->terminal), margin);
                ctk_widget_set_margin_end(CTK_WIDGET(window->terminal), margin);
                ctk_widget_set_margin_top(CTK_WIDGET(window->terminal), margin);
                ctk_widget_set_margin_bottom(CTK_WIDGET(window->terminal), margin);
        }

        ctk_range_set_adjustment(CTK_RANGE(window->scrollbar),
                                 ctk_scrollable_get_vadjustment(CTK_SCROLLABLE(window->terminal)));
        if (options.no_scrollbar) {
                ctk_widget_destroy(CTK_WIDGET(window->scrollbar));
                window->scrollbar = nullptr;
        }

        /* Create actions */
        GActionEntry const entries[] = {
                { .name = "copy",        .activate = window_action_copy_cb,       .parameter_type = "s",     .state = nullptr, .change_state = nullptr, .padding = { 0 } },
                { .name = "copy-match",  .activate = window_action_copy_match_cb, .parameter_type = "s",     .state = nullptr, .change_state = nullptr, .padding = { 0 } },
                { .name = "paste",       .activate = window_action_paste_cb,      .parameter_type = nullptr, .state = nullptr, .change_state = nullptr, .padding = { 0 } },
                { .name = "reset",       .activate = window_action_reset_cb,      .parameter_type = "b",     .state = nullptr, .change_state = nullptr, .padding = { 0 } },
                { .name = "find",        .activate = window_action_find_cb,       .parameter_type = nullptr, .state = nullptr, .change_state = nullptr, .padding = { 0 } },
                { .name = "fullscreen",  .activate = nullptr,                     .parameter_type = nullptr, .state = "false", .change_state = window_action_fullscreen_state_cb, .padding = { 0 } },
        };

        GActionMap* map = G_ACTION_MAP(window);
        g_action_map_add_action_entries(map, entries, G_N_ELEMENTS(entries), window);

        /* Property actions */
        auto action = g_property_action_new("input-enabled", window->terminal, "input-enabled");
        g_action_map_add_action(map, G_ACTION(action));
        g_object_unref(action);
        g_signal_connect(action, "notify::state", G_CALLBACK(window_input_enabled_state_cb), window);

        /* Find */
        window->search_popover = bteapp_search_popover_new(window->terminal,
                                                           CTK_WIDGET(window->find_button));
        g_signal_connect(window->search_popover, "closed",
                         G_CALLBACK(window_search_popover_closed_cb), window);
        g_signal_connect(window->find_button, "toggled",
                         G_CALLBACK(window_find_button_toggled_cb), window);

        /* Clipboard */
        window->clipboard = ctk_widget_get_clipboard(CTK_WIDGET(window), CDK_SELECTION_CLIPBOARD);
        g_signal_connect(window->clipboard, "owner-change", G_CALLBACK(window_clipboard_owner_change_cb), window);

        /* Set ARGB visual */
        if (options.transparency_percent >= 0) {
                if (!options.no_argb_visual) {
                        auto screen = ctk_widget_get_screen(CTK_WIDGET(window));
                        auto visual = cdk_screen_get_rgba_visual(screen);
                        if (visual != nullptr)
                                ctk_widget_set_visual(CTK_WIDGET(window), visual);
       }

                /* Without this transparency doesn't work; see bug #729884. */
                ctk_widget_set_app_paintable(CTK_WIDGET(window), true);
        }

        /* Signals */
        g_signal_connect(window->terminal, "popup-menu", G_CALLBACK(window_popup_menu_cb), window);
        g_signal_connect(window->terminal, "button-press-event", G_CALLBACK(window_button_press_cb), window);
        g_signal_connect(window->terminal, "char-size-changed", G_CALLBACK(window_cell_size_changed_cb), window);
        g_signal_connect(window->terminal, "child-exited", G_CALLBACK(window_child_exited_cb), window);
        g_signal_connect(window->terminal, "decrease-font-size", G_CALLBACK(window_decrease_font_size_cb), window);
        g_signal_connect(window->terminal, "deiconify-window", G_CALLBACK(window_deiconify_window_cb), window);
        g_signal_connect(window->terminal, "icon-title-changed", G_CALLBACK(window_icon_title_changed_cb), window);
        g_signal_connect(window->terminal, "iconify-window", G_CALLBACK(window_iconify_window_cb), window);
        g_signal_connect(window->terminal, "increase-font-size", G_CALLBACK(window_increase_font_size_cb), window);
        g_signal_connect(window->terminal, "lower-window", G_CALLBACK(window_lower_window_cb), window);
        g_signal_connect(window->terminal, "maximize-window", G_CALLBACK(window_maximize_window_cb), window);
        g_signal_connect(window->terminal, "move-window", G_CALLBACK(window_move_window_cb), window);
        g_signal_connect(window->terminal, "raise-window", G_CALLBACK(window_raise_window_cb), window);
        g_signal_connect(window->terminal, "refresh-window", G_CALLBACK(window_refresh_window_cb), window);
        g_signal_connect(window->terminal, "resize-window", G_CALLBACK(window_resize_window_cb), window);
        g_signal_connect(window->terminal, "restore-window", G_CALLBACK(window_restore_window_cb), window);
        g_signal_connect(window->terminal, "selection-changed", G_CALLBACK(window_selection_changed_cb), window);
        g_signal_connect(window->terminal, "window-title-changed", G_CALLBACK(window_window_title_changed_cb), window);
        if (options.object_notifications)
                g_signal_connect(window->terminal, "notify", G_CALLBACK(window_notify_cb), window);

        /* Settings */
        if (options.no_double_buffer) {
                ctk_widget_set_double_buffered(CTK_WIDGET(window->terminal), false);
        }

        if (options.encoding != nullptr) {
                auto error = bte::glib::Error{};
                if (!bte_terminal_set_encoding(window->terminal, options.encoding, error))
                        g_printerr("Failed to set encoding: %s\n", error.message());
        }

        if (options.word_char_exceptions != nullptr)
                bte_terminal_set_word_char_exceptions(window->terminal, options.word_char_exceptions);

        bte_terminal_set_allow_hyperlink(window->terminal, !options.no_hyperlink);
        bte_terminal_set_audible_bell(window->terminal, options.audible_bell);
        bte_terminal_set_allow_bold(window->terminal, !options.no_bold);
        bte_terminal_set_bold_is_bright(window->terminal, options.bold_is_bright);
        bte_terminal_set_cell_height_scale(window->terminal, options.cell_height_scale);
        bte_terminal_set_cell_width_scale(window->terminal, options.cell_width_scale);
        bte_terminal_set_cjk_ambiguous_width(window->terminal, options.cjk_ambiguous_width);
        bte_terminal_set_cursor_blink_mode(window->terminal, options.cursor_blink_mode);
        bte_terminal_set_cursor_shape(window->terminal, options.cursor_shape);
        bte_terminal_set_enable_bidi(window->terminal, !options.no_bidi);
        bte_terminal_set_enable_shaping(window->terminal, !options.no_shaping);
        bte_terminal_set_mouse_autohide(window->terminal, true);
        bte_terminal_set_rewrap_on_resize(window->terminal, !options.no_rewrap);
        bte_terminal_set_scroll_on_output(window->terminal, false);
        bte_terminal_set_scroll_on_keystroke(window->terminal, true);
        bte_terminal_set_scrollback_lines(window->terminal, options.scrollback_lines);
        bte_terminal_set_text_blink_mode(window->terminal, options.text_blink_mode);

        /* Style */
        if (options.font_string != nullptr) {
                auto desc = pango_font_description_from_string(options.font_string);
                bte_terminal_set_font(window->terminal, desc);
                pango_font_description_free(desc);
        }

        auto fg = options.get_color_fg();
        auto bg = options.get_color_bg();
        bte_terminal_set_colors(window->terminal, &fg, &bg, nullptr, 0);
        if (options.cursor_bg_color_set)
                bte_terminal_set_color_cursor(window->terminal, &options.cursor_bg_color);
        if (options.cursor_fg_color_set)
                bte_terminal_set_color_cursor_foreground(window->terminal, &options.cursor_fg_color);
        if (options.hl_bg_color_set)
                bte_terminal_set_color_highlight(window->terminal, &options.hl_bg_color);
        if (options.hl_fg_color_set)
                bte_terminal_set_color_highlight_foreground(window->terminal, &options.hl_fg_color);

        if (options.whole_window_transparent)
                ctk_widget_set_opacity (CTK_WIDGET (window), options.get_alpha());

        /* Dingus */
        if (!options.no_builtin_dingus)
                bteapp_window_add_dingus(window, builtin_dingus);
        if (options.dingus != nullptr)
                bteapp_window_add_dingus(window, options.dingus);

        /* Done! */
        ctk_box_pack_start(CTK_BOX(window->window_box), CTK_WIDGET(window->terminal),
                           true, true, 0);
        ctk_widget_show(CTK_WIDGET(window->terminal));

        window_update_paste_sensitivity(window);
        window_update_copy_sensitivity(window);

        ctk_widget_grab_focus(CTK_WIDGET(window->terminal));

        /* Sanity check */
        g_assert(!ctk_widget_get_realized(CTK_WIDGET(window)));
}

static void
bteapp_window_dispose(GObject *object)
{
        BteappWindow* window = BTEAPP_WINDOW(object);

        if (window->clipboard != nullptr) {
                g_signal_handlers_disconnect_by_func(window->clipboard,
                                                     (void*)window_clipboard_owner_change_cb,
                                                     window);
                window->clipboard = nullptr;
        }

        if (window->search_popover != nullptr) {
                ctk_widget_destroy(window->search_popover);
                window->search_popover = nullptr;
        }

        G_OBJECT_CLASS(bteapp_window_parent_class)->dispose(object);
}

static void
bteapp_window_realize(CtkWidget* widget)
{
        CTK_WIDGET_CLASS(bteapp_window_parent_class)->realize(widget);

        /* Now we can know the CSD size, and thus apply the geometry. */
        BteappWindow* window = BTEAPP_WINDOW(widget);
        verbose_print("BteappWindow::realize\n");
        bteapp_window_resize(window);
}

static void
bteapp_window_show(CtkWidget* widget)
{
        CTK_WIDGET_CLASS(bteapp_window_parent_class)->show(widget);

        /* Re-apply the geometry. */
        BteappWindow* window = BTEAPP_WINDOW(widget);
        verbose_print("BteappWindow::show\n");
        bteapp_window_resize(window);
}

static void
bteapp_window_style_updated(CtkWidget* widget)
{
        CTK_WIDGET_CLASS(bteapp_window_parent_class)->style_updated(widget);

        /* Re-apply the geometry. */
        BteappWindow* window = BTEAPP_WINDOW(widget);
        verbose_print("BteappWindow::style-update\n");
        bteapp_window_resize(window);
}

static gboolean
bteapp_window_state_event (CtkWidget* widget,
                           CdkEventWindowState* event)
{
        BteappWindow* window = BTEAPP_WINDOW(widget);

        if (event->changed_mask & CDK_WINDOW_STATE_FULLSCREEN) {
                window->fullscreen = (event->new_window_state & CDK_WINDOW_STATE_FULLSCREEN) != 0;

                auto action = reinterpret_cast<GSimpleAction*>(g_action_map_lookup_action(G_ACTION_MAP(window), "fullscreen"));
                g_simple_action_set_state(action, g_variant_new_boolean (window->fullscreen));
        }

        return CTK_WIDGET_CLASS(bteapp_window_parent_class)->window_state_event(widget, event);
}

static void
bteapp_window_class_init(BteappWindowClass* klass)
{
        GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
        gobject_class->constructed  = bteapp_window_constructed;
        gobject_class->dispose  = bteapp_window_dispose;

        CtkWidgetClass* widget_class = CTK_WIDGET_CLASS(klass);
        widget_class->realize = bteapp_window_realize;
        widget_class->show = bteapp_window_show;
        widget_class->style_updated = bteapp_window_style_updated;
        widget_class->window_state_event = bteapp_window_state_event;

        ctk_widget_class_set_template_from_resource(widget_class, "/org/gnome/bte/app/ui/window.ui");
        ctk_widget_class_set_css_name(widget_class, "bteapp-window");

        ctk_widget_class_bind_template_child(widget_class, BteappWindow, window_box);
        ctk_widget_class_bind_template_child(widget_class, BteappWindow, scrollbar);
        /* ctk_widget_class_bind_template_child(widget_class, BteappWindow, notification_box); */
        ctk_widget_class_bind_template_child(widget_class, BteappWindow, readonly_emblem);
        /* ctk_widget_class_bind_template_child(widget_class, BteappWindow, copy_button); */
        /* ctk_widget_class_bind_template_child(widget_class, BteappWindow, paste_button); */
        ctk_widget_class_bind_template_child(widget_class, BteappWindow, find_button);
        ctk_widget_class_bind_template_child(widget_class, BteappWindow, gear_button);
}

static BteappWindow*
bteapp_window_new(GApplication* app)
{
        return reinterpret_cast<BteappWindow*>(g_object_new(BTEAPP_TYPE_WINDOW,
                                                            "application", app,
                                                            nullptr));
}

/* application */

#define BTEAPP_TYPE_APPLICATION         (bteapp_application_get_type())
#define BTEAPP_APPLICATION(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), BTEAPP_TYPE_APPLICATION, BteappApplication))
#define BTEAPP_APPLICATION_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BTEAPP_TYPE_APPLICATION, BteappApplicationClass))
#define BTEAPP_IS_APPLICATION(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), BTEAPP_TYPE_APPLICATION))
#define BTEAPP_IS_APPLICATION_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), BTEAPP_TYPE_APPLICATION))
#define BTEAPP_APPLICATION_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), BTEAPP_TYPE_APPLICATION, BteappApplicationClass))

typedef struct _BteappApplication       BteappApplication;
typedef struct _BteappApplicationClass  BteappApplicationClass;

struct _BteappApplication {
        CtkApplication parent;

        guint input_source;
};

struct _BteappApplicationClass {
        CtkApplicationClass parent;
};

static GType bteapp_application_get_type(void);

static void
app_action_new_cb(GSimpleAction* action,
                  GVariant* parameter,
                  void* data)
{
        GApplication* application = G_APPLICATION(data);
        auto window = bteapp_window_new(application);
        bteapp_window_parse_geometry(window);
        ctk_window_present(CTK_WINDOW(window));
        bteapp_window_launch(window);
}

static void
app_action_close_cb(GSimpleAction* action,
                    GVariant* parameter,
                    void* data)
{
        CtkApplication* application = CTK_APPLICATION(data);
        auto window = ctk_application_get_active_window(application);
        if (window != nullptr)
                ctk_widget_destroy(CTK_WIDGET(window));
}

static gboolean
app_stdin_readable_cb(int fd,
                      GIOCondition condition,
                      BteappApplication* application)
{
        auto eos = bool{false};
        if (condition & G_IO_IN) {
                auto window = ctk_application_get_active_window(CTK_APPLICATION(application));
                auto terminal = BTEAPP_IS_WINDOW(window) ? BTEAPP_WINDOW(window)->terminal : nullptr;

                char buf[4096];
                auto r = int{0};
                do {
                        errno = 0;
                        r = read(fd, buf, sizeof(buf));
                        if (r > 0 && terminal != nullptr)
                                bte_terminal_feed(terminal, buf, r);
                } while (r > 0 || errno == EINTR);

                if (r == 0)
                        eos = true;
        }

        if (eos) {
                application->input_source = 0;
                return G_SOURCE_REMOVE;
        }

        return G_SOURCE_CONTINUE;
}

G_DEFINE_TYPE(BteappApplication, bteapp_application, CTK_TYPE_APPLICATION)

static void
bteapp_application_init(BteappApplication* application)
{
        g_object_set(ctk_settings_get_default(),
                     "ctk-enable-mnemonics", FALSE,
                     "ctk-enable-accels", FALSE,
                     /* Make ctk+ CSD not steal F10 from the terminal */
                     "ctk-menu-bar-accel", nullptr,
                     nullptr);

        if (options.css) {
                ctk_style_context_add_provider_for_screen(cdk_screen_get_default (),
                                                          CTK_STYLE_PROVIDER(options.css.get()),
                                                          CTK_STYLE_PROVIDER_PRIORITY_USER);
        }

        if (options.feed_stdin) {
                g_unix_set_fd_nonblocking(STDIN_FILENO, true, nullptr);
                application->input_source = g_unix_fd_add(STDIN_FILENO,
                                                          GIOCondition(G_IO_IN | G_IO_HUP | G_IO_ERR),
                                                          (GUnixFDSourceFunc)app_stdin_readable_cb,
                                                          application);
        }
}

static void
bteapp_application_dispose(GObject* object)
{
        BteappApplication* application = BTEAPP_APPLICATION(object);

        if (application->input_source != 0) {
                g_source_remove(application->input_source);
                application->input_source = 0;
        }

        G_OBJECT_CLASS(bteapp_application_parent_class)->dispose(object);
}

static void
bteapp_application_startup(GApplication* application)
{
        /* Create actions */
        GActionEntry const entries[] = {
                { .name = "new",   .activate = app_action_new_cb,   .parameter_type = nullptr, .state = nullptr, .change_state = nullptr, .padding = { 0 } },
                { .name = "close", .activate = app_action_close_cb, .parameter_type = nullptr, .state = nullptr, .change_state = nullptr, .padding = { 0 } },
        };

        GActionMap* map = G_ACTION_MAP(application);
        g_action_map_add_action_entries(map, entries, G_N_ELEMENTS(entries), application);

        g_application_set_resource_base_path (application, "/org/gnome/bte/app");

        G_APPLICATION_CLASS(bteapp_application_parent_class)->startup(application);
}

static void
bteapp_application_activate(GApplication* application)
{
        auto action = g_action_map_lookup_action(G_ACTION_MAP(application), "new");
        g_action_activate(action, nullptr);
}

static void
bteapp_application_class_init(BteappApplicationClass* klass)
{
        GObjectClass* object_class = G_OBJECT_CLASS(klass);
        object_class->dispose = bteapp_application_dispose;

        GApplicationClass* application_class = G_APPLICATION_CLASS(klass);
        application_class->startup = bteapp_application_startup;
        application_class->activate = bteapp_application_activate;
}

static GApplication*
bteapp_application_new(void)
{
        return reinterpret_cast<GApplication*>(g_object_new(BTEAPP_TYPE_APPLICATION,
                                                            "application-id", "org.gnome.Bte.Application",
                                                            "flags", guint(G_APPLICATION_NON_UNIQUE),
                                                            nullptr));
}

/* main */

int
main(int argc,
     char *argv[])
{
        setlocale(LC_ALL, "");

	bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");

        /* Not interested in silly debug spew, bug #749195 */
        if (g_getenv("G_ENABLE_DIAGNOSTIC") == nullptr)
                g_setenv("G_ENABLE_DIAGNOSTIC", "0", true);

       g_set_prgname("Terminal");
       g_set_application_name("Terminal");

       auto error = bte::glib::Error{};
       if (!options.parse_argv(argc, argv, error)) {
               g_printerr("Error parsing arguments: %s\n", error.message());
               return EXIT_FAILURE;
       }

        if (g_getenv("BTE_CJK_WIDTH") != nullptr)
                verbose_printerr("BTE_CJK_WIDTH is not supported anymore, use --cjk-width instead\n");

       if (options.version) {
               g_print("BTE Application %s %s\n", VERSION, bte_get_features());
               return EXIT_SUCCESS;
       }

       if (options.debug)
               cdk_window_set_debug_updates(true);
#ifdef BTE_DEBUG
       if (options.test_mode) {
               bte_set_test_flags(BTE_TEST_FLAGS_ALL);
               options.allow_window_ops = true;
       }
#endif

       auto reset_termios = bool{false};
       struct termios saved_tcattr;
       if (options.feed_stdin && isatty(STDIN_FILENO)) {
               /* Put terminal in raw mode */

               struct termios tcattr;
               if (tcgetattr(STDIN_FILENO, &tcattr) == 0) {
                       saved_tcattr = tcattr;
                       cfmakeraw(&tcattr);
                       if (tcsetattr(STDIN_FILENO, TCSANOW, &tcattr) == 0)
                               reset_termios = true;
               }
       }

       auto app = bteapp_application_new();
       auto rv = g_application_run(app, 0, nullptr);
       g_object_unref(app);

       if (reset_termios)
               tcsetattr(STDIN_FILENO, TCSANOW, &saved_tcattr);

       return rv;
}
