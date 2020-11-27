Important Note
------------------------------------------
### To user:

Please do not use this modified BitTorrent client on Private Trackers,

unless qBittorrent Enhanced Edition is allowed on the Private Tracker(_Depend on which PT you're using._).

Otherwise, you will get banned.

You should ask a tracker operator to whitelist this client rather than asking a developer to change the Client Peer ID or User Agent.

### To tracker operators:

qBittorrent Enhanced is based on qBittorrent, it's aimed at blocking leeching clients automatically.

These modifications won't take effect on private torrents.

Also, qBittorrent Enhanced has a different ID announce to trackers.

User agent: `qBittorrent Enhanced/LATEST_RELEASE_VERSION`, example: `qBittorrent Enhanced/4.3.0.10`

PeerID: `-qB430[A-Z]-`, example: `-qB430A-`
********************************
### Multiple instances tutorial

If you're an advanced(or private tracker) user and you want to open 2 qBittorrent instances at the same time.

Instance 1 for Public torrent, Instance 2 for Private torrent or something else.

You can open Official qBittorrent or qBittorrent Enhanced first, as you like.

`Method 1:` Portable mode (Most suitable for Windows user)

_**AppImage does not support this method, please use method 2.**_

1. Create profile directory under qBittorrent executable directory.

2. Open executable.

3. (Optional) Create a shortcut to the desktop.

qBittorrent should enter to Portable mode now.

`Method 2:` `configuration` command (Most suitable for *nix user)

1. Go to qBittorrent executable directory

2. Open command line, and type it in cmd

Windows:

`qbittorrent.exe --configuration="new_instance_profile_name"`

*nix:

If you're using a self-compiled version and installed to system

`qbittorrent --configuration="new_instance_profile_name"`

If you're using _qbittorrent-nox_linux_x64_static_build_

`./qbittorrent-nox --configuration="new_instance_profile_name"`

If you're using AppImage

`./qBittorrent-Enhanced-Edition.AppImage
 --configuration="new_instance_profile_name"`

qBittorrent should create a new configuration and use it for the new instance.
