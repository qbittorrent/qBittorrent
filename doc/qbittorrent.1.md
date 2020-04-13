% QBITTORRENT(1) BitTorrent client written in C++ / Qt
%
% 2020-03-20

# NAME

**qBittorrent** - BitTorrent client written in C++ / Qt

# SYNOPSIS

| **qbittorrent** \[_OPTIONS_\] \[_URI_...\]
| **qbittorrent** **`-h` | `-v`**

# DESCRIPTION

**qBittorrent** is an advanced BitTorrent client written in C++ / Qt, using the **libtorrent-rasterbar** library by Arvid Norberg as the backend.
qBittorrent provides a familiar, easy-to-use GUI, and is fast, stable and light.

qBittorrent supports several important modern features and BEPs (BitTorrent Enhancement Proposals) such as UPnP port forwarding / NAT-PMP, DHT (Mainline) and magnet URI (also known as magnet link) support, IPv6, uTP, PeX, LSD, UDP tracker protocol support and more.
Refer to the **libtorrent-rasterbar** documentation for the full list of features and supported BEPs.

In addition, qBittorrent supports Unicode and it provides a good integrated Python-based torrent search engine, an RSS downloader, a torrent creation GUI, a basic embedded tracker to facilitate sharing torrents with friends, and an HTTP API/Web UI combo for remote access/administration.

When qBittorrent is started from a terminal, its **stdout** and **stderr** will be attached to that terminal.

If no arguments are specified, qBittorrent simply starts and shows the GUI.

If either **`-h`** or **`-v`** is specified, the relevant information is printed to the terminal and qBittorrent exits.
Passing any other option alongside **`-h`** or **`-v`** is not allowed.

_URI_ is a string pointing to a BitTorrent metainfo file (a "torrent").
It may be a path to a file on disk or an online resource accessible through a supported URI scheme such as **magnet**, **HTTP**, or **HTTPS**.

Standalone **info-hashes** are also accepted as shorthand for a bare-bones **magnet** _URI_ constructed from that **info-hash**.
For example, passing the **info-hash** _`e73108cbd628fee5cf203acdf668c5bf45d07810`_ (for the file `ubuntu-18.04.4-live-server-amd64.iso`) is the same as passing the **magnet** _URI_ _`magnet:?xt=urn:btih:e73108cbd628fee5cf203acdf668c5bf45d07810`_.

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

qBittorrent can be cleanly terminated by fully closing the GUI (including the system tray, if that option is enabled) or by sending the **SIGINT** or **SIGTERM** signals to its process.

# OPTIONS

## Generic program information

`-h`, `--help`

:    Display a short help text about the command-line options and exit.

`-v`, `--version`

:    Display program version and exit.

## General configuration

`--configuration=`_NAME_

:    Use a different configuration with name _NAME_.
This enables running multiple separate instances of qBittorrent at the same time without conflict (provided they don't listen on the same ports for WebUI access, incoming connections, etc; in this case, the additional instances will either fail to start or be functionally unusable).

    Configuration and data files will be stored under a configuration hierarchy similar to the default one (see the _FILES_ section for more information) but with a final directory named **qBittorrent\__NAME_** instead of **qBittorrent**.

    For example, while the main configuration file is stored as _~/.config/qBittorrent/qBittorrent.conf_, the configuration file for a configuration named **foo** will be stored as _~/.config/qBittorrent\_**foo**/qBittorrent.conf_.

    If a configuration with the given _NAME_ does not exist yet, it is created with default settings.
    If it already exists, it is loaded as a normal configuration would be.

    If both **`--configuration`** and **`--profile`** are specified, only **`--profile`** takes effect.

`--no-splash`

:    Do not display the splash screen on startup.

`--profile=`_PATH_

:    Store configuration _FILES_ in _PATH_.
Similarly to **`--configuration`**, this option can be used to run multiple separate instances of qBittorrent at the same time without conflitcts (the same caveats about listening port conflicts and other potential unavoidable conflicts apply).

    The difference is that configuration files are all stored together under the specified configuration _PATH_, instead of scattered under a separate directory hierarchy in the system's configuration directories (see the _FILES_ section for more information).

    If a special directory literally named **`profile`** exists in the _same directory as the qBittorrent executable_, executing qBittorrent with no **`--profile=`** option is the same as executing qBittorrent from the qBittorrent executable directory with the options **`--profile=profile --relative-fastresume`**.
This special case is known as the "portable mode" in qBittorrent.
Note that if this special directory is present in the qBittorrent executable directory, but a non-empty **`--profile=`** is specified, the command line option will take precedence and **`--relative-fastresume`** will not be implied.
Specifying a non-empty **`--profile=`** together with **`--relative-fastresume`** explicitly is functionally equivalent to the "portable mode", but under a profile diretory of choice, other than the special directory described above.

    If _PATH_ does not exist, it is created, with configuration files initialized with default settings inside it that a new instance of qBittorrent then loads.
If it exists, the configurations are loaded from there.

    A relative _PATH_ may be used, in which case it will be relative to the current working directory that qBittorrent is started from.
Note that **`~`** will not get expanded, and will be interpreted as a directory literally named **`~`**.

    **Keep in mind that there are some caveats when using this option, with regards to the portability of profiles between machines:**

    - In general, the _.fastresume_ files stored in the _BT_backup_ directory are not portable between machines, unless they were generated by a qBittorrent instance started with **`--relative-fastresume`**, and the savepaths are able to be represented in a relative form (i.e. no paths to a different disk or UNC paths on Windows). Note that **`--relative-fastresume`** does not change anything in the _.fastresume_ files of previously added torrents. See the _FILES_ section for more information about the _BT_backup_ directory and the files stored within it.
    - Path-related settings stored in the main configuration file (such as save paths) might represent paths which are not be present on the target machine, and thus may need to be adjusted after migrating.

    If both **`--configuration`** and **`--profile`** are specified, only **`--profile`** takes effect.

`--relative-fastresume`

:   **Don't use this option explicitly unless you know you need it.
If you have to ask, you don't need it.**

    Make the paths of torrents' files stored in the _.fastresume_ files be relative to the **`--profile`** directory.

    If used explicitly without **`--profile`**, paths will be relative to the directory specfied by the **HOME** enviroment variable instead, which is the home directory of the current user by default.

    Implied when running in "portable mode", refer to the documentation of the **`--profile`** option to learn more.

`--webui-port=`_NUMBER_

:    Specify the port _NUMBER_ the WebUI should listen on.
The default if not specified is 8080.

## Options to control added torrents

These options only apply to the added torrents passed in \[_URI_...\].
If no torrent is specified, these options are ignored.

`--add-paused=`_TOGGLE_

:    _TOGGLE_ is one of **true** or **false**.
If **true**, the torrent(s) will not be automatically started.
Defaults to **false**.
Note that if a torrent's files exist in the destination save path, a recheck will start, unless **`--skip-hash-check`** is used.

`--category=`_NAME_

:    Assign the torrent(s) to the category _NAME_.
If this category does not exist, it will be automatically created.

`--first-and-last`

:    Download first and last pieces first.

`--save-path=`_PATH_
:    Save the torrent(s) to the specified _PATH_.
A relative _PATH_ may be used, in which case it will be relative to the current working directory that qBittorrent is started from.
Note that **`~`** will not get expanded, and will be interpreted as a directory literally named **`~`**.

`--sequential`
:    Download pieces in sequential order.

`--skip-dialog=`_TOGGLE_

:    _TOGGLE_ is one of **true** or **false**.
If **true**, the "Add New Torrent" dialog will not be shown when adding torrents.
Defaults to **false**.

`--skip-hash-check`

:    By default, if a torrent's files exist in the destination save path, a recheck is initiated.
This option prevents that from happening, which is not recommended in general.
Torrents may still be force-rechecked after they are added, as usual.
If the torrent's files do not exist in the destination save path, this option has no effect.

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

:    _BT_backup_ directory lock file.

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

# EXAMPLE

- Start qBittorrent, setting the WebUI to listen on port 9000

    `qbittorrent --webui-port=9000`

- Start qBittorrent, setting the WebUI to listen on port 9000, and add a torrent:

    `qbittorrent --webui-port=9000 ubuntu-18.04.4-desktop-amd64.iso.torrent`

- Start qBittorrent, setting the WebUI to listen on port 9000, and add a torrent immediately (skipping the "Add New Torrent" dialog) in paused state, configured to download first and last pieces first and sequentially:

    `qbittorrent --webui-port=9000 --first-and-last --sequential --skip-dialog=true --add-paused=true ubuntu-18.04.4-desktop-amd64.iso.torrent`

- Similar to the previous example, but adding 3 torrents instead of one; the torrent addition options are applied to all of them.
Note that one torrent is added as a magnet URI (for the file `ubuntu-18.04.4-live-server-amd64.iso`):

    `qbittorrent --webui-port=9000 --first-and-last --sequential --skip-dialog=true --add-paused=true ubuntu-18.04.4-desktop-amd64.iso.torrent some_torrent.torrent magnet:?xt=urn:btih:e73108cbd628fee5cf203acdf668c5bf45d07810`

- The following just starts qBittorrent as if no other options were passed; even though 3 torrent addition options were specified, there are no torrents being added to apply them to:

    `qBittorrent --category=foo --sequential --first-and-last`

- Start three separate instances of qBittorrent to run at the same time; two of which use configuration files stored in the system's configuration directories, and a third one using a self-contained configuration directory.
In all cases, the paths stored in the _.fastresume_ files are absolute.

    `qBittorrent --configuration=public_trackers`

    `qBittorrent --configuration=private_trackers`

    `qBittorrent --profile=Downloads/bobs_epic_portable_qbt_settings`

# SEE ALSO

**btcheck(1)**, **mktorrent(1)**
