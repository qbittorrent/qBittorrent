# WebAPI Changelog

## 2.14.1
* [#23212](https://github.com/qbittorrent/qBittorrent/pull/23212)
  * Add `app/rotateAPIKey` endpoint for generating, and rotating, the WebAPI API key
* [#23388](https://github.com/qbittorrent/qBittorrent/pull/23388)
  * Add `app/deleteAPIKey` endpoint for deleting the existing WebAPI API key

## 2.14.0
* [#23202](https://github.com/qbittorrent/qBittorrent/pull/23202)
  * WebAPI responds with the error message "Endpoint does not exist" when the endpoint does not exist, to better differentiate from unrelated Not Found (i.e. 404) responses
  * `auth/login` endpoint responds to invalid credentials with a 401
  * `torrents/add` endpoint responds with `success_count`, `pending_count`, `failure_count`, and `added_torrent_ids`
    * When `pending_count` is non-zero, response code 202 is used
    * When all torrents fail to be added, response code 409 is used

## 2.13.1
* [#23163](https://github.com/qbittorrent/qBittorrent/pull/23163)
  * `torrents/add` endpoint now supports downloading from a search plugin via the `downloader` parameter
  * `torrents/fetchMetadata` endpoint now supports fetching from a search plugin via the `downloader` parameter
* [#23088](https://github.com/qbittorrent/qBittorrent/pull/23088)
  * Add `clientdata/load` and `clientdata/store` endpoints for managing WebUI-specific client settings and other shared data

## 2.13.0
* [#23045](https://github.com/qbittorrent/qBittorrent/pull/23045)
  * `torrents/trackers` returns three new fields: `next_announce`, `min_announce` and `endpoints`
    * `endpoints` is an array of tracker endpoints, each with `name`, `updating`, `status`, `msg`, `bt_version`, `num_peers`, `num_peers`, `num_leeches`, `num_downloaded`, `next_announce` and `min_announce` fields
  *  `torrents/trackers` now returns `5` and `6` in `status` field as possible values
    * `5` for `Tracker error` and `6` for `Unreachable`
* [#22963](https://github.com/qbittorrent/qBittorrent/pull/22963)
  * `torrents/editTracker` endpoint now supports setting a tracker's tier via `tier` parameter
  * `torrents/editTracker` endpoint always responds with a 204 when successful
  * `torrents/editTracker` endpoint `origUrl` parameter renamed to `url`
* [#23061](https://github.com/qbittorrent/qBittorrent/pull/23061)
  * `sync/torrentPeers` returns one new field: `i2p_dest`, only when the peer is from I2P
    * In this case, the fields `ip` and `port` are not returned
* [#23085](https://github.com/qbittorrent/qBittorrent/pull/23085)
  * `torrents/parseMetadata` now responds with an array of metadata in the same order as the files in the request. It previously responded with an object keyed off of the submitted file name.

## 2.12.1
* [#23031](https://github.com/qbittorrent/qBittorrent/pull/23031)
  * Add `torrents/setComment` endpoint with parameters `hashes` and `comment` for setting a new torrent comment

## 2.12.0

* [#22989](https://github.com/qbittorrent/qBittorrent/pull/22989)
  * `sync/maindata` returns one new field: `share_limit_action`
  * `torrents/setShareLimits` now requires a new `shareLimitAction` param that sets a torrent's shareLimitAction property
    * possible values `Default`, `Stop`, `Remove`, `RemoveWithContent` and `EnableSuperSeeding`

## 2.11.10

* [#22958](https://github.com/qbittorrent/qBittorrent/pull/22958)
  * `torrents/categories` and `sync/maindata` now serialize categories' `downloadPath` to `null`, rather than `undefined`
* [#22954](https://github.com/qbittorrent/qBittorrent/pull/22954)
  * `torrents/reannounce` supports specifying individual trackers via `trackers` field

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
