# Per-file download order and sequential download

qBittorrent lets you choose the order in which the files of a torrent are downloaded,
and lets you download individual files in sequential order.

Example: a torrent has files `A`, `B` and `C`. You want `B` first, then `C`, then `A`,
and you want `A` and `C` to be downloaded from start to end (for example to preview them
while they download):

| File | Download Order | Sequential |
|------|----------------|------------|
| A    | 3              | yes        |
| B    | 1              | no         |
| C    | 2              | yes        |

Related feature request: [#21430](https://github.com/qbittorrent/qBittorrent/issues/21430).

## Using it

### Desktop GUI

In the torrent **Content** tab (and in the **Add new torrent** dialog) there are two columns:

* **Download Order** - click the cell and enter a number.
  * `None` (0) means the file has no explicit order.
  * Files with a lower number are downloaded first. Any number from 1 to 2147483647 can be used,
    so very large torrents can be fully ordered.
  * Files with the same number are downloaded together.
* **Sequential** - when checked, the pieces of the file are downloaded in order.

The context menu of the content list has a **Download order** submenu:

* **Download first** - move the selected files to the top of the order.
* **Download last** - move the selected files to the bottom of the order.
* **By shown file order** - give the selected files consecutive numbers, in the order they are
  shown, after the files that are already ordered.
* **Remove from download order** - set the selected files back to `None`.

and a **Download in sequential order** item that toggles the sequential flag of the selected files.
Selecting a folder applies the action to all files inside it.

The **File download order** mode is set in **Torrent options** (and in the **Add new torrent** dialog):

| Mode                    | Behaviour |
|-------------------------|-----------|
| **Soft** (default)      | Ordered files are preferred in their order: the next file in the order gets the most bandwidth. Other files can still be downloaded at the same time (libtorrent also prefers rare pieces), which keeps the download fast and helps the swarm. |
| **Ordered files first** | Ordered files are downloaded in their order, with the same preference as in *Soft*: the next file in the order gets the most bandwidth, and later ordered files can use spare bandwidth. Files without an order are not downloaded at all until every ordered file is complete. |
| **Strict**              | Only the next file(s) in the order are downloaded. Everything else waits. This can leave bandwidth unused when the next file is rare in the swarm. |

In *Strict* mode, files without an order are not ignored. They wait until every ordered file is complete,
and are then downloaded one priority level at a time: first the *Maximum* files, then the *High* files,
then the *Normal* files.

The normal file **Priority** still works:

* *Do not download* always wins: such files are skipped, whatever their order is.
* For files with an order, the order replaces the priority: *Normal*, *High* and *Maximum* make no
  difference to when they are downloaded.
* Files without an order keep their relative priority (*Maximum* > *High* > *Normal*),
  but they always come after ordered files.

The torrent-wide **Download in sequential order** option cannot be combined with per-file ordering
(libtorrent's sequential mode ignores most piece priorities):

* Turning on torrent-wide sequential download suspends per-file ordering. The order and sequential flags
  of the files are kept and apply again when it is turned off.
* Setting a download order or a sequential flag on a file turns torrent-wide sequential download off.

**Download first and last pieces first** keeps working, but while per-file ordering is in use it only applies
to the file(s) at the head of the order: their first and last pieces get priority 7. Raising the first and last
pieces of every file would break the order.

### WebUI and WebAPI

The WebUI offers the same columns and context menu entries. The WebAPI provides:

* `torrents/files` - every file has `order` (int, `0` = no order) and `sequential` (bool) fields.
* `torrents/setFileOrder` - `hash`, `id` (file indexes separated by `|`), `order` (int >= 0).
* `torrents/setFileSequential` - `hash`, `id` (file indexes separated by `|`), `value` (bool).
* `torrents/setFileSchedulingMode` - `hashes` (separated by `|`), `mode` (`Soft`, `RankedFirst` or `Strict`).
* `torrents/info` and `sync/maindata` - every torrent has a `file_scheduling_mode` field.
* `torrents/add` - accepts an optional `fileSchedulingMode` parameter, and optional `fileOrder` (one integer
  per file, separated by `,`) and `fileSequential` (`true` or `false` per file, separated by `,`) parameters.
  Like `filePriorities`, they can only be used for a single torrent whose metadata has already been fetched,
  which is how the WebUI's add torrent dialog adds torrents.

See [WebAPI_Changelog.md](../WebAPI_Changelog.md).

## How it works

libtorrent, the BitTorrent library used by qBittorrent, only offers:

* 8 piece priority levels: 0 (do not download) and 1 to 7,
* one torrent-wide "sequential download" flag.

Priorities 1 to 7 are not strict levels. libtorrent's rarest-first picker sorts pieces by
`availability * (8 - priority)` (see `piece_pos::priority()` in libtorrent's `piece_picker.hpp`),
so the priority is a weight against rarity: a priority 7 piece is preferred over a priority 1 piece
unless the priority 1 piece is more than 7 times rarer. Only priority 0 is strict: the piece is not
downloaded at all.

This is why there are three modes: *Soft* only uses weights, while *Ordered files first* and *Strict*
also hold pieces back with priority 0.

libtorrent also offers piece deadlines ("time critical" pieces). qBittorrent does not use them:
combined with pieces held at priority 0 they can stall the download, and they are known to reduce
the download speed. Everything below is done with piece priorities only.

It has no notion of "download file A before file B" for an arbitrary number of files.
qBittorrent therefore builds a schedule on top of these primitives and updates it as the download
progresses. No change to libtorrent is needed.

The code lives in:

* `src/base/bittorrent/filescheduler.h/.cpp` - the scheduler. It is a pure function: it takes the files,
  their pieces and the pieces we already have, and returns the piece priorities to apply.
  The header contains the detailed rules. Unit tests: `test/testbittorrentfilescheduler.cpp`.
* `src/base/bittorrent/torrentimpl.cpp` - `TorrentImpl::updateFileSchedule()` collects the input,
  applies the result to libtorrent (only if it changed) and runs again whenever a piece completes or
  a setting changes. libtorrent applies file priorities asynchronously and then recomputes all piece
  priorities from them, so the schedule is applied again when libtorrent reports that file priorities
  have been applied.

### The queue

Wanted files that are not complete yet are put into a queue of groups:

1. files with an order, grouped by order number, lowest first;
2. files without an order, grouped by priority, highest first.

The first group is the *head* of the queue.

### Mapping the queue to libtorrent priorities

| Queue position            | Soft / Ordered files first      | Strict |
|---------------------------|---------------------------------|--------|
| 1st ordered group (head)  | 7                               | 7      |
| 2nd ordered group         | 6                               | 0      |
| 3rd ordered group         | 5                               | 0      |
| 4th and later groups      | 4                               | 0      |
| no order, *Maximum*       | 3 (*Ordered files first*: 0)    | 0      |
| no order, *High*          | 2 (*Ordered files first*: 0)    | 0      |
| no order, *Normal*        | 1 (*Ordered files first*: 0)    | 0      |

In *Soft* mode the next file in the order gets priority 7 and is strongly preferred; later files are
preferred less and less, and files without an order are preferred least. Rare pieces of lower files can
still be picked earlier (see above), so the order is a strong preference, not a guarantee. Use
*Ordered files first* or *Strict* when it must be a guarantee.

Only the first few groups get their own level, but that does not limit the number of orders:
the schedule is rebuilt every time a file completes, so every file moves up one level at a time
and reaches level 7 when it is next in line. The order is exact at the top of the queue, which is
the part that is being downloaded.

### Sequential files

* **At the head of the queue:** the next N missing pieces of the file (the "window") get priority 7,
  and the rest of the file gets priority 6. N is about 8 MiB worth of pieces (between 4 and 16 pieces).
  The window is strongly preferred, and the rest of the file uses spare bandwidth instead of wasting it
  (also in *Strict* mode). As pieces complete, the window moves forward, so the file is downloaded from
  start to end. Inside the window, pieces can arrive in any order, which is why the window is kept small.
* **Further down the queue:** only the next N missing pieces of the file get the file's level, the rest
  of the file is held at priority 0. As pieces complete, the window moves forward, so the file is filled
  from start to end using spare bandwidth.

### Pieces shared by files

A piece can contain the end of one file and the start of the next one. Such a piece gets the higher
of the two priorities, i.e. it is downloaded together with the earlier file in the order.

### Held pieces and the "finished" state

*Ordered files first*, *Strict* and sequential windows hold back pieces of wanted files by giving them
priority 0. libtorrent does not count such pieces as wanted, which has two side effects that qBittorrent
handles:

* **Progress, size and ETA** - while pieces are held, qBittorrent computes the wanted size and the progress
  from the files instead of using libtorrent's numbers, so the progress bar still reflects the whole download.
* **False "torrent finished" events** - libtorrent considers a torrent finished when every piece with
  priority above 0 is downloaded, which could happen for a moment when the head of the queue completes.
  To prevent this, when fewer than 16 non-held pieces are missing, held pieces of the next files are
  released at priority 1 ("lookahead") until 16 pieces are wanted again. So near the end of a file up to
  16 pieces of the following files may be downloaded early. If libtorrent still reports the torrent as finished
  while pieces are held, qBittorrent moves the schedule forward and ignores the event, so actions such as
  moving the files to the save path, running the "on finished" program or showing a notification only
  happen when the whole torrent is complete.

### Storage

The order, the sequential flags and the mode are saved with the rest of the torrent state:

* fastresume files: `qBt-fileOrder` (list of int), `qBt-fileSequential` (list of int),
  `qBt-fileSchedulingMode` (string);
* SQLite resume database (version 11): `file_order`, `file_sequential` (comma separated integers),
  `file_scheduling_mode` columns.

Empty lists mean "not set". The values are indexed like the files of the torrent (padding files excluded).
