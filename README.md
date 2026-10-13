# XScreenSaver

This is an independent fork of [XScreenSaver](https://www.jwz.org/xscreensaver/), focused on improving Wayland and GNOME compatibility.

Upstream is maintained by Jamie Zawinski and contributors. Releases are published as tarballs from <https://www.jwz.org/xscreensaver/>. This repository is not affiliated with that project. This tree was imported from the read-only mirror <https://github.com/Zygo/xscreensaver>, whose tip is the XScreenSaver 6.17 release (`xscreensaver-6.17.tar.gz`). Original copyright notices and the original permissive license text in the source files are unchanged.

## Goal

Make recent XScreenSaver versions not crash when Mutter does not implement `ext_idle_notifier_v1` or `org_kde_kwin_idle`.
