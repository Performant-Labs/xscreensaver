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
 * GNOME Shell does not implement ext_idle_notifier_v1 or org_kde_kwin_idle.
 * Keystrokes typed into native Wayland clients never arrive as XInput events,
 * so the daemon asks Mutter how long the session has been idle instead.
 * This reports only a duration, never which keys were pressed.
 */

#ifndef __XSCREENSAVER_GNOME_IDLE_H__
#define __XSCREENSAVER_GNOME_IDLE_H__

typedef struct gnome_idle_s gnome_idle;

/* Connects to the session bus and checks that Mutter's IdleMonitor answers.
   Returns NULL if this is not a GNOME session or the bus call fails. */
extern gnome_idle *gnome_idle_open (void);

/* Milliseconds since the last keyboard or pointer event, or -1 on failure. */
extern long long gnome_idle_ms (gnome_idle *);

extern void gnome_idle_free (gnome_idle *);

/* Ask GNOME to consume Escape while the saver is blanked, so the press
   does not reach the focused application.  Safe to call more than once,
   and safe when this is not a GNOME session. */
extern void gnome_escape_absorb_install (void);
extern void gnome_escape_absorb_remove (void);

#endif /* __XSCREENSAVER_GNOME_IDLE_H__ */
