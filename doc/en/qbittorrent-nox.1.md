% QBITTORRENT-NOX(1) Command line Bittorrent client written in C++ / Qt
%
% January 16th 2010

# NAME
qBittorrent-nox - a command line Bittorrent client written in C++ / Qt


# SYNOPSIS
**qbittorrent-nox** `[--d|--daemon] [--webui-port=x] [TORRENT_FILE | URL]...`

**qbittorrent-nox** `--help`

**qbittorrent-nox** `--version`


# DESCRIPTION
**qBittorrent-nox** is an advanced command-line Bittorrent client written in C++ / Qt
using the **libtorrent-rasterbar** library by Arvid Norberg.
qBittorrent-nox aims to be a good alternative to other command line bittorrent
clients and provides features similar to popular graphical clients.

qBittorrent-nox is fast, stable, light and it supports unicode. It also comes with
UPnP port forwarding / NAT-PMP, encryption (Vuze compatible), FAST extension (mainline)
and PeX support (utorrent compatible).

qBittorrent-nox is meant to be controlled via its feature-rich Web UI which is
accessible as a default on http://localhost:8080. The Web UI access is secured and
the default account user name is "admin" with "adminadmin" as a password.


# OPTIONS
**`--help`** Prints the command line options.

**`--version`** Prints qbittorrent program version number.

**`--webui-port=x`** Changes Web UI port to x (default: 8080).


# BUGS
If you find a bug, please report it at https://bugs.qbittorrent.org


# AUTHORS
Christophe Dumez <chris@qbittorrent.org>.
