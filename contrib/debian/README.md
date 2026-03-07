
Debian
====================
This directory contains files used to package kubud/kubu-qt
for Debian-based Linux systems. If you compile kubud/kubu-qt yourself, there are some useful files here.

## kubu: URI support ##


kubu-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install kubu-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your kubu-qt binary to `/usr/bin`
and the `../../share/pixmaps/kubu128.png` to `/usr/share/pixmaps`

kubu-qt.protocol (KDE)

