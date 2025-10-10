/*
 * Copyright (C) 2001,2002 Red Hat, Inc.
 * Copyright © 2009, 2010, 2019 Christian Persch
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/**
 * SECTION: bte-pty
 * @short_description: Functions for starting a new process on a new pseudo-terminal and for
 * manipulating pseudo-terminals
 *
 * The terminal widget uses these functions to start commands with new controlling
 * pseudo-terminals and to resize pseudo-terminals.
 */

#include "config.h"

#include <exception>

#include <bte/bte.h>

#include <errno.h>
#include <glib.h>
#include <gio/gio.h>
#include "debug.h"

#include <glib/gi18n-lib.h>

#include "cxx-utils.hh"
#include "libc-glue.hh"
#include "pty.hh"
#include "refptr.hh"
#include "spawn.hh"

#include "bteptyinternal.hh"

#define I_(string) (g_intern_static_string(string))

typedef struct _BtePtyPrivate BtePtyPrivate;

typedef struct {
	GSpawnChildSetupFunc extra_child_setup;
	gpointer extra_child_setup_data;
} BtePtyChildSetupData;

/**
 * BtePty:
 */
struct _BtePty {
        GObject parent_instance;

        /* <private> */
        BtePtyPrivate *priv;
};

struct _BtePtyPrivate {
        bte::base::Pty* pty; /* owned */
        int foreign_fd; /* foreign FD if  != -1 */
        BtePtyFlags flags;
};

struct _BtePtyClass {
        GObjectClass parent_class;
};

bte::base::Pty*
_bte_pty_get_impl(BtePty* pty)
{
        return pty->priv->pty;
}

#define IMPL(wrapper) (_bte_pty_get_impl(wrapper))

/**
 * BTE_SPAWN_NO_PARENT_ENVV:
 *
 * Use this as a spawn flag (together with flags from #GSpawnFlags) in
 * bte_pty_spawn_async().
 *
 * Normally, the spawned process inherits the environment from the parent
 * process; when this flag is used, only the environment variables passed
 * to bte_pty_spawn_async() etc. are passed to the child process.
 */

/**
 * BTE_SPAWN_NO_SYSTEMD_SCOPE:
 *
 * Use this as a spawn flag (together with flags from #GSpawnFlags) in
 * bte_pty_spawn_async().
 *
 * Prevents bte_pty_spawn_async() etc. from moving the newly created child
 * process to a systemd user scope.
 *
 * Since: 0.60
 */

/**
 * BTE_SPAWN_REQUIRE_SYSTEMD_SCOPE
 *
 * Use this as a spawn flag (together with flags from #GSpawnFlags) in
 * bte_pty_spawn_async().
 *
 * Requires bte_pty_spawn_async() etc. to move the newly created child
 * process to a systemd user scope; if that fails, the whole spawn fails.
 *
 * This is supported on Linux only.
 *
 * Since: 0.60
 */

/**
 * bte_pty_child_setup:
 * @pty: a #BtePty
 */
void
bte_pty_child_setup (BtePty *pty) noexcept
try
{
        g_return_if_fail(pty != nullptr);
        auto impl = IMPL(pty);
        g_return_if_fail(impl != nullptr);

        impl->child_setup();
}
catch (...)
{
        bte::log_exception();
}

/**
 * bte_pty_set_size:
 * @pty: a #BtePty
 * @rows: the desired number of rows
 * @columns: the desired number of columns
 * @error: (allow-none): return location to store a #GError, or %NULL
 *
 * Attempts to resize the pseudo terminal's window size.  If successful, the
 * OS kernel will send #SIGWINCH to the child process group.
 *
 * If setting the window size failed, @error will be set to a #GIOError.
 *
 * Returns: %TRUE on success, %FALSE on failure with @error filled in
 */
gboolean
bte_pty_set_size(BtePty *pty,
                 int rows,
                 int columns,
                 GError **error) noexcept
{
        /* No way to determine the pixel size; set it to (0, 0), meaning
         * "undefined".
         */
        return _bte_pty_set_size(pty, rows, columns, 0, 0, error);
}

bool
_bte_pty_set_size(BtePty *pty,
                  int rows,
                  int columns,
                  int cell_height_px,
                  int cell_width_px,
                  GError **error) noexcept
try
{
        g_return_val_if_fail(BTE_IS_PTY(pty), FALSE);
        auto impl = IMPL(pty);
        g_return_val_if_fail(impl != nullptr, FALSE);

        if (impl->set_size(rows, columns, cell_height_px, cell_width_px))
                return true;

        auto errsv = bte::libc::ErrnoSaver{};
        g_set_error(error, G_IO_ERROR,
                    g_io_error_from_errno(errsv),
                    "Failed to set window size: %s",
                    g_strerror(errsv));

        return false;
}
catch (...)
{
        return bte::glib::set_error_from_exception(error);
}


/**
 * bte_pty_get_size:
 * @pty: a #BtePty
 * @rows: (out) (allow-none): a location to store the number of rows, or %NULL
 * @columns: (out) (allow-none): a location to store the number of columns, or %NULL
 * @error: return location to store a #GError, or %NULL
 *
 * Reads the pseudo terminal's window size.
 *
 * If getting the window size failed, @error will be set to a #GIOError.
 *
 * Returns: %TRUE on success, %FALSE on failure with @error filled in
 */
gboolean
bte_pty_get_size(BtePty *pty,
                 int *rows,
                 int *columns,
                 GError **error) noexcept
try
{
        g_return_val_if_fail(BTE_IS_PTY(pty), FALSE);
        auto impl = IMPL(pty);
        g_return_val_if_fail(impl != nullptr, FALSE);

        if (impl->get_size(rows, columns))
                return true;

        auto errsv = bte::libc::ErrnoSaver{};
        g_set_error(error, G_IO_ERROR,
                    g_io_error_from_errno(errsv),
                    "Failed to get window size: %s",
                    g_strerror(errsv));
        return false;
}
catch (...)
{
        return bte::glib::set_error_from_exception(error);
}

/**
 * bte_pty_set_utf8:
 * @pty: a #BtePty
 * @utf8: whether or not the pty is in UTF-8 mode
 * @error: (allow-none): return location to store a #GError, or %NULL
 *
 * Tells the kernel whether the terminal is UTF-8 or not, in case it can make
 * use of the info.  Linux 2.6.5 or so defines IUTF8 to make the line
 * discipline do multibyte backspace correctly.
 *
 * Returns: %TRUE on success, %FALSE on failure with @error filled in
 */
gboolean
bte_pty_set_utf8(BtePty *pty,
                 gboolean utf8,
                 GError **error) noexcept
try
{
        g_return_val_if_fail(BTE_IS_PTY(pty), FALSE);
        auto impl = IMPL(pty);
        g_return_val_if_fail(impl != nullptr, FALSE);

        if (impl->set_utf8(utf8))
                return true;

        auto errsv = bte::libc::ErrnoSaver{};
        g_set_error(error, G_IO_ERROR, g_io_error_from_errno(errsv),
                    "%s failed: %s", "tc[sg]etattr", g_strerror(errsv));
        return false;
}
catch (...)
{
        return bte::glib::set_error_from_exception(error);
}

/**
 * bte_pty_close:
 * @pty: a #BtePty
 *
 * Since 0.42 this is a no-op.
 *
 * Deprecated: 0.42
 */
void
bte_pty_close (BtePty *pty) noexcept
{
        /* impl->close(); */
}

/* BTE PTY class */

enum {
        PROP_0,
        PROP_FLAGS,
        PROP_FD,
};

/* GInitable impl */

static gboolean
bte_pty_initable_init (GInitable *initable,
                       GCancellable *cancellable,
                       GError **error) noexcept
try
{
        BtePty *pty = BTE_PTY (initable);
        BtePtyPrivate *priv = pty->priv;

        if (priv->foreign_fd != -1) {
                priv->pty = bte::base::Pty::create_foreign(priv->foreign_fd, priv->flags);
                priv->foreign_fd = -1;
        } else {
                priv->pty = bte::base::Pty::create(priv->flags);
        }

        if (priv->pty == nullptr) {
                auto errsv = bte::libc::ErrnoSaver{};
                g_set_error(error, G_IO_ERROR, g_io_error_from_errno(errsv),
                            "Failed to open PTY: %s", g_strerror(errsv));
                return FALSE;
        }

        return !g_cancellable_set_error_if_cancelled(cancellable, error);
}
catch (...)
{
        return bte::glib::set_error_from_exception(error);
}

static void
bte_pty_initable_iface_init (GInitableIface  *iface)
{
        iface->init = bte_pty_initable_init;
}

/* GObjectClass impl */

G_DEFINE_TYPE_WITH_CODE (BtePty, bte_pty, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (BtePty)
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, bte_pty_initable_iface_init))

static void
bte_pty_init (BtePty *pty)
{
        BtePtyPrivate *priv;

        priv = pty->priv = (BtePtyPrivate *)bte_pty_get_instance_private (pty);

        priv->pty = nullptr;
        priv->foreign_fd = -1;
        priv->flags = BTE_PTY_DEFAULT;
}

static void
bte_pty_finalize (GObject *object) noexcept
try
{
        BtePty *pty = BTE_PTY (object);
        BtePtyPrivate *priv = pty->priv;

        auto implptr = bte::base::RefPtr<bte::base::Pty>{priv->pty}; // moved

        G_OBJECT_CLASS (bte_pty_parent_class)->finalize (object);
}
catch (...)
{
        bte::log_exception();
}

static void
bte_pty_get_property (GObject    *object,
                       guint       property_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
        BtePty *pty = BTE_PTY (object);
        BtePtyPrivate *priv = pty->priv;

        switch (property_id) {
        case PROP_FLAGS:
                g_value_set_flags(value, priv->flags);
                break;

        case PROP_FD:
                g_value_set_int(value, bte_pty_get_fd(pty));
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        }
}

static void
bte_pty_set_property (GObject      *object,
                       guint         property_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
        BtePty *pty = BTE_PTY (object);
        BtePtyPrivate *priv = pty->priv;

        switch (property_id) {
        case PROP_FLAGS:
                priv->flags = (BtePtyFlags) g_value_get_flags(value);
                break;

        case PROP_FD:
                priv->foreign_fd = g_value_get_int(value);
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
}

static void
bte_pty_class_init (BtePtyClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->set_property = bte_pty_set_property;
        object_class->get_property = bte_pty_get_property;
        object_class->finalize     = bte_pty_finalize;

        /**
         * BtePty:flags:
         *
         * Flags.
         */
        g_object_class_install_property
                (object_class,
                 PROP_FLAGS,
                 g_param_spec_flags ("flags", NULL, NULL,
                                     BTE_TYPE_PTY_FLAGS,
                                     BTE_PTY_DEFAULT,
                                     (GParamFlags) (G_PARAM_READWRITE |
                                                    G_PARAM_CONSTRUCT_ONLY |
                                                    G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY)));

        /**
         * BtePty:fd:
         *
         * The file descriptor of the PTY master.
         */
        g_object_class_install_property
                (object_class,
                 PROP_FD,
                 g_param_spec_int ("fd", NULL, NULL,
                                   -1, G_MAXINT, -1,
                                   (GParamFlags) (G_PARAM_READWRITE |
                                                  G_PARAM_CONSTRUCT_ONLY |
                                                  G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY)));
}

/* public API */

/**
 * bte_pty_error_quark:
 *
 * Error domain for BTE PTY errors. Errors in this domain will be from the #BtePtyError
 * enumeration. See #GError for more information on error domains.
 *
 * Returns: the error domain for BTE PTY errors
 */
GQuark
bte_pty_error_quark(void) noexcept
{
  static GQuark quark = 0;

  if (G_UNLIKELY (quark == 0))
    quark = g_quark_from_static_string("bte-pty-error");

  return quark;
}

/**
 * bte_pty_new_sync: (constructor)
 * @flags: flags from #BtePtyFlags
 * @cancellable: (allow-none): a #GCancellable, or %NULL
 * @error: (allow-none): return location for a #GError, or %NULL
 *
 * Allocates a new pseudo-terminal.
 *
 * You can later use fork() or the g_spawn_async() family of functions
 * to start a process on the PTY.
 *
 * If using fork(), you MUST call bte_pty_child_setup() in the child.
 *
 * If using g_spawn_async() and friends, you MUST either use
 * bte_pty_child_setup() directly as the child setup function, or call
 * bte_pty_child_setup() from your own child setup function supplied.
 *
 * When using bte_terminal_spawn_sync() with a custom child setup
 * function, bte_pty_child_setup() will be called before the supplied
 * function; you must not call it again.
 *
 * Also, you MUST pass the %G_SPAWN_DO_NOT_REAP_CHILD flag.
 *
 * Note also that %G_SPAWN_STDOUT_TO_DEV_NULL, %G_SPAWN_STDERR_TO_DEV_NULL,
 * and %G_SPAWN_CHILD_INHERITS_STDIN are not supported, since stdin, stdout
 * and stderr of the child process will always be connected to the PTY.
 *
 * Note that you should set the PTY's size using bte_pty_set_size() before
 * spawning the child process, so that the child process has the correct
 * size from the start instead of starting with a default size and then
 * shortly afterwards receiving a SIGWINCH signal. You should prefer
 * using bte_terminal_pty_new_sync() which does this automatically.
 *
 * Returns: (transfer full): a new #BtePty, or %NULL on error with @error filled in
 */
BtePty *
bte_pty_new_sync (BtePtyFlags flags,
                  GCancellable *cancellable,
                  GError **error) noexcept
{
        return (BtePty *) g_initable_new (BTE_TYPE_PTY,
                                          cancellable,
                                          error,
                                          "flags", flags,
                                          NULL);
}

/**
 * bte_pty_new_foreign_sync: (constructor)
 * @fd: a file descriptor to the PTY
 * @cancellable: (allow-none): a #GCancellable, or %NULL
 * @error: (allow-none): return location for a #GError, or %NULL
 *
 * Creates a new #BtePty for the PTY master @fd.
 *
 * No entry will be made in the lastlog, utmp or wtmp system files.
 *
 * Note that the newly created #BtePty will take ownership of @fd
 * and close it on finalize.
 *
 * Returns: (transfer full): a new #BtePty for @fd, or %NULL on error with @error filled in
 */
BtePty *
bte_pty_new_foreign_sync (int fd,
                          GCancellable *cancellable,
                          GError **error) noexcept
{
        g_return_val_if_fail(fd != -1, nullptr);

        return (BtePty *) g_initable_new (BTE_TYPE_PTY,
                                          cancellable,
                                          error,
                                          "fd", fd,
                                          NULL);
}

/**
 * bte_pty_get_fd:
 * @pty: a #BtePty
 *
 * Returns: the file descriptor of the PTY master in @pty. The
 *   file descriptor belongs to @pty and must not be closed or have
 *   its flags changed
 */
int
bte_pty_get_fd (BtePty *pty) noexcept
try
{
        g_return_val_if_fail(BTE_IS_PTY(pty), FALSE);
        return IMPL(pty)->fd();
}
catch (...)
{
        bte::log_exception();
        return -1;
}

static constexpr inline auto
all_spawn_flags() noexcept
{
        return (G_SPAWN_LEAVE_DESCRIPTORS_OPEN |
                G_SPAWN_DO_NOT_REAP_CHILD |
                G_SPAWN_SEARCH_PATH |
                G_SPAWN_STDOUT_TO_DEV_NULL |
                G_SPAWN_STDERR_TO_DEV_NULL |
                G_SPAWN_CHILD_INHERITS_STDIN |
                G_SPAWN_FILE_AND_ARGV_ZERO |
                G_SPAWN_SEARCH_PATH_FROM_ENVP |
                G_SPAWN_CLOEXEC_PIPES |
                BTE_SPAWN_NO_PARENT_ENVV |
                BTE_SPAWN_NO_SYSTEMD_SCOPE |
                BTE_SPAWN_REQUIRE_SYSTEMD_SCOPE);
}

static constexpr inline auto
forbidden_spawn_flags() noexcept
{
        return GSpawnFlags(G_SPAWN_LEAVE_DESCRIPTORS_OPEN |
                           G_SPAWN_STDOUT_TO_DEV_NULL |
                           G_SPAWN_STDERR_TO_DEV_NULL |
                           G_SPAWN_CHILD_INHERITS_STDIN);
}

static constexpr inline auto
ignored_spawn_flags() noexcept
{
        return GSpawnFlags(G_SPAWN_CLOEXEC_PIPES |
                           G_SPAWN_DO_NOT_REAP_CHILD);
}

static bte::base::SpawnContext
spawn_context_from_args(BtePty* pty,
                        char const* working_directory,
                        char const* const* argv,
                        char const* const* envv,
                        int const* fds,
                        int n_fds,
                        int const* fd_map_to,
                        int n_fd_map_to,
                        GSpawnFlags spawn_flags,
                        GSpawnChildSetupFunc child_setup,
                        void* child_setup_data,
                        GDestroyNotify child_setup_data_destroy)
{
        auto context = bte::base::SpawnContext{};
        context.set_pty(bte::glib::make_ref(pty));
        context.set_cwd(working_directory);
        context.set_fallback_cwd(g_get_home_dir());
        context.set_child_setup(child_setup, child_setup_data, child_setup_data_destroy);

        if ((spawn_flags & G_SPAWN_SEARCH_PATH_FROM_ENVP) ||
            (spawn_flags & G_SPAWN_SEARCH_PATH))
                context.set_search_path();

        if (spawn_flags & G_SPAWN_FILE_AND_ARGV_ZERO)
                context.set_argv(argv[0], argv + 1);
        else
                context.set_argv(argv[0], argv);

        context.set_environ(envv);
        if (spawn_flags & BTE_SPAWN_NO_PARENT_ENVV)
                context.set_no_inherit_environ();

        if (spawn_flags & BTE_SPAWN_NO_SYSTEMD_SCOPE)
                context.set_no_systemd_scope();
        if (spawn_flags & BTE_SPAWN_REQUIRE_SYSTEMD_SCOPE)
                context.set_require_systemd_scope();

        context.add_fds(fds, n_fds);
        context.add_map_fds(fds, n_fds, fd_map_to, n_fd_map_to);

        return context;
}

bool
_bte_pty_spawn_sync(BtePty* pty,
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
                    GError** error) noexcept
try
{
        /* These are ignored or need not be passed since the behaviour is the default */
        g_warn_if_fail((spawn_flags & ignored_spawn_flags()) == 0);

        /* This may be upgraded to a g_return_if_fail in the future */
        g_warn_if_fail((spawn_flags & forbidden_spawn_flags()) == 0);
        spawn_flags = GSpawnFlags(spawn_flags & ~forbidden_spawn_flags());

        auto op = bte::base::SpawnOperation{spawn_context_from_args(pty,
                                                                    working_directory,
                                                                    argv,
                                                                    envv,
                                                                    nullptr, 0,
                                                                    nullptr, 0,
                                                                    spawn_flags,
                                                                    child_setup,
                                                                    child_setup_data,
                                                                    child_setup_data_destroy),
                                            timeout,
                                            cancellable};

        auto err = bte::glib::Error{};
        auto rv = op.run_sync(child_pid, err);
        if (!rv)
                err.propagate(error);

        return rv;
}
catch (...)
{
        return bte::glib::set_error_from_exception(error);
}

/*
 * _bte_pty_check_envv:
 * @strv:
 *
 * Validates that each element is of the form 'KEY=VALUE'.
 */
bool
_bte_pty_check_envv(char const* const* strv) noexcept
{
  if (!strv)
    return true;

  for (int i = 0; strv[i]; ++i) {
          const char *str = strv[i];
          const char *equal = strchr(str, '=');
          if (equal == nullptr || equal == str)
                  return false;
  }

  return true;
}

/**
 * bte_pty_spawn_with_fds_async:
 * @pty: a #BtePty
 * @working_directory: (allow-none): the name of a directory the command should start
 *   in, or %NULL to use the current working directory
 * @argv: (array zero-terminated=1) (element-type filename): child's argument vector
 * @envv: (allow-none) (array zero-terminated=1) (element-type filename): a list of environment
 *   variables to be added to the environment before starting the process, or %NULL
 * @fds: (nullable) (array length=n_fds) (transfer none) (scope call): an array of file descriptors, or %NULL
 * @n_fds: the number of file descriptors in @fds, or 0 if @fds is %NULL
 * @map_fds: (nullable) (array length=n_map_fds) (transfer none) (scope call): an array of integers, or %NULL
 * @n_map_fds: the number of elements in @map_fds, or 0 if @map_fds is %NULL
 * @spawn_flags: flags from #GSpawnFlags
 * @child_setup: (allow-none) (scope async): an extra child setup function to run in the child just before exec(), or %NULL
 * @child_setup_data: (nullable) (closure child_setup): user data for @child_setup, or %NULL
 * @child_setup_data_destroy: (nullable) (destroy child_setup_data): a #GDestroyNotify for @child_setup_data, or %NULL
 * @timeout: a timeout value in ms, -1 for the default timeout, or G_MAXINT to wait indefinitely
 * @cancellable: (allow-none): a #GCancellable, or %NULL
 * @callback: (nullable) (scope async): a #GAsyncReadyCallback, or %NULL
 * @user_data: (closure callback): user data for @callback
 *
 * Starts the specified command under the pseudo-terminal @pty.
 * The @argv and @envv lists should be %NULL-terminated.
 * The "TERM" environment variable is automatically set to a default value,
 * but can be overridden from @envv.
 * @pty_flags controls logging the session to the specified system log files.
 *
 * Note also that %G_SPAWN_STDOUT_TO_DEV_NULL, %G_SPAWN_STDERR_TO_DEV_NULL,
 * and %G_SPAWN_CHILD_INHERITS_STDIN are not supported in @spawn_flags, since
 * stdin, stdout and stderr of the child process will always be connected to
 * the PTY. Also %G_SPAWN_LEAVE_DESCRIPTORS_OPEN is not supported; and
 * %G_SPAWN_DO_NOT_REAP_CHILD will always be added to @spawn_flags.
 *
 * If @fds is not %NULL, the child process will map the file descriptors from
 * @fds according to @map_fds; @n_map_fds must be less or equal to @n_fds.
 * This function will take ownership of the file descriptors in @fds;
 * you must not use or close them after this call. All file descriptors in @fds
 * must have the FD_CLOEXEC flag set on them; it will be unset in the child process
 * before calling exec.
 *
 * Note that all  open file descriptors apart from those mapped as above
 * will be closed in the child. (If you want to keep some other file descriptor
 * open for use in the child process, you need to use a child setup function
 * that unsets the FD_CLOEXEC flag on that file descriptor manually.)
 *
 * Beginning with 0.60, and on linux only, and unless %BTE_SPAWN_NO_SYSTEMD_SCOPE is
 * passed in @spawn_flags, the newly created child process will be moved to its own
 * systemd user scope; and if %BTE_SPAWN_REQUIRE_SYSTEMD_SCOPE is passed, and creation
 * of the systemd user scope fails, the whole spawn will fail.
 * You can override the options used for the systemd user scope by
 * providing a systemd override file for 'bte-spawn-.scope' unit. See man:systemd.unit(5)
 * for further information.
 *
 * See bte_pty_new(), and bte_terminal_watch_child() for more information.
 *
 * Since: 0.62
 */
void
bte_pty_spawn_with_fds_async(BtePty *pty,
                             char const* working_directory,
                             char const* const* argv,
                             char const* const* envv,
                             int const* fds,
                             int n_fds,
                             int const* fd_map_to,
                             int n_fd_map_to,
                             GSpawnFlags spawn_flags,
                             GSpawnChildSetupFunc child_setup,
                             gpointer child_setup_data,
                             GDestroyNotify child_setup_data_destroy,
                             int timeout,
                             GCancellable *cancellable,
                             GAsyncReadyCallback callback,
                             gpointer user_data) noexcept
try
{
        g_return_if_fail(argv != nullptr);
        g_return_if_fail(argv[0] != nullptr);
        g_return_if_fail(envv == nullptr || _bte_pty_check_envv(envv));
        g_return_if_fail(n_fds == 0 || fds != nullptr);
        for (auto i = int{0}; i < n_fds; ++i)
                g_return_if_fail(bte::libc::fd_get_cloexec(fds[i]));
        g_return_if_fail(n_fd_map_to == 0 || fd_map_to != nullptr);
        g_return_if_fail(n_fds >= n_fd_map_to);
        g_return_if_fail((spawn_flags & ~all_spawn_flags()) == 0);
        g_return_if_fail(!child_setup_data || child_setup);
        g_return_if_fail(!child_setup_data_destroy || child_setup_data);
        g_return_if_fail(timeout >= -1);
        g_return_if_fail(cancellable == nullptr || G_IS_CANCELLABLE (cancellable));

        /* These are ignored or need not be passed since the behaviour is the default */
        g_warn_if_fail((spawn_flags & ignored_spawn_flags()) == 0);

        /* This may be upgraded to a g_return_if_fail in the future */
        g_warn_if_fail((spawn_flags & forbidden_spawn_flags()) == 0);
        spawn_flags = GSpawnFlags(spawn_flags & ~forbidden_spawn_flags());

        auto op = new bte::base::SpawnOperation{spawn_context_from_args(pty,
                                                                        working_directory,
                                                                        argv,
                                                                        envv,
                                                                        fds, n_fds,
                                                                        fd_map_to, n_fd_map_to,
                                                                        spawn_flags,
                                                                        child_setup,
                                                                        child_setup_data,
                                                                        child_setup_data_destroy),
                                                timeout,
                                                cancellable};

        /* takes ownership of @op */
        op->run_async((void*)bte_pty_spawn_async, /* tag */
                      callback,
                      user_data);
}
catch (...)
{
        // FIXME: make the function above exception safe. It needs to guarantee
        // that the callback will be invoked regardless of when the throw occurred.

        bte::log_exception();
}

/**
 * bte_pty_spawn_async:
 * @pty: a #BtePty
 * @working_directory: (allow-none): the name of a directory the command should start
 *   in, or %NULL to use the current working directory
 * @argv: (array zero-terminated=1) (element-type filename): child's argument vector
 * @envv: (allow-none) (array zero-terminated=1) (element-type filename): a list of environment
 *   variables to be added to the environment before starting the process, or %NULL
 * @spawn_flags: flags from #GSpawnFlags
 * @child_setup: (allow-none) (scope async): an extra child setup function to run in the child just before exec(), or %NULL
 * @child_setup_data: (nullable) (closure child_setup): user data for @child_setup, or %NULL
 * @child_setup_data_destroy: (nullable) (destroy child_setup_data): a #GDestroyNotify for @child_setup_data, or %NULL
 * @timeout: a timeout value in ms, -1 for the default timeout, or G_MAXINT to wait indefinitely
 * @cancellable: (allow-none): a #GCancellable, or %NULL
 * @callback: (nullable) (scope async): a #GAsyncReadyCallback, or %NULL
 * @user_data: (closure callback): user data for @callback
 *
 * Like bte_pty_spawn_with_fds_async(), except that this function does not
 * allow passing file descriptors to the child process. See bte_pty_spawn_with_fds_async()
 * for more information.
 *
 * Since: 0.48
 */
void
bte_pty_spawn_async(BtePty *pty,
                    const char *working_directory,
                    char **argv,
                    char **envv,
                    GSpawnFlags spawn_flags,
                    GSpawnChildSetupFunc child_setup,
                    gpointer child_setup_data,
                    GDestroyNotify child_setup_data_destroy,
                    int timeout,
                    GCancellable *cancellable,
                    GAsyncReadyCallback callback,
                    gpointer user_data) noexcept
{
        bte_pty_spawn_with_fds_async(pty, working_directory, argv, envv,
                                     nullptr, 0, nullptr, 0,
                                     spawn_flags,
                                     child_setup, child_setup_data, child_setup_data_destroy,
                                     timeout, cancellable,
                                     callback, user_data);
}

/**
 * bte_pty_spawn_finish:
 * @pty: a #BtePty
 * @result: a #GAsyncResult
 * @child_pid: (out) (allow-none) (transfer full): a location to store the child PID, or %NULL
 * @error: (allow-none): return location for a #GError, or %NULL
 *
 * Returns: %TRUE on success, or %FALSE on error with @error filled in
 *
 * Since: 0.48
 */
gboolean
bte_pty_spawn_finish(BtePty* pty,
                     GAsyncResult* result,
                     GPid* child_pid /* out */,
                     GError** error) noexcept
{
        g_return_val_if_fail (BTE_IS_PTY(pty), false);
        g_return_val_if_fail (G_IS_TASK(result), false);
        g_return_val_if_fail (g_task_get_source_tag(G_TASK (result)) == bte_pty_spawn_async, false);
        g_return_val_if_fail(error == nullptr || *error == nullptr, false);

        auto pid = g_task_propagate_int(G_TASK(result), error);
        if (child_pid)
                *child_pid = pid;

        return pid != -1;
}
