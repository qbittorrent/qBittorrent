# Experimental streaming playback design

## Problem solved

Opening an incomplete qBittorrent payload file directly is not safe for media playback. The file may already have its final filesystem size while some BitTorrent pieces inside that byte range have not been downloaded and verified yet. A media player that reads those holes as if they were valid media bytes can show corrupted frames, decoder artifacts, freezes, or container errors.

The streaming feature therefore follows one central invariant:

> **No payload bytes are read from disk and sent to the player until every BitTorrent piece covering that read has been reported present by libtorrent.**

The player never reads the incomplete file directly. It reads an HTTP resource exposed by qBittorrent, and qBittorrent uses the existing torrent session as the source of truth for piece availability. Missing pieces are requested with ordered deadlines and the HTTP response waits rather than returning unverified bytes.

This verified-piece gate is the mechanism that makes playback of an incomplete video substantially different from simply opening a partially downloaded `.!qB`/payload file in a media player.

## Server design

Streaming playback is disabled by default. When enabled, qBittorrent starts a dedicated asynchronous HTTP listener using `QTcpServer` and the existing BitTorrent session. The default bind address is `127.0.0.1`; non-loopback binding requires a separate LAN opt-in. Wildcard addresses are rejected so generated URLs always contain an explicit usable host. An authenticated Web API action creates a URL containing a persistent cryptographically random token, torrent ID, and stable payload file index. The listener never accepts filesystem paths.

`HEAD` and single-range `GET` requests are supported, with at most one active GET stream. Pure helpers normalize HTTP ranges and map file-relative bytes through the torrent file offset to piece indexes. Before every positional disk read, a read gate verifies all covering pieces through libtorrent. Missing pieces receive ordered `set_piece_deadline()` requests with `alert_when_available` and a bounded read-ahead window. A new range replaces the active GET and resets only the piece deadlines owned by that stream. Streaming never changes file priorities, enables global sequential-download mode, creates a second libtorrent session, or copies torrent data.

The current output block size is 1 MiB. The socket write backlog is limited to 4 MiB. Piece availability is polled on a 100 ms single-shot timer while the current block is waiting, so waiting does not block the Qt event loop.

Stopped, checking, moving, errored, missing, removed, metadata-less, or priority-0 files fail closed. The resolved actual libtorrent path is used, including the `.!qB` name when append-extension mode applies. A timer polls piece verification without blocking the Qt event loop; timeout, cancellation, shutdown, storage changes, or an I/O error stops the response. `QFile` reads occur only after every piece covering that disk block reports `have_piece()`.

The default settings are `enabled=false`, `address=127.0.0.1`, `port=8081`, `read-ahead=128 MiB`, `wait timeout=30 seconds`, and `allow LAN=false`. The access token is created internally, saved in the profile, never returned by the preferences API, and remains mandatory for loopback and LAN requests. Settings changes reconfigure the listener immediately. The authenticated `torrents/streamUrl` Web API action accepts `hash` and a numeric stable file index and returns the tokenized URL used by the WebUI Stream / Play action.

## Playback flow

For an HTTP request, the server performs the following sequence:

1. Validate the token, torrent ID, file index, torrent state, and actual storage path.
2. Parse the requested byte range.
3. Map the next output block to its covering torrent pieces.
4. Extend that piece range by the configured read-ahead window.
5. Assign ordered libtorrent piece deadlines to missing pieces.
6. Check `have_piece()` for every piece covering the block that would be read now.
7. If any current piece is unavailable, wait and poll again; do not call `QFile::read()` for that block.
8. Once all current pieces are present, seek to the requested file offset, read the block, and write it to the HTTP socket.
9. Continue with the next block.
10. On a seek/new Range request, cancel the old active GET and replace its owned deadlines with priorities around the new playback position.

This means buffering may be visible when the required torrent pieces are unavailable, but the server prefers waiting or timing out over sending bytes that libtorrent has not confirmed.

## WebUI

When streaming is enabled, the torrent Files tab shows a dedicated Play column. Recognized video files whose download priority is not ignored receive a per-row play button, and the same action is available from the file context menu. Folders, subtitle/poster files, ignored files, and the Add Torrent file-selection dialog do not expose playback. The main torrent list intentionally has no generic play action because a multi-file torrent has no unambiguous playback target. Disabling streaming hides the Play column and its buttons without changing normal file-list behavior.

The currently recognized extensions are `.3g2`, `.3gp`, `.asf`, `.avi`, `.divx`, `.flv`, `.m2ts`, `.m4v`, `.mkv`, `.mov`, `.mp4`, `.mpeg`, `.mpg`, `.mts`, `.ogm`, `.ogv`, `.ts`, `.vob`, `.webm`, and `.wmv`.

## Initial limits

Initial limits are one HTTP byte range per request, one active GET, plain HTTP, no transcoding, no playlist generation, and cancellation rather than migration when storage paths change. Media format and seeking behavior depend on the client player and the torrent's piece availability.

The feature intentionally does not promise uninterrupted playback at insufficient torrent speed. Its correctness goal is narrower: **never substitute not-yet-verified torrent data for valid media bytes.**
