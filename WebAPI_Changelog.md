# WebAPI Changelog

## 2.11.9

* [#22863](https://github.com/qbittorrent/qBittorrent/pull/22863)
  * Introduce `torrents/bulkFiles`, accepts a list of `hash`es separated with `|` (pipe), returns list of files for all selected torrents

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
