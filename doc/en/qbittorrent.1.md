% QBITTORRENT(1) Bittorrent client written in C++ / Qt
%
% August 2026

# NAME
qBittorrent - a Bittorrent client written in C++ / Qt


# SYNOPSIS
**qbittorrent** `[options] [(<filename> | <url>)...]`

**qbittorrent** `--help`

**qbittorrent** `--version`


# DESCRIPTION
**qBittorrent** is an advanced Bittorrent client written in C++ / Qt,
using the **libtorrent-rasterbar** library by Arvid Norberg. qBittorrent is similar to uTorrent.
qBittorrent is fast, stable, light, it supports unicode and it provides a good integrated
search engine. It also comes with UPnP port forwarding / NAT-PMP, encryption (Vuze compatible),
FAST extension (mainline) and PeX support (utorrent compatible).


# OPTIONS
**`-h | --help`** Display this help message and exit.

**`-v | --version`** Display program version and exit.

**`--confirm-legal-notice`** Confirm the legal notice.

**`--webui-port=<port>`** Change the WebUI port.

**`--torrenting-port=<port>`** Change the torrenting port.

**`--no-splash`** Disable splash screen.

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
'TRUE'. For example, to disable the splash screen:

    QBT_NO_SPLASH=1 qbittorrent

Command line parameters take precedence over environment variables.


# BUGS
If you find a bug, please report it at https://bugs.qbittorrent.org


# AUTHORS
Christophe Dumez <chris@qbittorrent.org>.
