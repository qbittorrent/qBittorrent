% QBITTORRENT-NOX(1) Command line Bittorrent client written in C++ / Qt
%
% August 2026

# NAME
qBittorrent-nox - a command line Bittorrent client written in C++ / Qt


# SYNOPSIS
**qbittorrent-nox** `[options] [(<filename> | <url>)...]`

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
accessible by default at http://localhost:8080. The default WebUI administrator
user name is "admin". If no password has been set, a temporary random password is
generated and printed to the console on each startup; you should set your own
password in the program preferences.


# OPTIONS
**`-h | --help`** Display this help message and exit.

**`-v | --version`** Display program version and exit.

**`--confirm-legal-notice`** Confirm the legal notice.

**`--webui-port=<port>`** Change the WebUI port.

**`--torrenting-port=<port>`** Change the torrenting port.

**`-d | --daemon`** Run in daemon-mode (background).

**`--profile=<dir>`** Store configuration files in `<dir>`.

**`--configuration=<name>`** Store configuration files in directories qBittorrent_`<name>`.

**`--relative-fastresume`** Hack into libtorrent fastresume files and make file paths
relative to the profile directory.

**`(<filename> | <url>)...`** Download the torrents passed by the user.


## Options when adding new torrents
**`--save-path=<path>`** Torrent save path.

**`--add-stopped=<true|false>`** Add torrents as running or stopped.

**`--seed-mode`** Seed mode.

**`--category=<name>`** Assign torrents to category. If the category doesn't exist,
it will be created.

**`--sequential`** Download files in sequential order.

**`--first-and-last`** Download first and last pieces first.

**`--skip-dialog=<true|false>`** Specify whether the "Add New Torrent" dialog opens
when adding a torrent.


# ENVIRONMENT
Option values may be supplied via environment variables. For an option named
'parameter-name', the environment variable name is 'QBT_PARAMETER_NAME' (upper
case, with '-' replaced by '_'). To pass flag values, set the variable to '1' or
'TRUE'. For example, to change the WebUI port:

    QBT_WEBUI_PORT=8081 qbittorrent-nox

Command line parameters take precedence over environment variables.


# BUGS
If you find a bug, please report it at https://bugs.qbittorrent.org


# AUTHORS
Christophe Dumez <chris@qbittorrent.org>.
