/* xscreensaver, Copyright © 2026 Performant Labs
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *
 * Poll org.gnome.Mutter.IdleMonitor.GetIdletime on the session bus.
 * https://mutter.gnome.org/meta/class.IdleMonitor.html
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>

#include <dbus/dbus.h>

#include "blurb.h"
#include "gnome-idle.h"

/* GNOME Shell only lets Settings and the portal grab accelerators.
   gsd-media-keys is one of those, and it grabs every custom keybinding.
   Installing one for Escape makes Mutter consume the key press. */
#define ESCAPE_PATH \
  "/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/xscreensaver-escape/"
#define MEDIA_SCHEMA "org.gnome.settings-daemon.plugins.media-keys"
#define BINDING_SCHEMA \
  "org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:" ESCAPE_PATH

struct gnome_idle_s {
  DBusConnection *conn;
};

static long long
gnome_idle_query (DBusConnection *conn)
{
  DBusMessage *msg, *reply;
  DBusError err;
  dbus_uint64_t ms = 0;

  msg = dbus_message_new_method_call ("org.gnome.Mutter.IdleMonitor",
                                      "/org/gnome/Mutter/IdleMonitor/Core",
                                      "org.gnome.Mutter.IdleMonitor",
                                      "GetIdletime");
  if (!msg)
    return -1;

  dbus_error_init (&err);
  reply = dbus_connection_send_with_reply_and_block (conn, msg, 500, &err);
  dbus_message_unref (msg);
  if (!reply)
    {
      dbus_error_free (&err);
      return -1;
    }

  dbus_error_init (&err);
  if (!dbus_message_get_args (reply, &err,
                              DBUS_TYPE_UINT64, &ms,
                              DBUS_TYPE_INVALID))
    {
      dbus_message_unref (reply);
      dbus_error_free (&err);
      return -1;
    }

  dbus_message_unref (reply);
  return (long long) ms;
}


gnome_idle *
gnome_idle_open (void)
{
  DBusError err;
  DBusConnection *conn;
  gnome_idle *g;

  dbus_error_init (&err);
  conn = dbus_bus_get_private (DBUS_BUS_SESSION, &err);
  if (!conn)
    {
      dbus_error_free (&err);
      return NULL;
    }

  /* A vanished session bus must not take the daemon down with it. */
  dbus_connection_set_exit_on_disconnect (conn, FALSE);

  if (gnome_idle_query (conn) < 0)
    {
      dbus_connection_close (conn);
      dbus_connection_unref (conn);
      return NULL;
    }

  g = (gnome_idle *) calloc (1, sizeof (*g));
  if (!g)
    {
      dbus_connection_close (conn);
      dbus_connection_unref (conn);
      return NULL;
    }
  g->conn = conn;
  return g;
}


long long
gnome_idle_ms (gnome_idle *g)
{
  if (!g || !g->conn)
    return -1;
  return gnome_idle_query (g->conn);
}


void
gnome_idle_free (gnome_idle *g)
{
  if (!g)
    return;
  if (g->conn)
    {
      dbus_connection_close (g->conn);
      dbus_connection_unref (g->conn);
    }
  free (g);
}


static int
gsettings_exec (char *const argv[])
{
  pid_t pid;
  int status = 1;

  pid = fork ();
  if (pid < 0)
    return 0;
  if (pid == 0)
    {
      execv (argv[0], argv);
      _exit (127);
    }
  while (waitpid (pid, &status, 0) < 0)
    {
      if (errno != EINTR)
        return 0;
    }
  return WIFEXITED (status) && WEXITSTATUS (status) == 0;
}


static int
gsettings_get (const char *schema, const char *key, char *buf, size_t buflen)
{
  char cmd[1024];
  FILE *fp;
  size_t n;

  snprintf (cmd, sizeof cmd, "/usr/bin/gsettings get '%s' '%s' 2>/dev/null",
            schema, key);
  fp = popen (cmd, "r");
  if (!fp)
    return 0;
  n = fread (buf, 1, buflen - 1, fp);
  buf[n] = 0;
  pclose (fp);
  while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
    buf[--n] = 0;
  return n > 0;
}


/* gsettings prints a string array as ['/path/', '/other/']. */
static int
quoted_strings (const char *text, char dest[][768], int max)
{
  int count = 0;
  const char *p = text;

  while (count < max && (p = strchr (p, '\'')))
    {
      const char *end;
      size_t len;
      p++;
      end = strchr (p, '\'');
      if (!end)
        break;
      len = (size_t) (end - p);
      if (len < sizeof dest[0])
        {
          memcpy (dest[count], p, len);
          dest[count][len] = 0;
          count++;
        }
      p = end + 1;
    }
  return count;
}


static int
write_binding_list (char items[][768], int count, int include_ours)
{
  char value[8192];
  size_t used = 0;
  int i, saw = 0;
  char *argv[6];

  value[0] = '[';
  value[1] = 0;
  used = 1;
  for (i = 0; i < count; i++)
    {
      if (!strcmp (items[i], ESCAPE_PATH))
        {
          saw = 1;
          if (!include_ours)
            continue;
        }
      if (used > 1 && used + 2 < sizeof value)
        {
          memcpy (value + used, ", ", 2);
          used += 2;
          value[used] = 0;
        }
      used += snprintf (value + used, sizeof value - used, "'%s'", items[i]);
      if (used >= sizeof value)
        return 0;
    }
  if (include_ours && !saw)
    {
      if (used > 1 && used + 2 < sizeof value)
        {
          memcpy (value + used, ", ", 2);
          used += 2;
        }
      used += snprintf (value + used, sizeof value - used, "'%s'", ESCAPE_PATH);
      if (used >= sizeof value)
        return 0;
    }
  if (used + 2 >= sizeof value)
    return 0;
  value[used++] = ']';
  value[used] = 0;

  argv[0] = "/usr/bin/gsettings";
  argv[1] = "set";
  argv[2] = MEDIA_SCHEMA;
  argv[3] = "custom-keybindings";
  argv[4] = value;
  argv[5] = NULL;
  return gsettings_exec (argv);
}


static int
binding_set (const char *key, const char *value)
{
  char *argv[6];

  argv[0] = "/usr/bin/gsettings";
  argv[1] = "set";
  argv[2] = BINDING_SCHEMA;
  argv[3] = (char *) key;
  argv[4] = (char *) value;
  argv[5] = NULL;
  return gsettings_exec (argv);
}


void
gnome_escape_absorb_install (void)
{
  char current[8192];
  char items[32][768];
  int count;

  if (!binding_set ("name", "XScreenSaver") ||
      !binding_set ("command", "/bin/true") ||
      !binding_set ("binding", "Escape"))
    {
      fprintf (stderr, "%s: could not install the Escape binding\n", blurb());
      return;
    }

  if (!gsettings_get (MEDIA_SCHEMA, "custom-keybindings",
                      current, sizeof current))
    {
      fprintf (stderr, "%s: could not read custom keybindings\n", blurb());
      return;
    }
  count = quoted_strings (current, items, 32);
  if (!write_binding_list (items, count, 1))
    {
      fprintf (stderr, "%s: could not enable the Escape binding\n", blurb());
      return;
    }
  if (verbose_p)
    fprintf (stderr, "%s: absorbing Escape while the screen is blanked\n",
             blurb());
}


void
gnome_escape_absorb_remove (void)
{
  char current[8192];
  char items[32][768];
  int count, i, saw = 0;
  char *argv[5];

  if (!gsettings_get (MEDIA_SCHEMA, "custom-keybindings",
                      current, sizeof current))
    return;
  count = quoted_strings (current, items, 32);
  for (i = 0; i < count; i++)
    if (!strcmp (items[i], ESCAPE_PATH))
      saw = 1;
  if (!saw)
    return;
  if (!write_binding_list (items, count, 0))
    {
      fprintf (stderr, "%s: could not remove the Escape binding\n", blurb());
      return;
    }
  argv[0] = "/usr/bin/gsettings";
  argv[1] = "reset";
  argv[2] = BINDING_SCHEMA;
  argv[4] = NULL;
  argv[3] = "binding";
  gsettings_exec (argv);
  argv[3] = "command";
  gsettings_exec (argv);
  argv[3] = "name";
  gsettings_exec (argv);
  if (verbose_p)
    fprintf (stderr, "%s: Escape reaches applications again\n", blurb());
}
