# WebAPI Changelog

## 2.11.10

* [#22932](https://github.com/qbittorrent/qBittorrent/pull/22932)
  * `torrents/categories` and `sync/maindata` now serialize categories' `downloadPath` to `null`, rather than `undefined`

## 2.11.9

* [#21015](https://github.com/qbittorrent/qBittorrent/pull/21015)
  * Add `torrents/fetchMetadata` endpoint for retrieving torrent metadata associated with a URL
  * Add `torrents/parseMetadata` endpoint for retrieving torrent metadata associated with a .torrent file
  * Add `torrents/saveMetadata` endpoint for saving retrieved torrent metadata to a .torrent file
  * `torrents/add` allows adding a torrent with metadata previously retrieved via `torrents/fetchMetadata` or `torrents/parseMetadata`
  * `torrents/add` allows specifying a torrent's file priorities
* [#22698](https://github.com/qbittorrent/qBittorrent/pull/22698)
  * `torrents/addTrackers` and `torrents/removeTrackers` now accept `hash=all` and adds/removes the tracker to/from *all* torrents
    * For compatibility, `torrents/removeTrackers` still accepts `hash=*` internally we transform it into `all`
  * Allow passing a pipe (`|`) separated list of hashes in `hash` for `torrents/addTrackers` and `torrents/removeTrackers`

## 2.11.8

* [#21349](https://github.com/qbittorrent/qBittorrent/pull/21349)
  * Handle sending `204 No Content` status code when response contains no data
    * Some endpoints still return `200 OK` to ensure smooth transition
* [#22750](https://github.com/qbittorrent/qBittorrent/pull/22750)
  * `torrents/info` allows an optional parameter `includeFiles` that defaults to `false`
    * Each torrent will contain a new key `files` which will list all files similar to the `torrents/files` endpoint
* [#22813](https://github.com/qbittorrent/qBittorrent/pull/22813)
  * `app/getDirectoryContent` allows an optional parameter `withMetadata` to send file metadata
    * Fields are `name`, `type`, `size`, `creation_date`, `last_access_date`, `last_modification_date`
    * See PR for TypeScript types

## 2.11.7

* [#22166](https://github.com/qbittorrent/qBittorrent/pull/22166)
  * `sync/maindata` returns 3 new torrent fields: `has_tracker_warning`, `has_tracker_error`, `has_other_announce_error`

## 2.11.6

* [#22460](https://github.com/qbittorrent/qBittorrent/pull/22460)
  * `app/setPreferences` allows only one of `max_ratio_enabled`, `max_ratio` to be present
  * `app/setPreferences` allows only one of `max_seeding_time_enabled`, `max_seeding_time` to be present
  * `app/setPreferences` allows only one of `max_inactive_seeding_time_enabled`, `max_inactive_seeding_time` to be present
