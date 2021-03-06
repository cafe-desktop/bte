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

#include "config.h"

#include "debug.h"
#include "reaper.hh"

struct _BteReaper {
        GObject parent_instance;
};

struct _BteReaperClass {
        GObjectClass parent_class;
};
typedef struct _BteReaperClass BteReaperClass;

#define BTE_TYPE_REAPER            (bte_reaper_get_type())
#define BTE_REAPER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), BTE_TYPE_REAPER, BteReaper))
#define BTE_REAPER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  BTE_TYPE_REAPER, BteReaperClass))
#define BTE_IS_REAPER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BTE_TYPE_REAPER))
#define BTE_IS_REAPER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  BTE_TYPE_REAPER))
#define BTE_REAPER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  BTE_TYPE_REAPER, BteReaperClass))

static GType bte_reaper_get_type(void);

G_DEFINE_TYPE(BteReaper, bte_reaper, G_TYPE_OBJECT)

static BteReaper *singleton_reaper = nullptr;

static void
bte_reaper_child_watch_cb(GPid pid,
                          int status,
                          gpointer data)
{
        _bte_debug_print(BTE_DEBUG_SIGNALS,
                         "Reaper emitting child-exited signal.\n");
        g_signal_emit_by_name(data, "child-exited", pid, status);
        g_spawn_close_pid (pid);
}

/*
 * bte_reaper_add_child:
 * @pid: the ID of a child process which will be monitored
 *
 * Ensures that child-exited signals will be emitted when @pid exits.
 */
void
bte_reaper_add_child(GPid pid)
{
        g_child_watch_add_full(G_PRIORITY_LOW,
                               pid,
                               bte_reaper_child_watch_cb,
                               bte_reaper_ref(),
                               (GDestroyNotify)g_object_unref);
}

static void
bte_reaper_init(BteReaper *reaper)
{
}

static GObject*
bte_reaper_constructor (GType                  type,
                        guint                  n_construct_properties,
                        GObjectConstructParam *construct_properties)
{
  if (singleton_reaper) {
          return (GObject*)g_object_ref (singleton_reaper);
  } else {
          GObject *obj;
          obj = G_OBJECT_CLASS (bte_reaper_parent_class)->constructor (type, n_construct_properties, construct_properties);
          singleton_reaper = BTE_REAPER (obj);
          return obj;
  }
}


static void
bte_reaper_finalize(GObject *reaper)
{
        G_OBJECT_CLASS(bte_reaper_parent_class)->finalize(reaper);
        singleton_reaper = nullptr;
}

static void
bte_reaper_class_init(BteReaperClass *klass)
{
        GObjectClass *gobject_class;

        /*
         * BteReaper::child-exited:
         * @btereaper: the object which received the signal
         * @arg1: the process ID of the exited child
         * @arg2: the status of the exited child, as returned by waitpid()
         *
         * Emitted when the #BteReaper object detects that a child of the
         * current process has exited.
         */
        g_signal_new(g_intern_static_string("child-exited"),
                     G_OBJECT_CLASS_TYPE(klass),
                     G_SIGNAL_RUN_LAST,
                     0,
                     nullptr,
                     nullptr,
                     g_cclosure_marshal_generic,
                     G_TYPE_NONE,
                     2,
                     G_TYPE_INT, G_TYPE_INT);
        
        gobject_class = G_OBJECT_CLASS(klass);
        gobject_class->constructor = bte_reaper_constructor;
        gobject_class->finalize = bte_reaper_finalize;
}

/*
 * bte_reaper_ref:
 *
 * Finds the address of the global #BteReaper object, creating the object if
 * necessary.
 *
 * Returns: a reference to the global #BteReaper object, which
 *  must be unreffed when done with it
 */
BteReaper *
bte_reaper_ref(void)
{
        return (BteReaper*)g_object_new(BTE_TYPE_REAPER, nullptr);
}

#ifdef MAIN

#include <unistd.h>

GMainContext *context;
GMainLoop *loop;
pid_t child;

static void
child_exited(GObject *object, int pid, int status, gpointer data)
{
        g_print("[parent] Child with pid %d exited with code %d, "
                "was waiting for %d.\n", pid, status, GPOINTER_TO_INT(data));
        if (child == pid) {
                g_print("[parent] Quitting.\n");
                g_main_loop_quit(loop);
        }
}

int
main(int argc, char **argv)
{
        BteReaper *reaper;
        pid_t p, q;

        _bte_debug_init();

        context = g_main_context_default();
        loop = g_main_loop_new(context, FALSE);
        reaper = bte_reaper_ref();

        g_print("[parent] Forking1.\n");
        p = fork();
        switch (p) {
                case -1:
                        g_print("[parent] Fork1 failed.\n");
                        g_assert_not_reached();
                        break;
                case 0:
                        g_print("[child1]  Going to sleep.\n");
                        sleep(10);
                        g_print("[child1]  Quitting.\n");
                        _exit(30);
                        break;
                default:
                        g_print("[parent] Starting to wait for %d.\n", p);
                        bte_reaper_add_child(p);
                        child = p;
                        g_signal_connect(reaper,
                                         "child-exited",
                                         G_CALLBACK(child_exited),
                                         GINT_TO_POINTER(child));
                        break;
        }

        g_print("[parent] Forking2.\n");
        q = fork();
        switch (q) {
                case -1:
                        g_print("[parent] Fork2 failed.\n");
                        g_assert_not_reached();
                        break;
                case 0:
                        g_print("[child2]  Going to sleep.\n");
                        sleep(5);
                        g_print("[child2]  Quitting.\n");
                        _exit(5);
                        break;
                default:
                        g_print("[parent] Not waiting for %d.\n", q);
                        break;
        }


        g_main_loop_run(loop);

        g_object_unref(reaper);

        return 0;
}
#endif
