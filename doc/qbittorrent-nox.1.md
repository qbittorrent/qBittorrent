% QBITTORRENT-NOX(1) BitTorrent client written in C++ / Qt
%
% 2020-03-20

# NAME

**qBittorrent** - BitTorrent client written in C++ / Qt (command-line version)

# SYNOPSIS

| **qbittorrent** \[_OPTIONS_\] \[_URI_...\]
| **qbittorrent** **`-h` | `-v`**

# DESCRIPTION

**qBittorrent-nox** is an advanced command-line BitTorrent client written in C++ / Qt, using the **libtorrent-rasterbar** library by Arvid Norberg as the backend.
qBittorrent-nox is fast, stable and light, and is controllable via feature-rich Web UI similar to popular graphical clients.

qBittorrent-nox supports several important modern features and BEPs (BitTorrent Enhancement Proposals) such as UPnP port forwarding / NAT-PMP, DHT (Mainline) and magnet URI (also known as magnet link) support, IPv6, uTP, PeX, LSD, UDP tracker protocol support and more.
Refer to the **libtorrent-rasterbar** documentation for the full list of features and supported BEPs.

In addition, qBittorrent-nox supports Unicode and it provides a good integrated Python-based torrent search engine, an RSS downloader, a torrent creation GUI, a basic embedded tracker to facilitate sharing torrents with friends, and an HTTP API/Web UI combo for remote access/administration.

qBittorrent-nox is meant to be controlled via its bundled Web UI/Web API which is
accessible by default at http://localhost:8080.
The default access credentials are the user "admin" with "adminadmin" as a password.
qBittorrent-nox provides good support for setting up secure HTTPS access, either directly or via a reverse-proxy scheme.

Please note that at present, not all of the features of the GUI version may be available in the Web UI or exposed via HTTP API methods. They are added on a best-effort basis.
From version 4.1.0 and up, it is possible configure qBittorrent-nox to use alternate Web UIs developed by the community instead of the default one.

When qBittorrent-nox is started from a terminal, its **stdout** and **stderr** will be attached to that terminal.

If no arguments are specified, qBittorrent-nox simply starts.
qBittorrent-nox supports running as a background daemon, see the **`--daemon`** option. This is convenient for managing its lifecycle with a service manager like **systemd(1)**, for example.

If either **`-h`** or **`-v`** is specified, the relevant information is printed to the terminal and qBittorrent exits.
Passing any other option alongside **`-h`** or **`-v`** is not allowed.

_URI_ is a string pointing to a BitTorrent metainfo file (a "torrent").
It may be a path to a file on disk or an online resource accessible through a supported URI scheme such as **magnet**, **HTTP**, or **HTTPS**.

Standalone **info-hashes** are also accepted as shorthand for a bare-bones **magnet** _URI_ constructed from that **info-hash**.
For example, passing the **info-hash** _`e73108cbd628fee5cf203acdf668c5bf45d07810`_ (for the file `ubuntu-18.04.4-live-server-amd64.iso`) is the same as passing the **magnet** _URI_ _`magnet:?xt=urn:btih:e73108cbd628fee5cf203acdf668c5bf45d07810`_.

TODO: what happens in the nox version?
By default, any _URI_ passed as an argument will be downloaded if needed, and then shown in an "Add New Torrent" dialog to be added to the transfer list.

_OPTIONS_ control general program settings, such as the port the WebUI should listen on, as well as settings for the torrents to be added, if any, such as their save path.
In addition, the "Add New Torrent" dialog can be entirely skipped, to just automatically add the torrents wihthout confirmation with the specified settings.

Some _OPTIONS_ have both a long form (**`--option`**) and a short form (**`-o`**), others only have one of the two.

_OPTIONS_ can be passed in from several sources.
If the same option is specified in multiple sources, the last one takes precedence, in the following order:

1. The main configuration file (see the _FILES_ section)
2. _ENVIRONMENT_ variables
3. Command-line _OPTIONS_

**IMPORTANT NOTE:** Settings changed via command-line _OPTIONS_ or _ENVIRONMENT_ variables not only **override** their corresponding values in the configuration file, they **also overwrite** them.

_ENVIRONMENT_ variables and command-line _OPTIONS_ can currently only override a small subset of all the available settings that can be configured in the main configuration file.

qBittorrent-nox can be cleanly terminated by sending the **SIGINT** or **SIGTERM** signals to its process.

# OPTIONS

TODO: options:
- no torrent creator
- no skip splash option
- daemon: start in background, cant use if already started (but probably instances with different configuration names can still coexist)

TODO: document the fact that the main config file is populated upon first save. qbittorrent-conf manpage does not exist,
but it's better to just tell users to start once to generate it than to create and maintain such a man page


# ENVIRONMENT

_OPTIONS_ may be supplied via environment variables with equivalent names.

For an option named **parameter-name**, the corresponding environment variable name is **QBT\_PARAMETER\_NAME**.
In other words, the parameter name is converted to upper-case, any hyphens (**-**) are replaced by underscores (**\_**), and **QBT\_** is prepended to it.

To pass flag values, set the variable to **1** or **TRUE**.
For example, **`--add-paused=true`** becomes **QBT\_ADD\_PAUSED=1**

# FILES

## Standard configuration files directory hierarchy

Currently, qBittorrent complies somewhat with the Freedesktop XDG Base Directory specification.
The configuration files are stored in the default directories of the specification:

1. Configuration files are stored in _~/.config/qBittorrent_
2. Data files are stored in _~/.local/share/data/qBittorrent_
3. Cache is stored in _~/.cache/qBittorrent_

However, qBittorrent will not use custom XDG paths set via the **\$XDG\_CONFIG\_HOME** environment variables and the like; it will always use the default ones.

The **`--configuration`** and **`profile`** options will instruct qBittorrent to instead create/use configuration files in different places.
Refer to the documentation of each of these options to learn more.

## Files used by qBittorrent

This is a list of the files qBittorrent creates and uses for normal operation.
While the name and purpose of these files is unlikely to change, the file formats themselves are not stable, and thus intentionally not documented here.
Depending on what features are used, some files may not exist.
They are only created when needed.

_~/.config/qBittorrent/qBittorrent.conf_

:   Main configuration file.

_~/.config/qBittorrent/qBittorrent-data.conf_

:   This is where all-time statistics are saved.

_~/.config/qBittorrent/rss/feeds.json_

:    RSS feeds configuration.

_~/.config/qBittorrent/rss/download\_rules.json_

:    RSS auto-downloading rules configuration file.

_~/.local/share/data/qBittorrent/BT\_backup/_

:    This directory contains the _.fastresume_ files, which is the mechanism qBittorrent uses to keep track of the state of each added torrent across restarts.
In addition, a copy of each of the added torrents' metainfo files is kept here, with the torrents' hashes as the file names.

_~/.local/share/data/qBittorrent/GeoDB/_

:    Database files for IP <-> country flag resolution in the "Peers" tab.

_~/.local/share/data/qBittorrent/logs/_

:    Execution logs.

_~/.local/share/data/qBittorrent/nova3/_

:    Python files for the Search Engine functionality.

_~/.local/share/data/qBittorrent/nova3/engines/_

:    Custom search engine code and files.

_~/.local/share/data/qBittorrent/rss/articles/_

:    RSS feed storage.

_~/.cache/qBittorrent/_

:    Miscellaneous temporary cache files.

_~/.config/qBittorrent/rss/storage.lock_

:    RSS feed settings lock file.

_~/.local/share/data/qBittorrent/BT\_backup/session.lock_

:    BT_backup lock file, for _.fastresume_ files and backup metainfo files (_.torrent_ files).

_~/.local/share/data/qBittorrent/rss/articles/storage.lock_

:    RSS articles lock file.

_/tmp/qtsingleapp-qBitto-**\<app_id_string\>**-lockfile_

:    Per-configuration application instance lock file.
Each application instance with a different **`--configuration`** or **`--profile`** has a different **\<app_id_string\>**.
Two instances with the same configuration cannot run at the same time.

# NOTES

Visit the wiki at [http://wiki.qbittorrent.org](http://wiki.qbittorrent.org) for addtional information, including guides on how to compile qBittorrent from source.

# BUGS

Please report any bugs at [http://bugs.qbittorrent.org](http://bugs.qbittorrent.org).

TODO: examples

# SEE ALSO

**btcheck(1)**, **mktorrent(1)**
