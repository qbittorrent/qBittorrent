This WebUI API documentation applies to qBittorrent v4.1+. For other WebUI API versions, visit [WebUI API](https://github.com/qbittorrent/qBittorrent/wiki#WebUI-API).

# Table of Contents #

1. [Changes](#changes)
   1. [API v2.0](#api-v20)
   1. [API v2.0.1](#api-v201)
   1. [API v2.0.2](#api-v202)
   1. [API v2.1.0](#api-v210)
   1. [API v2.1.1](#api-v211)
   1. [API v2.2.0](#api-v220)
   1. [API v2.2.1](#api-v221)
   1. [API v2.3.0](#api-v230)
   1. [API v2.4.0](#api-v240)
   1. [API v2.4.1](#api-v241)
   1. [API v2.5.0](#api-v250)
   1. [API v2.5.1](#api-v251)
   1. [API v2.6.0](#api-v260)
   1. [API v2.6.1](#api-v261)
   1. [API v2.6.2](#api-v262)
   1. [API v2.7.0](#api-v270)
   1. [API v2.8.0](#api-v280)
   1. [API v2.8.1](#api-v281)
   1. [API v2.8.2](#api-v282)
   1. [API v2.8.3](#api-v283)
1. [General information](#general-information)
1. [Authentication](#authentication)
   1. [Login](#login)
   1. [Logout](#logout)
1. [Application](#application)
   1. [Get application version](#get-application-version)
   1. [Get API version](#get-api-version)
   1. [Get build info](#get-build-info)
   1. [Shutdown application](#shutdown-application)
   1. [Get application preferences](#get-application-preferences)
   1. [Set application preferences](#set-application-preferences)
   1. [Get default save path](#get-default-save-path)
1. [Log](#log)
   1. [Get log](#get-log)
   1. [Get peer log](#get-peer-log)
1. [Sync](#sync)
   1. [Get main data](#get-main-data)
   1. [Get torrent peers data](#get-torrent-peers-data)
1. [Transfer info](#transfer-info)
   1. [Get global transfer info](#get-global-transfer-info)
   1. [Get alternative speed limits state](#get-alternative-speed-limits-state)
   1. [Toggle alternative speed limits](#toggle-alternative-speed-limits)
   1. [Get global download limit](#get-global-download-limit)
   1. [Set global download limit](#set-global-download-limit)
   1. [Get global upload limit](#get-global-upload-limit)
   1. [Set global upload limit](#set-global-upload-limit)
   1. [Ban peers](#ban-peers)
1. [Torrent management](#torrent-management)
   1. [Get torrent list](#get-torrent-list)
   1. [Get torrent generic properties](#get-torrent-generic-properties)
   1. [Get torrent trackers](#get-torrent-trackers)
   1. [Get torrent web seeds](#get-torrent-web-seeds)
   1. [Get torrent contents](#get-torrent-contents)
   1. [Get torrent pieces' states](#get-torrent-pieces-states)
   1. [Get torrent pieces' hashes](#get-torrent-pieces-hashes)
   1. [Pause torrents](#pause-torrents)
   1. [Resume torrents](#resume-torrents)
   1. [Delete torrents](#delete-torrents)
   1. [Recheck torrents](#recheck-torrents)
   1. [Reannounce torrents](#reannounce-torrents)
   1. [Edit trackers](#edit-trackers)
   1. [Remove trackers](#remove-trackers)
   1. [Add peers](#add-peers)
   1. [Add new torrent](#add-new-torrent)
   1. [Add trackers to torrent](#add-trackers-to-torrent)
   1. [Increase torrent priority](#increase-torrent-priority)
   1. [Decrease torrent priority](#decrease-torrent-priority)
   1. [Maximal torrent priority](#maximal-torrent-priority)
   1. [Minimal torrent priority](#minimal-torrent-priority)
   1. [Set file priority](#set-file-priority)
   1. [Get torrent download limit](#get-torrent-download-limit)
   1. [Set torrent download limit](#set-torrent-download-limit)
   1. [Set torrent share limit](#set-torrent-share-limit)
   1. [Get torrent upload limit](#get-torrent-upload-limit)
   1. [Set torrent upload limit](#set-torrent-upload-limit)
   1. [Set torrent location](#set-torrent-location)
   1. [Set torrent name](#set-torrent-name)
   1. [Set torrent category](#set-torrent-category)
   1. [Get all categories](#get-all-categories)
   1. [Add new category](#add-new-category)
   1. [Edit category](#edit-category)
   1. [Remove categories](#remove-categories)
   1. [Add torrent tags](#add-torrent-tags)
   1. [Remove torrent tags](#remove-torrent-tags)
   1. [Get all tags](#get-all-tags)
   1. [Create tags](#create-tags)
   1. [Delete tags](#delete-tags)
   1. [Set automatic torrent management](#set-automatic-torrent-management)
   1. [Toggle sequential download](#toggle-sequential-download)
   1. [Set first/last piece priority](#set-firstlast-piece-priority)
   1. [Set force start](#set-force-start)
   1. [Set super seeding](#set-super-seeding)
   1. [Rename file](#rename-file)
   1. [Rename folder](#rename-folder)
1. [RSS (experimental)](#rss-experimental)
   1. [Add folder](#add-folder)
   1. [Add feed](#add-feed)
   1. [Remove item](#remove-item)
   1. [Move item](#move-item)
   1. [Get all items](#get-all-items)
   1. [Mark as read](#mark-as-read)
   1. [Refresh item](#refresh-item)
   1. [Set auto-downloading rule](#set-auto-downloading-rule)
   1. [Rename auto-downloading rule](#rename-auto-downloading-rule)
   1. [Remove auto-downloading rule](#remove-auto-downloading-rule)
   1. [Get all auto-downloading rules](#get-all-auto-downloading-rules)
   1. [Get all articles matching a rule](#get-all-articles-matching-a-rule)
1. [Search](#search)
   1. [Start search](#start-search)
   1. [Stop search](#stop-search)
   1. [Get search status](#get-search-status)
   1. [Get search results](#get-search-results)
   1. [Delete search](#delete-search)
   1. [Get search plugins](#get-search-plugins)
   1. [Install search plugin](#install-search-plugin)
   1. [Uninstall search plugin](#uninstall-search-plugin)
   1. [Enable search plugin](#enable-search-plugin)
   1. [Update search plugins](#update-search-plugins)
1. [WebAPI versioning](#webapi-versioning)

***

# Changes #

## API v2.0 ##

- New version naming scheme: X.Y.Z (where X - major version, Y - minor version, Z - release version)
- New API paths. All API methods are under `api/vX/` (where X is API major version)
- API methods are under new scopes

## API v2.0.1 ##

- Add `hashes` field to `/torrents/info` ([#8782](https://github.com/qbittorrent/qBittorrent/pull/8782))
- Add `/torrents/setShareLimits/` method ([#8598](https://github.com/qbittorrent/qBittorrent/pull/8598))

## API v2.0.2 ##

- Add `/torrents/reannounce` method ([#9229](https://github.com/qbittorrent/qBittorrent/pull/9229))

## API v2.1.0 ##

- Change `/sync/maindata` `categories` field from `array` to `object` ([#9228](https://github.com/qbittorrent/qBittorrent/pull/9228))
- Add `savePath` field to `/torrents/createCategory` ([#9228](https://github.com/qbittorrent/qBittorrent/pull/9228)). This method now requires the category to already exist and will not create new categories.
- Add `/torrents/editCategory` method ([#9228](https://github.com/qbittorrent/qBittorrent/pull/9228))

## API v2.1.1 ##

- Add `/torrents/categories` method ([#9586](https://github.com/qbittorrent/qBittorrent/pull/9586))
- Add `/search/` methods ([#8584](https://github.com/qbittorrent/qBittorrent/pull/8584))
- Add `free_space_on_disk` field to `/sync/maindata` ([#8217](https://github.com/qbittorrent/qBittorrent/pull/8217))

## API v2.2.0 ##

- Add `/torrents/editTracker` and `/torrents/removeTracker` methods ([#9375](https://github.com/qbittorrent/qBittorrent/pull/9375))
- Add `tier`, `num_seeds`, `num_leeches`, and `num_downloaded` fields to `/torrents/trackers` ([#9375](https://github.com/qbittorrent/qBittorrent/pull/9375))
- Change `status` field from translated string to an integer for `/torrents/trackers` ([#9375](https://github.com/qbittorrent/qBittorrent/pull/9375))
- Change `/torrents/filePrio` `id` field to accept multiple ids ([#9541](https://github.com/qbittorrent/qBittorrent/pull/9541))
- Throw additional errors for failed requests to `/torrents/filePrio` ([#9541](https://github.com/qbittorrent/qBittorrent/pull/9541))
- Add `autoTMM` field to `/torrents/add` ([#9752](https://github.com/qbittorrent/qBittorrent/pull/9752))
- Add various fields to `/app/getPreferences` and `/app/setPreferences` (`create_subfolder_enabled`, `start_paused_enabled`, `auto_delete_mode`, `preallocate_all`, `incomplete_files_ext`, `auto_tmm_enabled`, `torrent_changed_tmm_enabled`, `save_path_changed_tmm_enabled`, `category_changed_tmm_enabled`, `mail_notification_sender`, `limit_lan_peers`, `slow_torrent_dl_rate_threshold`, `slow_torrent_ul_rate_threshold`, `slow_torrent_inactive_timer`, `alternative_webui_enabled`, `alternative_webui_path`) ([#9752](https://github.com/qbittorrent/qBittorrent/pull/9752))

## API v2.2.1 ##

- Add `rss/refreshItem` ([#11067](https://github.com/qbittorrent/qBittorrent/pull/11067))

## API v2.3.0 ##

- Remove `web_ui_password` field from `/app/preferences`, this field is still writable in `/app/setPreferences` method ([#9942](https://github.com/qbittorrent/qBittorrent/pull/9942))
- Add `/app/buildInfo` method ([#10096](https://github.com/qbittorrent/qBittorrent/pull/10096))
- Always use `/` as path separator in `/torrents/files` response ([#10153](https://github.com/qbittorrent/qBittorrent/pull/10153/))
- Add `/torrents/addPeers` and `/transfer/banPeers` methods ([#10158](https://github.com/qbittorrent/qBittorrent/pull/10158))
- Add `/torrents/addTags`, `/torrents/removeTags`, `/torrents/tags`, `/torrents/createTags`, `/torrents/deleteTags` methods ([#10527](https://github.com/qbittorrent/qBittorrent/pull/10527))

## API v2.4.0 ##

- Add `/torrents/renameFile` method ([#11029](https://github.com/qbittorrent/qBittorrent/pull/11029))

## API v2.4.1 ##

- Add `stalled`, `stalled_uploading` and `stalled_downloading` as possible values for the `filter` parameter in `/torrents/info` ([#11825](https://github.com/qbittorrent/qBittorrent/pull/11825))
- Add various fields to `/app/preferences` and `/app/setPreferences` (`piece_extent_affinity`, `web_ui_secure_cookie_enabled`, `web_ui_max_auth_fail_count`, `web_ui_ban_duration`, `stop_tracker_timeout`) ([#11781](https://github.com/qbittorrent/qBittorrent/pull/11781), [#11726](https://github.com/qbittorrent/qBittorrent/pull/11726), [#12004](https://github.com/qbittorrent/qBittorrent/pull/12004), [#11834](https://github.com/qbittorrent/qBittorrent/pull/11834))

## API v2.5.0 ##
- Removes `enable_super_seeding` as fields from `/app/preferences` and `/app/setPreferences`  ([#12423](https://github.com/qbittorrent/qBittorrent/pull/12423))

## API v2.5.1 ##
- Add `web_ui_use_custom_http_headers_enabled`, `web_ui_custom_http_headers`, `rss_download_repack_proper_episodes` and `rss_smart_episode_filters` as fields to `/app/preferences` and `/app/setPreferences` ([#12579](https://github.com/qbittorrent/qBittorrent/pull/12579), [#12549](https://github.com/qbittorrent/qBittorrent/pull/12549))
- Add `/rss/markAsRead` and `/rss/matchingArticles` methods ([#12549](https://github.com/qbittorrent/qBittorrent/pull/12549))

## API v2.6.0 ##
- Removed `/search/categories` method and modified `/search/plugins` method's response ([#12705](https://github.com/qbittorrent/qBittorrent/pull/12705))

## API v2.6.1 ##
- Exposed `contentPath` via the `content_path` field in the response to `/torrents/info` ([#13625](https://github.com/qbittorrent/qBittorrent/pull/13625))

## API v2.6.2 ##
- Added `tags` optional field to `/torrents/add` ([#13882](https://github.com/qbittorrent/qBittorrent/pull/13882))

## API v2.8.0 ##
- Added `/torrents/renameFolder` method and modified `/torrents/renameFile` method's parameters ([#13995](https://github.com/qbittorrent/qBittorrent/pull/13995))

Note that this change was released in qBittorrent v4.3.3, but the WebAPI version was incorrectly set to v2.7.0 (see [#14275](https://github.com/qbittorrent/qBittorrent/pull/14275#issuecomment-766310862) for details)

## API v2.8.1 ##
- Added `ratioLimit` and `seedingTimeLimit` optional fields to `/torrents/add` ([#14519](https://github.com/qbittorrent/qBittorrent/pull/14519))
- Added `seeding_time` field to `/torrents/info` ([#14554](https://github.com/qbittorrent/qBittorrent/pull/14554))

## API v2.8.2 ##
- Added `indexes` optional parameter to `/torrents/files` ([#14795](https://github.com/qbittorrent/qBittorrent/pull/14795))
- Added `index` field to `/torrents/files` response ([#14795](https://github.com/qbittorrent/qBittorrent/pull/14795))

## API v2.8.3 ##
- Added `tag` optional parameter to `/torrents/info` ([#15152](https://github.com/qbittorrent/qBittorrent/pull/15152))

# General Information #

- All API methods follows the format `/api/v2/APIName/methodName`, where `APIName` is a certain subgroup of API methods whose functionality is related.
- All API methods only allows `GET` or `POST` methods. Use `POST` when you are mutating some state (or when your request is too big to fit into `GET`) and use `GET` otherwise. Starting with qBittorrent v4.4.4, server will return `405 Method Not Allowed` when you used the wrong request method.
- All API methods require [authentication](#authentication) (except `/api/v2/auth/login`, obviously).

# Authentication #

All Authentication API methods are under "auth", e.g.: `/api/v2/auth/methodName`.

qBittorrent uses cookie-based authentication.

## Login ##

Name: `login`

**Parameters:**

Parameter                         | Type    | Description
----------------------------------|---------|------------
`username`                        | string  | Username used to access the WebUI
`password`                        | string  | Password used to access the WebUI

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
403                               | User's IP is banned for too many failed login attempts
200                               | All other scenarios

Upon success, the response will contain a cookie with your SID. You must supply the cookie whenever you want to perform an operation that requires authentication.

Example showing how to login and execute a command that requires authentication using `curl`:

```sh
$ curl -i --header 'Referer: http://localhost:8080' --data 'username=admin&password=adminadmin' http://localhost:8080/api/v2/auth/login
HTTP/1.1 200 OK
Content-Encoding:
Content-Length: 3
Content-Type: text/plain; charset=UTF-8
Set-Cookie: SID=hBc7TxF76ERhvIw0jQQ4LZ7Z1jQUV0tQ; path=/
$ curl http://localhost:8080/api/v2/torrents/info --cookie "SID=hBc7TxF76ERhvIw0jQQ4LZ7Z1jQUV0tQ"
```

Note: Set `Referer` or `Origin` header to the exact same domain and port as used in the HTTP query `Host` header.

## Logout ##

Name: `logout`

**Parameters:**

None

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

# Application #

All Application API methods are under "app", e.g.: `/api/v2/app/methodName`.

## Get application version ##

Name: `version`

**Parameters:**

None

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

The response is a string with the application version, e.g. `v4.1.3`

## Get API version ##

Name: `webapiVersion`

**Parameters:**

None

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

The response is a string with the WebAPI version, e.g. `2.0`

## Get build info ##

Name: `buildInfo`

**Parameters:**

None

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios- see JSON below

The response is a JSON object containing the following fields

Property     | Type    | Description
-------------|---------|------------
`qt`         | string  | QT version
`libtorrent` | string  | libtorrent version
`boost`      | string  | Boost version
`openssl`    | string  | OpenSSL version
`bitness`    | int     | Application bitness (e.g. 64-bit)

## Shutdown application ##

Name: `shutdown`

**Parameters:**

None

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

## Get application preferences ##

Name: `preferences`

**Parameters:**

None

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios- see JSON below

The response is a JSON object with several fields (key-value) pairs representing the application's settings. The contents may vary depending on which settings are present in qBittorrent.ini.

Possible fields:

Property                                 | Type    | Description
-----------------------------------------|---------|------------
`locale`                                 | string  | Currently selected language (e.g. en_GB for English)
`create_subfolder_enabled`               | bool    | True if a subfolder should be created when adding a torrent
`start_paused_enabled`                   | bool    | True if torrents should be added in a Paused state
`auto_delete_mode`                       | integer | TODO
`preallocate_all`                        | bool    | True if disk space should be pre-allocated for all files
`incomplete_files_ext`                   | bool    | True if ".!qB" should be appended to incomplete files
`auto_tmm_enabled`                       | bool    | True if Automatic Torrent Management is enabled by default
`torrent_changed_tmm_enabled`            | bool    | True if torrent should be relocated when its Category changes
`save_path_changed_tmm_enabled`          | bool    | True if torrent should be relocated when the default save path changes
`category_changed_tmm_enabled`           | bool    | True if torrent should be relocated when its Category's save path changes
`save_path`                              | string  | Default save path for torrents, separated by slashes
`temp_path_enabled`                      | bool    | True if folder for incomplete torrents is enabled
`temp_path`                              | string  | Path for incomplete torrents, separated by slashes
`scan_dirs`                              | object  | Property: directory to watch for torrent files, value: where torrents loaded from this directory should be downloaded to (see list of possible values below). Slashes are used as path separators; multiple key/value pairs can be specified
`export_dir`                             | string  | Path to directory to copy .torrent files to. Slashes are used as path separators
`export_dir_fin`                         | string  | Path to directory to copy .torrent files of completed downloads to. Slashes are used as path separators
`mail_notification_enabled`              | bool    | True if e-mail notification should be enabled
`mail_notification_sender`               | string  | e-mail where notifications should originate from
`mail_notification_email`                | string  | e-mail to send notifications to
`mail_notification_smtp`                 | string  | smtp server for e-mail notifications
`mail_notification_ssl_enabled`          | bool    | True if smtp server requires SSL connection
`mail_notification_auth_enabled`         | bool    | True if smtp server requires authentication
`mail_notification_username`             | string  | Username for smtp authentication
`mail_notification_password`             | string  | Password for smtp authentication
`autorun_enabled`                        | bool    | True if external program should be run after torrent has finished downloading
`autorun_program`                        | string  | Program path/name/arguments to run if `autorun_enabled` is enabled; path is separated by slashes; you can use `%f` and `%n` arguments, which will be expanded by qBittorent as path_to_torrent_file and torrent_name (from the GUI; not the .torrent file name) respectively
`queueing_enabled`                       | bool    | True if torrent queuing is enabled
`max_active_downloads`                   | integer | Maximum number of active simultaneous downloads
`max_active_torrents`                    | integer | Maximum number of active simultaneous downloads and uploads
`max_active_uploads`                     | integer | Maximum number of active simultaneous uploads
`dont_count_slow_torrents`               | bool    | If true torrents w/o any activity (stalled ones) will not be counted towards `max_active_*` limits; see [dont_count_slow_torrents](https://www.libtorrent.org/reference-Settings.html#dont_count_slow_torrents) for more information
`slow_torrent_dl_rate_threshold`         | integer | Download rate in KiB/s for a torrent to be considered "slow"
`slow_torrent_ul_rate_threshold`         | integer | Upload rate in KiB/s for a torrent to be considered "slow"
`slow_torrent_inactive_timer`            | integer | Seconds a torrent should be inactive before considered "slow"
`max_ratio_enabled`                      | bool    | True if share ratio limit is enabled
`max_ratio`                              | float   | Get the global share ratio limit
`max_ratio_act`                          | integer | Action performed when a torrent reaches the maximum share ratio. See list of possible values here below.
`listen_port`                            | integer | Port for incoming connections
`upnp`                                   | bool    | True if UPnP/NAT-PMP is enabled
`random_port`                            | bool    | True if the port is randomly selected
`dl_limit`                               | integer | Global download speed limit in KiB/s; `-1` means no limit is applied
`up_limit`                               | integer | Global upload speed limit in KiB/s; `-1` means no limit is applied
`max_connec`                             | integer | Maximum global number of simultaneous connections
`max_connec_per_torrent`                 | integer | Maximum number of simultaneous connections per torrent
`max_uploads`                            | integer | Maximum number of upload slots
`max_uploads_per_torrent`                | integer | Maximum number of upload slots per torrent
`stop_tracker_timeout`                   | integer | Timeout in seconds for a `stopped` announce request to trackers
`enable_piece_extent_affinity`           | bool    | True if the advanced libtorrent option `piece_extent_affinity` is enabled
`bittorrent_protocol`                    | integer | Bittorrent Protocol to use (see list of possible values below)
`limit_utp_rate`                         | bool    | True if `[du]l_limit` should be applied to uTP connections; this option is only available in qBittorent built against libtorrent version 0.16.X and higher
`limit_tcp_overhead`                     | bool    | True if `[du]l_limit` should be applied to estimated TCP overhead (service data: e.g. packet headers)
`limit_lan_peers`                        | bool    | True if `[du]l_limit` should be applied to peers on the LAN
`alt_dl_limit`                           | integer | Alternative global download speed limit in KiB/s
`alt_up_limit`                           | integer | Alternative global upload speed limit in KiB/s
`scheduler_enabled`                      | bool    | True if alternative limits should be applied according to schedule
`schedule_from_hour`                     | integer | Scheduler starting hour
`schedule_from_min`                      | integer | Scheduler starting minute
`schedule_to_hour`                       | integer | Scheduler ending hour
`schedule_to_min`                        | integer | Scheduler ending minute
`scheduler_days`                         | integer | Scheduler days. See possible values here below
`dht`                                    | bool    | True if DHT is enabled
`pex`                                    | bool    | True if PeX is enabled
`lsd`                                    | bool    | True if LSD is enabled
`encryption`                             | integer | See list of possible values here below
`anonymous_mode`                         | bool    | If true anonymous mode will be enabled; read more [here](Anonymous-Mode); this option is only available in qBittorent built against libtorrent version 0.16.X and higher
`proxy_type`                             | integer | See list of possible values here below
`proxy_ip`                               | string  | Proxy IP address or domain name
`proxy_port`                             | integer | Proxy port
`proxy_peer_connections`                 | bool    | True if peer and web seed connections should be proxified; this option will have any effect only in qBittorent built against libtorrent version 0.16.X and higher
`proxy_auth_enabled`                     | bool    | True proxy requires authentication; doesn't apply to SOCKS4 proxies
`proxy_username`                         | string  | Username for proxy authentication
`proxy_password`                         | string  | Password for proxy authentication
`proxy_torrents_only`                    | bool    | True if proxy is only used for torrents
`ip_filter_enabled`                      | bool    | True if external IP filter should be enabled
`ip_filter_path`                         | string  | Path to IP filter file (.dat, .p2p, .p2b files are supported); path is separated by slashes
`ip_filter_trackers`                     | bool    | True if IP filters are applied to trackers
`web_ui_domain_list`                     | string  | Comma-separated list of domains to accept when performing Host header validation
`web_ui_address`                         | string  | IP address to use for the WebUI
`web_ui_port`                            | integer | WebUI port
`web_ui_upnp`                            | bool    | True if UPnP is used for the WebUI port
`web_ui_username`                        | string  | WebUI username
`web_ui_password`                        | string  | For API ≥ v2.3.0: Plaintext WebUI password, not readable, write-only. For API < v2.3.0: MD5 hash of WebUI password, hash is generated from the following string: `username:Web UI Access:plain_text_web_ui_password`
`web_ui_csrf_protection_enabled`         | bool    | True if WebUI CSRF protection is enabled
`web_ui_clickjacking_protection_enabled` | bool    | True if WebUI clickjacking protection is enabled
`web_ui_secure_cookie_enabled`           | bool    | True if WebUI cookie `Secure` flag is enabled
`web_ui_max_auth_fail_count`             | integer | Maximum number of authentication failures before WebUI access ban
`web_ui_ban_duration`                    | integer | WebUI access ban duration in seconds
`web_ui_session_timeout`                 | integer | Seconds until WebUI is automatically signed off
`web_ui_host_header_validation_enabled`  | bool    | True if WebUI host header validation is enabled
`bypass_local_auth`                      | bool    | True if authentication challenge for loopback address (127.0.0.1) should be disabled
`bypass_auth_subnet_whitelist_enabled`   | bool    | True if webui authentication should be bypassed for clients whose ip resides within (at least) one of the subnets on the whitelist
`bypass_auth_subnet_whitelist`           | string  | (White)list of ipv4/ipv6 subnets for which webui authentication should be bypassed; list entries are separated by commas
`alternative_webui_enabled`              | bool    | True if an alternative WebUI should be used
`alternative_webui_path`                 | string  | File path to the alternative WebUI
`use_https`                              | bool    | True if WebUI HTTPS access is enabled
`ssl_key`                                | string  | For API < v2.0.1: SSL keyfile contents (this is a not a path)
`ssl_cert`                               | string  | For API < v2.0.1: SSL certificate contents (this is a not a path)
`web_ui_https_key_path`                  | string  | For API ≥ v2.0.1: Path to SSL keyfile
`web_ui_https_cert_path`                 | string  | For API ≥ v2.0.1: Path to SSL certificate
`dyndns_enabled`                         | bool    | True if server DNS should be updated dynamically
`dyndns_service`                         | integer | See list of possible values here below
`dyndns_username`                        | string  | Username for DDNS service
`dyndns_password`                        | string  | Password for DDNS service
`dyndns_domain`                          | string  | Your DDNS domain name
`rss_refresh_interval`                   | integer | RSS refresh interval
`rss_max_articles_per_feed`              | integer | Max stored articles per RSS feed
`rss_processing_enabled`                 | bool    | Enable processing of RSS feeds
`rss_auto_downloading_enabled`           | bool    | Enable auto-downloading of torrents from the RSS feeds
`rss_download_repack_proper_episodes`    | bool    | For API ≥ v2.5.1: Enable downloading of repack/proper Episodes
`rss_smart_episode_filters`              | string  | For API ≥ v2.5.1: List of RSS Smart Episode Filters
`add_trackers_enabled`                   | bool    | Enable automatic adding of trackers to new torrents
`add_trackers`                           | string  | List of trackers to add to new torrent
`web_ui_use_custom_http_headers_enabled` | bool    | For API ≥ v2.5.1: Enable custom http headers
`web_ui_custom_http_headers`             | string  | For API ≥ v2.5.1: List of custom http headers
`max_seeding_time_enabled`               | bool    | True enables max seeding time 
`max_seeding_time`                       | integer | Number of minutes to seed a torrent
`announce_ip`                            | string  | TODO
`announce_to_all_tiers`                  | bool    | True always announce to all tiers
`announce_to_all_trackers`               | bool    | True always announce to all trackers in a tier
`async_io_threads`                       | integer | Number of asynchronous I/O threads
`banned_IPs`                             | string  | List of banned IPs
`checking_memory_use`                    | integer | Outstanding memory when checking torrents in MiB
`current_interface_address`              | string  | IP Address to bind to. Empty String means All addresses
`current_network_interface`              | string  | Network Interface used
`disk_cache`                             | integer | Disk cache used in MiB
`disk_cache_ttl`                         | integer | Disk cache expiry interval in seconds
`embedded_tracker_port`                  | integer | Port used for embedded tracker
`enable_coalesce_read_write`             | bool    | True enables coalesce reads & writes
`enable_embedded_tracker`                | bool    | True enables embedded tracker
`enable_multi_connections_from_same_ip`  | bool    | True allows multiple connections from the same IP address
`enable_os_cache`                        | bool    | True enables os cache
`enable_upload_suggestions`              | bool    | True enables sending of upload piece suggestions
`file_pool_size`                         | integer | File pool size
`outgoing_ports_max`                     | integer | Maximal outgoing port (0: Disabled)
`outgoing_ports_min`                     | integer | Minimal outgoing port (0: Disabled)
`recheck_completed_torrents`             | bool    | True rechecks torrents on completion
`resolve_peer_countries`                 | bool    | True resolves peer countries
`save_resume_data_interval`              | integer | Save resume data interval in min
`send_buffer_low_watermark`              | integer | Send buffer low watermark in KiB
`send_buffer_watermark`                  | integer | Send buffer watermark in KiB
`send_buffer_watermark_factor`           | integer | Send buffer watermark factor in percent
`socket_backlog_size`                    | integer | Socket backlog size
`upload_choking_algorithm`               | integer | Upload choking algorithm used (see list of possible values below)
`upload_slots_behavior`                  | integer | Upload slots behavior used (see list of possible values below)
`upnp_lease_duration`                    | integer | UPnP lease duration (0: Permanent lease)
`utp_tcp_mixed_mode`                     | integer | μTP-TCP mixed mode algorithm (see list of possible values below)

Possible values of `scan_dirs`:

Value                       | Description
----------------------------|------------
`0`                         | Download to the monitored folder
`1`                         | Download to the default save path
`"/path/to/download/to"`    | Download to this path

Possible values of `scheduler_days`:

Value  | Description
-------|------------
`0`    | Every day
`1`    | Every weekday
`2`    | Every weekend
`3`    | Every Monday
`4`    | Every Tuesday
`5`    | Every Wednesday
`6`    | Every Thursday
`7`    | Every Friday
`8`    | Every Saturday
`9`    | Every Sunday

Possible values of `encryption`:

Value  | Description
-------|------------
`0`    | Prefer encryption
`1`    | Force encryption on
`2`    | Force encryption off

NB: the first options allows you to use both encrypted and unencrypted connections (this is the default); other options are mutually exclusive: e.g. by forcing encryption on you won't be able to use unencrypted connections and vice versa.

Possible values of `proxy_type`:

Value | Description
------|------------
`-1`  | Proxy is disabled
`1`   | HTTP proxy without authentication
`2`   | SOCKS5 proxy without authentication
`3`   | HTTP proxy with authentication
`4`   | SOCKS5 proxy with authentication
`5`   | SOCKS4 proxy without authentication

Possible values of `dyndns_service`:

Value  | Description
-------|------------
`0`    | Use DyDNS
`1`    | Use NOIP

Possible values of `max_ratio_act`:

Value | Description
------|------------
`0`   | Pause torrent
`1`   | Remove torrent

Possible values of `bittorrent_protocol`:

Value  | Description
-------|------------
`0`    | TCP and μTP
`1`    | TCP
`2`    | μTP

Possible values of `upload_choking_algorithm`:

Value  | Description
-------|------------
`0`    | Round-robin
`1`    | Fastest upload
`2`    | Anti-leech

Possible values of `upload_slots_behavior`:

Value  | Description
-------|------------
`0`    | Fixed slots
`1`    | Upload rate based

Possible values of `utp_tcp_mixed_mode`:

Value  | Description
-------|------------
`0`    | Prefer TCP
`1`    | Peer proportional

Example:

```JSON
{
    "add_trackers": "",
    "add_trackers_enabled": false,
    "alt_dl_limit": 10240,
    "alt_up_limit": 10240,
    "alternative_webui_enabled": false,
    "alternative_webui_path": "/home/user/Documents/qbit-webui",
    "announce_ip": "",
    "announce_to_all_tiers": true,
    "announce_to_all_trackers": false,
    "anonymous_mode": false,
    "async_io_threads": 4,
    "auto_delete_mode": 0,
    "auto_tmm_enabled": false,
    "autorun_enabled": false,
    "autorun_program": "",
    "banned_IPs": "",
    "bittorrent_protocol": 0,
    "bypass_auth_subnet_whitelist": "",
    "bypass_auth_subnet_whitelist_enabled": false,
    "bypass_local_auth": false,
    "category_changed_tmm_enabled": false,
    "checking_memory_use": 32,
    "create_subfolder_enabled": true,
    "current_interface_address": "",
    "current_network_interface": "",
    "dht": true,
    "disk_cache": -1,
    "disk_cache_ttl": 60,
    "dl_limit": 0,
    "dont_count_slow_torrents": false,
    "dyndns_domain": "changeme.dyndns.org",
    "dyndns_enabled": false,
    "dyndns_password": "",
    "dyndns_service": 0,
    "dyndns_username": "",
    "embedded_tracker_port": 9000,
    "enable_coalesce_read_write": false,
    "enable_embedded_tracker": false,
    "enable_multi_connections_from_same_ip": false,
    "enable_os_cache": true,
    "enable_piece_extent_affinity": false,
    "enable_upload_suggestions": false,
    "encryption": 0,
    "export_dir": "/home/user/Downloads/all",
    "export_dir_fin": "/home/user/Downloads/completed",
    "file_pool_size": 40,
    "incomplete_files_ext": false,
    "ip_filter_enabled": false,
    "ip_filter_path": "",
    "ip_filter_trackers": false,
    "limit_lan_peers": true,
    "limit_tcp_overhead": false,
    "limit_utp_rate": true,
    "listen_port": 58925,
    "locale": "en",
    "lsd": true,
    "mail_notification_auth_enabled": false,
    "mail_notification_email": "",
    "mail_notification_enabled": false,
    "mail_notification_password": "",
    "mail_notification_sender": "qBittorrent_notification@example.com",
    "mail_notification_smtp": "smtp.changeme.com",
    "mail_notification_ssl_enabled": false,
    "mail_notification_username": "",
    "max_active_downloads": 3,
    "max_active_torrents": 5,
    "max_active_uploads": 3,
    "max_connec": 500,
    "max_connec_per_torrent": 100,
    "max_ratio": -1,
    "max_ratio_act": 0,
    "max_ratio_enabled": false,
    "max_seeding_time": -1,
    "max_seeding_time_enabled": false,
    "max_uploads": -1,
    "max_uploads_per_torrent": -1,
    "outgoing_ports_max": 0,
    "outgoing_ports_min": 0,
    "pex": true,
    "preallocate_all": false,
    "proxy_auth_enabled": false,
    "proxy_ip": "0.0.0.0",
    "proxy_password": "",
    "proxy_peer_connections": false,
    "proxy_port": 8080,
    "proxy_torrents_only": false,
    "proxy_type": 0,
    "proxy_username": "",
    "queueing_enabled": false,
    "random_port": false,
    "recheck_completed_torrents": false,
    "resolve_peer_countries": true,
    "rss_auto_downloading_enabled":true,
    "rss_download_repack_proper_episodes":true,
    "rss_max_articles_per_feed":50,
    "rss_processing_enabled":true,
    "rss_refresh_interval":30,
    "rss_smart_episode_filters":"s(\\d+)e(\\d+)\n(\\d+)x(\\d+)\n(\\d{4}[.\\-]\\d{1,2}[.\\-]\\d{1,2})",
    "save_path": "/home/user/Downloads/",
    "save_path_changed_tmm_enabled": false,
    "save_resume_data_interval": 60,
    "scan_dirs":
    {
        "/home/user/Downloads/incoming/games": 0,
        "/home/user/Downloads/incoming/movies": 1,
    },
    "schedule_from_hour": 8,
    "schedule_from_min": 0,
    "schedule_to_hour": 20,
    "schedule_to_min": 0,
    "scheduler_days": 0,
    "scheduler_enabled": false,
    "send_buffer_low_watermark": 10,
    "send_buffer_watermark": 500,
    "send_buffer_watermark_factor": 50,
    "slow_torrent_dl_rate_threshold": 2,
    "slow_torrent_inactive_timer": 60,
    "slow_torrent_ul_rate_threshold": 2,
    "socket_backlog_size": 30,
    "start_paused_enabled": false,
    "stop_tracker_timeout": 1,
    "temp_path": "/home/user/Downloads/temp",
    "temp_path_enabled": false,
    "torrent_changed_tmm_enabled": true,
    "up_limit": 0,
    "upload_choking_algorithm": 1,
    "upload_slots_behavior": 0,
    "upnp": true,
    "use_https": false,
    "utp_tcp_mixed_mode": 0,
    "web_ui_address": "*",
    "web_ui_ban_duration": 3600,
    "web_ui_clickjacking_protection_enabled": true,
    "web_ui_csrf_protection_enabled": true,
    "web_ui_custom_http_headers": "",
    "web_ui_domain_list": "*",
    "web_ui_host_header_validation_enabled": true,
    "web_ui_https_cert_path": "",
    "web_ui_https_key_path": "",
    "web_ui_max_auth_fail_count": 5,
    "web_ui_port": 8080,
    "web_ui_secure_cookie_enabled": true,
    "web_ui_session_timeout": 3600,
    "web_ui_upnp": false,
    "web_ui_use_custom_http_headers_enabled": false,
    "web_ui_username": "admin"
}
```

## Set application preferences ##

Name: `setPreferences`

**Parameters:**

A json object with key-value pairs of the settings you want to change and their new values.

Example:

```JSON
json={"save_path":"C:/Users/Dayman/Downloads","queueing_enabled":false,"scan_dirs":{"C:/Games": 0,"D:/Downloads": 1}}
```

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

**Notes**:

  1. There is no need to pass all possible preferences' `token:value` pairs if you only want to change one option
  1. Paths in `scan_dirs` must exist, otherwise this option will have no effect
  1. String values must be quoted; integer and boolean values must never be quoted

For a list of possible preference options see [Get application preferences](#get-application-preferences)

## Get default save path ##

Name: `defaultSavePath`

**Parameters:**

None

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

The response is a string with the default save path, e.g. `C:/Users/Dayman/Downloads`.

# Log #

All Log API methods are under "log", e.g.: `/api/v2/log/methodName`.

## Get log ##

Name: `main`

**Parameters:**

Parameter       | Type    | Description
----------------|---------|------------
`normal`        | bool    | Include normal messages (default: `true`)
`info`          | bool    | Include info messages (default: `true`)
`warning`       | bool    | Include warning messages (default: `true`)
`critical`      | bool    | Include critical messages (default: `true`)
`last_known_id` | integer | Exclude messages with "message id" <= `last_known_id` (default: `-1`)

Example:

```http
/api/v2/log/main?normal=true&info=true&warning=true&critical=true&last_known_id=-1
```

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios- see JSON below

The response is a JSON array in which each element is an entry of the log.

Each element of the array has the following properties:

Property    | Type    | Description
------------|---------|------------
`id`        | integer | ID of the message
`message`   | string  | Text of the message
`timestamp` | integer | Milliseconds since epoch
`type`      | integer | Type of the message: Log::NORMAL: `1`, Log::INFO: `2`, Log::WARNING: `4`, Log::CRITICAL: `8`

Example:

```JSON
[
    {
        "id":0,
        "message":"qBittorrent v3.4.0 started",
        "timestamp":1507969127860,
        "type":1
    },
    {
        "id":1,
        "message":"qBittorrent is trying to listen on any interface port: 19036",
        "timestamp":1507969127869,
        "type":2
    },
    {
        "id":2,
        "message":"Peer ID: -qB3400-",
        "timestamp":1507969127870,
        "type":1
    },
    {
        "id":3,
        "message":"HTTP User-Agent is 'qBittorrent/3.4.0'",
        "timestamp":1507969127870,
        "type":1
    },
    {
        "id":4,
        "message":"DHT support [ON]",
        "timestamp":1507969127871,
        "type":2
    },
    {
        "id":5,
        "message":"Local Peer Discovery support [ON]",
        "timestamp":1507969127871,
        "type":2
    },
    {
        "id":6,
        "message":"PeX support [ON]",
        "timestamp":1507969127871,
        "type":2
    },
    {
        "id":7,
        "message":"Anonymous mode [OFF]",
        "timestamp":1507969127871,
        "type":2
    },
    {
        "id":8,
        "message":"Encryption support [ON]",
        "timestamp":1507969127871,
        "type":2
    },
    {
        "id":9,
        "message":"Embedded Tracker [OFF]",
        "timestamp":1507969127871,
        "type":2
    },
    {
        "id":10,
        "message":"UPnP / NAT-PMP support [ON]",
        "timestamp":1507969127873,
        "type":2
    },
    {
        "id":11,
        "message":"Web UI: Now listening on port 8080",
        "timestamp":1507969127883,
        "type":1
    },
    {
        "id":12,
        "message":"Options were saved successfully.",
        "timestamp":1507969128055,
        "type":1
    },
    {
        "id":13,
        "message":"qBittorrent is successfully listening on interface :: port: TCP/19036",
        "timestamp":1507969128270,
        "type":2
    },
    {
        "id":14,
        "message":"qBittorrent is successfully listening on interface 0.0.0.0 port: TCP/19036",
        "timestamp":1507969128271,
        "type":2
    },
    {
        "id":15,
        "message":"qBittorrent is successfully listening on interface 0.0.0.0 port: UDP/19036",
        "timestamp":1507969128272,
        "type":2
    }
]
```

## Get peer log ##

Name: `peers`

**Parameters:**

Parameter       | Type    | Description
----------------|---------|------------
`last_known_id` | integer | Exclude messages with "message id" <= `last_known_id` (default: `-1`)

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios- see JSON below

The response a JSON array. Each element of the array of objects (each object is the information relative to a peer) containing the following fields

Property    | Type    | Description
------------|---------|------------
`id`        | integer | ID of the peer
`ip`        | string  | IP of the peer
`timestamp` | integer | Milliseconds since epoch
`blocked`   | boolean | Whether or not the peer was blocked
`reason`    | string  | Reason of the block

# Sync #

Sync API implements requests for obtaining changes since the last request.
All Sync API methods are under "sync", e.g.: `/api/v2/sync/methodName`.

## Get main data ##

Name: `maindata`

**Parameters:**

Parameter | Type    | Description
----------|---------|------------
`rid`     | integer | Response ID. If not provided, `rid=0` will be assumed. If the given `rid` is different from the one of last server reply, `full_update` will be `true` (see the server reply details for more info)

Example:

```http
/api/v2/sync/maindata?rid=14
```

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios- see JSON below

The response is a JSON object with the following possible fields

Property                      | Type    | Description
------------------------------|---------|------------
`rid`                         | integer | Response ID
`full_update`                 | bool    | Whether the response contains all the data or partial data
`torrents`                    | object  | Property: torrent hash, value: same as [torrent list](#get-torrent-list)
`torrents_removed`            | array   | List of hashes of torrents removed since last request
`categories`                  | object  | Info for categories added since last request
`categories_removed`          | array   | List of categories removed since last request
`tags`                        | array   | List of tags added since last request
`tags_removed`                | array   | List of tags removed since last request
`server_state`                | object  | Global transfer info

Example:

```JSON
{
    "rid":15,
    "torrents":
    {
        "8c212779b4abde7c6bc608063a0d008b7e40ce32":
        {
            "state":"pausedUP"
        }
    }
}
```

## Get torrent peers data ##

Name: `torrentPeers`

**Parameters:**

Parameter | Type    | Description
----------|---------|------------
`hash`    | string  | Torrent hash
`rid`     | integer | Response ID. If not provided, `rid=0` will be assumed. If the given `rid` is different from the one of last server reply, `full_update` will be `true` (see the server reply details for more info)

Example:

```http
/api/v2/sync/torrentPeers?hash=8c212779b4abde7c6bc608063a0d008b7e40ce32?rid=14
```

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
404                               | Torrent hash was not found
200                               | All other scenarios- see JSON below

The response is TODO

# Transfer info #

All Transfer info API methods are under "transfer", e.g.: `/api/v2/transfer/methodName`.

## Get global transfer info ##

This method returns info you usually see in qBt status bar.

Name: `info`

**Parameters:**

None

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios- see JSON below

The response is a JSON object with the following fields

Property            | Type    | Description
--------------------|---------|------------
`dl_info_speed`     | integer | Global download rate (bytes/s)
`dl_info_data`      | integer | Data downloaded this session (bytes)
`up_info_speed`     | integer | Global upload rate (bytes/s)
`up_info_data`      | integer | Data uploaded this session (bytes)
`dl_rate_limit`     | integer | Download rate limit (bytes/s)
`up_rate_limit`     | integer | Upload rate limit (bytes/s)
`dht_nodes`         | integer | DHT nodes connected to
`connection_status` | string  | Connection status. See possible values here below

In addition to the above in partial data requests (see [Get partial data](#get-partial-data) for more info):

Property               | Type    | Description
-----------------------|---------|------------
`queueing`             | bool    | True if torrent queueing is enabled
`use_alt_speed_limits` | bool    | True if alternative speed limits are enabled
`refresh_interval`     | integer | Transfer list refresh interval (milliseconds)

Possible values of `connection_status`:

Value               |
--------------------|
`connected`         |
`firewalled`        |
`disconnected`      |

Example:

```JSON
{
    "connection_status":"connected",
    "dht_nodes":386,
    "dl_info_data":681521119,
    "dl_info_speed":0,
    "dl_rate_limit":0,
    "up_info_data":10747904,
    "up_info_speed":0,
    "up_rate_limit":1048576
}
```

## Get alternative speed limits state ##

Name: `speedLimitsMode`

**Parameters:**

None

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

The response is `1` if alternative speed limits are enabled, `0` otherwise.

## Toggle alternative speed limits ##

Name: `toggleSpeedLimitsMode`

**Parameters:**

None

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

## Get global download limit ##

Name: `downloadLimit`

**Parameters:**

None

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

The response is the value of current global download speed limit in bytes/second; this value will be zero if no limit is applied.

## Set global download limit ##

Name: `setDownloadLimit`

**Parameters:**

Parameter                         | Type    | Description
----------------------------------|---------|------------
`limit`                           | integer | The global download speed limit to set in bytes/second

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

## Get global upload limit ##

Name: `uploadLimit`

**Parameters:**

None

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

The response is the value of current global upload speed limit in bytes/second; this value will be zero if no limit is applied.

## Set global upload limit ##

Name: `setUploadLimit`

**Parameters:**

Parameter                         | Type    | Description
----------------------------------|---------|------------
`limit`                           | integer | The global upload speed limit to set in bytes/second

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

## Ban peers ##

Name: `banPeers`

**Parameters:**

Parameter                         | Type    | Description
----------------------------------|---------|------------
`peers`                           | string  | The peer to ban, or multiple peers separated by a pipe `\|`. Each peer is a colon-separated `host:port`

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

# Torrent management #

All Torrent management API methods are under "torrents", e.g.: `/api/v2/torrents/methodName`.

## Get torrent list ##

Name: `info`

**Parameters:**

Parameter             | Type    | Description
----------------------|---------|------------
`filter`  _optional_  | string  | Filter torrent list by state. Allowed state filters: `all`, `downloading`, `seeding`, `completed`, `paused`, `active`, `inactive`, `resumed`, `stalled`, `stalled_uploading`, `stalled_downloading`, `errored`
`category` _optional_ | string  | Get torrents with the given category (empty string means "without category"; no "category" parameter means "any category" <- broken until [#11748](https://github.com/qbittorrent/qBittorrent/issues/11748) is resolved). Remember to URL-encode the category name. For example, `My category` becomes `My%20category`
`tag` _optional_ <sup>since 2.8.3</sup> | string  | Get torrents with the given tag (empty string means "without tag"; no "tag" parameter means "any tag". Remember to URL-encode the category name. For example, `My tag` becomes `My%20tag`
`sort` _optional_     | string  | Sort torrents by given key. They can be sorted using any field of the response's JSON array (which are documented below) as the sort key.
`reverse` _optional_  | bool    | Enable reverse sorting. Defaults to `false`
`limit` _optional_    | integer | Limit the number of torrents returned
`offset` _optional_   | integer | Set offset (if less than 0, offset from end)
`hashes` _optional_   | string  | Filter by hashes. Can contain multiple hashes separated by `\|`

Example:

```http
/api/v2/torrents/info?filter=downloading&category=sample%20category&sort=ratio
```

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios- see JSON below

The response is a JSON array with the following fields

Property             | Type    | Description
---------------------|---------|------------
`added_on`           | integer | Time (Unix Epoch) when the torrent was added to the client
`amount_left`        | integer | Amount of data left to download (bytes)
`auto_tmm`           | bool    | Whether this torrent is managed by Automatic Torrent Management
`availability`       | float   | Percentage of file pieces currently available
`category`           | string  | Category of the torrent
`completed`          | integer | Amount of transfer data completed (bytes)
`completion_on`      | integer | Time (Unix Epoch) when the torrent completed
`content_path`       | string  | Absolute path of torrent content (root path for multifile torrents, absolute file path for singlefile torrents)
`dl_limit`           | integer | Torrent download speed limit (bytes/s). `-1` if ulimited.
`dlspeed`            | integer | Torrent download speed (bytes/s)
`downloaded`         | integer | Amount of data downloaded
`downloaded_session` | integer | Amount of data downloaded this session
`eta`                | integer | Torrent ETA (seconds)
`f_l_piece_prio`     | bool    | True if first last piece are prioritized
`force_start`        | bool    | True if force start is enabled for this torrent
`hash`               | string  | Torrent hash
`last_activity`      | integer | Last time (Unix Epoch) when a chunk was downloaded/uploaded
`magnet_uri`         | string  | Magnet URI corresponding to this torrent
`max_ratio`          | float   | Maximum share ratio until torrent is stopped from seeding/uploading
`max_seeding_time`   | integer | Maximum seeding time (seconds) until torrent is stopped from seeding
`name`               | string  | Torrent name
`num_complete`       | integer | Number of seeds in the swarm
`num_incomplete`     | integer | Number of leechers in the swarm
`num_leechs`         | integer | Number of leechers connected to
`num_seeds`          | integer | Number of seeds connected to
`priority`           | integer | Torrent priority. Returns -1 if queuing is disabled or torrent is in seed mode
`progress`           | float   | Torrent progress (percentage/100)
`ratio`              | float   | Torrent share ratio. Max ratio value: 9999.
`ratio_limit`        | float   | TODO (what is different from `max_ratio`?)
`save_path`          | string  | Path where this torrent's data is stored
`seeding_time`       | integer | Torrent elapsed time while complete (seconds)
`seeding_time_limit` | integer | TODO (what is different from `max_seeding_time`?) seeding_time_limit is a per torrent setting, when Automatic Torrent Management is disabled, furthermore then max_seeding_time is set to seeding_time_limit for this torrent. If Automatic Torrent Management is enabled, the value is -2. And if max_seeding_time is unset it have a default value -1.
`seen_complete`      | integer | Time (Unix Epoch) when this torrent was last seen complete
`seq_dl`             | bool    | True if sequential download is enabled
`size`               | integer | Total size (bytes) of files selected for download
`state`              | string  | Torrent state. See table here below for the possible values
`super_seeding`      | bool    | True if super seeding is enabled
`tags`               | string  | Comma-concatenated tag list of the torrent
`time_active`        | integer | Total active time (seconds)
`total_size`         | integer | Total size (bytes) of all file in this torrent (including unselected ones)
`tracker`            | string  | The first tracker with working status. Returns empty string if no tracker is working.
`up_limit`           | integer | Torrent upload speed limit (bytes/s). `-1` if ulimited.
`uploaded`           | integer | Amount of data uploaded
`uploaded_session`   | integer | Amount of data uploaded this session
`upspeed`            | integer | Torrent upload speed (bytes/s)

Possible values of `state`:

Value         | Description
--------------|------------
`error`       | Some error occurred, applies to paused torrents
`missingFiles`| Torrent data files is missing
`uploading`   | Torrent is being seeded and data is being transferred
`pausedUP`    | Torrent is paused and has finished downloading
`queuedUP`    | Queuing is enabled and torrent is queued for upload
`stalledUP`   | Torrent is being seeded, but no connection were made
`checkingUP`  | Torrent has finished downloading and is being checked
`forcedUP`    | Torrent is forced to uploading and ignore queue limit
`allocating`  | Torrent is allocating disk space for download
`downloading` | Torrent is being downloaded and data is being transferred
`metaDL`      | Torrent has just started downloading and is fetching metadata
`pausedDL`    | Torrent is paused and has NOT finished downloading
`queuedDL`    | Queuing is enabled and torrent is queued for download
`stalledDL`   | Torrent is being downloaded, but no connection were made
`checkingDL`  | Same as checkingUP, but torrent has NOT finished downloading
`forcedDL`    | Torrent is forced to downloading to ignore queue limit
`checkingResumeData`| Checking resume data on qBt startup
`moving`      | Torrent is moving to another location
`unknown`     | Unknown status

Example:

```JSON
[
    {
        "dlspeed":9681262,
        "eta":87,
        "f_l_piece_prio":false,
        "force_start":false,
        "hash":"8c212779b4abde7c6bc608063a0d008b7e40ce32",
        "category":"",
        "tags": "",
        "name":"debian-8.1.0-amd64-CD-1.iso",
        "num_complete":-1,
        "num_incomplete":-1,
        "num_leechs":2,
        "num_seeds":54,
        "priority":1,
        "progress":0.16108787059783936,
        "ratio":0,
        "seq_dl":false,
        "size":657457152,
        "state":"downloading",
        "super_seeding":false,
        "upspeed":0
    },
    {
        another_torrent_info
    }
]
```

## Get torrent generic properties ##

Requires knowing the torrent hash. You can get it from [torrent list](#get-torrent-list).

Name: `properties`

**Parameters:**

Parameter | Type   | Description
----------|--------|------------
`hash`    | string | The hash of the torrent you want to get the generic properties of

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
404                               | Torrent hash was not found
200                               | All other scenarios- see JSON below

The response is:

- empty, if the torrent hash is invalid
- otherwise, a JSON object with the following fields

Property                  | Type    | Description
--------------------------|---------|------------
`save_path`               | string  | Torrent save path
`creation_date`           | integer | Torrent creation date (Unix timestamp)
`piece_size`              | integer | Torrent piece size (bytes)
`comment`                 | string  | Torrent comment
`total_wasted`            | integer | Total data wasted for torrent (bytes)
`total_uploaded`          | integer | Total data uploaded for torrent (bytes)
`total_uploaded_session`  | integer | Total data uploaded this session (bytes)
`total_downloaded`        | integer | Total data downloaded for torrent (bytes)
`total_downloaded_session`| integer | Total data downloaded this session (bytes)
`up_limit`                | integer | Torrent upload limit (bytes/s)
`dl_limit`                | integer | Torrent download limit (bytes/s)
`time_elapsed`            | integer | Torrent elapsed time (seconds)
`seeding_time`            | integer | Torrent elapsed time while complete (seconds)
`nb_connections`          | integer | Torrent connection count
`nb_connections_limit`    | integer | Torrent connection count limit
`share_ratio`             | float   | Torrent share ratio
`addition_date`           | integer | When this torrent was added (unix timestamp)
`completion_date`         | integer | Torrent completion date (unix timestamp)
`created_by`              | string  | Torrent creator
`dl_speed_avg`            | integer | Torrent average download speed (bytes/second)
`dl_speed`                | integer | Torrent download speed (bytes/second)
`eta`                     | integer | Torrent ETA (seconds)
`last_seen`               | integer | Last seen complete date (unix timestamp)
`peers`                   | integer | Number of peers connected to
`peers_total`             | integer | Number of peers in the swarm
`pieces_have`             | integer | Number of pieces owned
`pieces_num`              | integer | Number of pieces of the torrent
`reannounce`              | integer | Number of seconds until the next announce
`seeds`                   | integer | Number of seeds connected to
`seeds_total`             | integer | Number of seeds in the swarm
`total_size`              | integer | Torrent total size (bytes)
`up_speed_avg`            | integer | Torrent average upload speed (bytes/second)
`up_speed`                | integer | Torrent upload speed (bytes/second)

NB: `-1` is returned if the type of the property is integer but its value is not known.

Example:

```JSON
{
    "addition_date":1438429165,
    "comment":"\"Debian CD from cdimage.debian.org\"",
    "completion_date":1438429234,
    "created_by":"",
    "creation_date":1433605214,
    "dl_limit":-1,
    "dl_speed":0,
    "dl_speed_avg":9736015,
    "eta":8640000,
    "last_seen":1438430354,
    "nb_connections":3,
    "nb_connections_limit":250,
    "peers":1,
    "peers_total":89,
    "piece_size":524288,
    "pieces_have":1254,
    "pieces_num":1254,
    "reannounce":672,
    "save_path":"/Downloads/debian-8.1.0-amd64-CD-1.iso",
    "seeding_time":1128,
    "seeds":1,
    "seeds_total":254,
    "share_ratio":0.00072121022562178299,
    "time_elapsed":1197,
    "total_downloaded":681521119,
    "total_downloaded_session":681521119,
    "total_size":657457152,
    "total_uploaded":491520,
    "total_uploaded_session":491520,
    "total_wasted":23481724,
    "up_limit":-1,
    "up_speed":0,
    "up_speed_avg":410
}
```

## Get torrent trackers ##

Requires knowing the torrent hash. You can get it from [torrent list](#get-torrent-list).

Name: `trackers`

**Parameters:**

Parameter | Type   | Description
----------|--------|------------
`hash`    | string | The hash of the torrent you want to get the trackers of

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
404                               | Torrent hash was not found
200                               | All other scenarios- see JSON below

The response is a JSON array, where each element contains info about one tracker, with the following fields

Property         | Type     | Description
-----------------|----------|-------------
`url`            | string   | Tracker url
`status`         | integer  | Tracker status. See the table below for possible values
`tier`           | integer  | Tracker priority tier. Lower tier trackers are tried before higher tiers. Tier numbers are valid when `>= 0`, `< 0` is used as placeholder when `tier` does not exist for special entries (such as DHT).
`num_peers`      | integer  | Number of peers for current torrent, as reported by the tracker
`num_seeds`      | integer  | Number of seeds for current torrent, asreported by the tracker
`num_leeches`    | integer  | Number of leeches for current torrent, as reported by the tracker
`num_downloaded` | integer  | Number of completed downlods for current torrent, as reported by the tracker
`msg`            | string   | Tracker message (there is no way of knowing what this message is - it's up to tracker admins)

Possible values of `status`:

Value  | Description
-------|------------
0      | Tracker is disabled (used for DHT, PeX, and LSD)
1      | Tracker has not been contacted yet
2      | Tracker has been contacted and is working
3      | Tracker is updating
4      | Tracker has been contacted, but it is not working (or doesn't send proper replies)

Example:

```JSON
[
    {
        "msg":"",
        "num_peers":100,
        "status":2,
        "url":"http://bttracker.debian.org:6969/announce"
    },
    {
        another_tracker_info
    }
]
```

## Get torrent web seeds ##

Requires knowing the torrent hash. You can get it from [torrent list](#get-torrent-list).

Name: `webseeds`

**Parameters:**

Parameter | Type   | Description
----------|--------|------------
`hash`    | string | The hash of the torrent you want to get the webseeds of

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
404                               | Torrent hash was not found
200                               | All other scenarios- see JSON below

The response is a JSON array, where each element is information about one webseed, with the following fields

Property      | Type     | Description
--------------|----------|------------
`url`         | string   | URL of the web seed

Example:

```JSON
[
    {
        "url":"http://some_url/"
    },
    {
        "url":"http://some_other_url/"
    }
]
```

## Get torrent contents ##

Requires knowing the torrent hash. You can get it from [torrent list](#get-torrent-list).

Name: `files`

**Parameters:**

Parameter | Type   | Description
----------|--------|------------
`hash`    | string | The hash of the torrent you want to get the contents of
`indexes` _optional_ <sup>since 2.8.2</sup> | string | The indexes of the files you want to retrieve. `indexes` can contain multiple values separated by `\|`.

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
404                               | Torrent hash was not found
200                               | All other scenarios- see JSON below

The response is:

- empty, if the torrent hash is invalid
- otherwise, a JSON array, where each element contains info about one file, with the following fields

Property       | Type          | Description
---------------|---------------|-------------
`index` <sup>since 2.8.2</sup> | integer | File index
`name`         | string        | File name (including relative path)
`size`         | integer       | File size (bytes)
`progress`     | float         | File progress (percentage/100)
`priority`     | integer       | File priority. See possible values here below
`is_seed`      | bool          | True if file is seeding/complete
`piece_range`  | integer array | The first number is the starting piece index and the second number is the ending piece index (inclusive)
`availability` | float         | Percentage of file pieces currently available (percentage/100)

Possible values of `priority`:

Value      | Description
-----------|------------
`0`        | Do not download
`1`        | Normal priority
`6`        | High priority
`7`        | Maximal priority

Example:

```JSON

[
    {
        "index":0,
        "is_seed":false,
        "name":"debian-8.1.0-amd64-CD-1.iso",
        "piece_range":[0,1253],
        "priority":1,
        "progress":0,
        "size":657457152,
        "availability":0.5,
    }
]
```

## Get torrent pieces' states ##

Requires knowing the torrent hash. You can get it from [torrent list](#get-torrent-list).

Name: `pieceStates`

**Parameters:**

Parameter | Type   | Description
----------|--------|------------
`hash`    | string | The hash of the torrent you want to get the pieces' states of

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
404                               | Torrent hash was not found
200                               | All other scenarios- see JSON below

The response is:

- empty, if the torrent hash is invalid
- otherwise, an array of states (integers) of all pieces (in order) of a specific torrent.

Value meanings are defined as below:

Value      | Description
-----------|------------
`0`        | Not downloaded yet
`1`        | Now downloading
`2`        | Already downloaded

Example:

```JSON
[0,0,2,1,0,0,2,1]
```

## Get torrent pieces' hashes ##

Requires knowing the torrent hash. You can get it from [torrent list](#get-torrent-list).

Name: `pieceHashes`

**Parameters:**

Parameter | Type   | Description
----------|--------|------------
`hash`    | string | The hash of the torrent you want to get the pieces' hashes of

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
404                               | Torrent hash was not found
200                               | All other scenarios- see JSON below

The response is:

- empty, if the torrent hash is invalid
- otherwise, an array of hashes (strings) of all pieces (in order) of a specific torrent.

Example:

```JSON
["54eddd830a5b58480a6143d616a97e3a6c23c439","f8a99d225aa4241db100f88407fc3bdaead583ab","928fb615b9bd4dd8f9e9022552c8f8f37ef76f58"]
```

## Pause torrents ##

Requires knowing the torrent hashes. You can get it from [torrent list](#get-torrent-list).

Name: `pause`

**Parameters:**

Parameter   | Type     | Description
------------|----------|------------
`hashes`    | string   | The hashes of the torrents you want to pause. `hashes` can contain multiple hashes separated by `\|`, to pause multiple torrents, or set to `all`, to pause all torrents.

Example:

```http
/api/v2/torrents/pause?hashes=8c212779b4abde7c6bc608063a0d008b7e40ce32|54eddd830a5b58480a6143d616a97e3a6c23c439
```

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

## Resume torrents ##

Requires knowing the torrent hashes. You can get it from [torrent list](#get-torrent-list).

Name: `resume`

**Parameters:**

Parameter   | Type     | Description
------------|----------|------------
`hashes`    | string   | The hashes of the torrents you want to resume. `hashes` can contain multiple hashes separated by `\|`, to resume multiple torrents, or set to `all`, to resume all torrents.

Example:

```http
/api/v2/torrents/resume?hashes=8c212779b4abde7c6bc608063a0d008b7e40ce32|54eddd830a5b58480a6143d616a97e3a6c23c439
```

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

## Delete torrents ##

Requires knowing the torrent hashes. You can get it from [torrent list](#get-torrent-list).

Name: `delete`

**Parameters:**

Parameter   | Type     | Description
------------|----------|------------
`hashes`    | string   | The hashes of the torrents you want to delete. `hashes` can contain multiple hashes separated by `\|`, to delete multiple torrents, or set to `all`, to delete all torrents.
`deleteFiles` | If set to `true`, the downloaded data will also be deleted, otherwise has no effect.

Example:

```http
/api/v2/torrents/delete?hashes=8c212779b4abde7c6bc608063a0d008b7e40ce32&deleteFiles=false
```

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

## Recheck torrents ##

Requires knowing the torrent hashes. You can get it from [torrent list](#get-torrent-list).

Name: `recheck`

**Parameters:**

Parameter   | Type     | Description
------------|----------|------------
`hashes`    | string   | The hashes of the torrents you want to recheck. `hashes` can contain multiple hashes separated by `\|`, to recheck multiple torrents, or set to `all`, to recheck all torrents.

Example:

```http
/api/v2/torrents/recheck?hashes=8c212779b4abde7c6bc608063a0d008b7e40ce32|54eddd830a5b58480a6143d616a97e3a6c23c439
```

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

## Reannounce torrents ##

Requires knowing the torrent hashes. You can get it from [torrent list](#get-torrent-list).

Name: `reannounce`

**Parameters:**

Parameter   | Type     | Description
------------|----------|------------
`hashes`    | string   | The hashes of the torrents you want to reannounce. `hashes` can contain multiple hashes separated by `\|`, to reannounce multiple torrents, or set to `all`, to reannounce all torrents.

Example:

```http
/api/v2/torrents/reannounce?hashes=8c212779b4abde7c6bc608063a0d008b7e40ce32|54eddd830a5b58480a6143d616a97e3a6c23c439
```

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

## Add new torrent ##

This method can add torrents from  server local file or from URLs. `http://`, `https://`, `magnet:` and `bc://bt/` links are supported.

Add torrent from URLs example:

```http
POST /api/v2/torrents/add HTTP/1.1
User-Agent: Fiddler
Host: 127.0.0.1
Cookie: SID=your_sid
Content-Type: multipart/form-data; boundary=---------------------------6688794727912
Content-Length: length

-----------------------------6688794727912
Content-Disposition: form-data; name="urls"

https://torcache.net/torrent/3B1A1469C180F447B77021074DBBCCAEF62611E7.torrent
https://torcache.net/torrent/3B1A1469C180F447B77021074DBBCCAEF62611E8.torrent
-----------------------------6688794727912
Content-Disposition: form-data; name="savepath"

C:/Users/qBit/Downloads
-----------------------------6688794727912
Content-Disposition: form-data; name="cookie"

ui=28979218048197
-----------------------------6688794727912
Content-Disposition: form-data; name="category"

movies
-----------------------------6688794727912
Content-Disposition: form-data; name="skip_checking"

true
-----------------------------6688794727912
Content-Disposition: form-data; name="paused"

true
-----------------------------6688794727912
Content-Disposition: form-data; name="root_folder"

true
-----------------------------6688794727912--
```

Add torrents from files example:

```http
POST /api/v2/torrents/add HTTP/1.1
Content-Type: multipart/form-data; boundary=-------------------------acebdf13572468
User-Agent: Fiddler
Host: 127.0.0.1
Cookie: SID=your_sid
Content-Length: length

---------------------------acebdf13572468
Content-Disposition: form-data; name="torrents"; filename="8f18036b7a205c9347cb84a253975e12f7adddf2.torrent"
Content-Type: application/x-bittorrent

file_binary_data_goes_here
---------------------------acebdf13572468
Content-Disposition: form-data; name="torrents"; filename="UFS.torrent"
Content-Type: application/x-bittorrent

file_binary_data_goes_here
---------------------------acebdf13572468--

```

The above example will add two torrent files. `file_binary_data_goes_here` represents raw data of torrent file (basically a byte array).

Property                        | Type    | Description
--------------------------------|---------|------------
`urls`                          | string  | URLs separated with newlines
`torrents`                      | raw     | Raw data of torrent file. `torrents` can be presented multiple times.
`savepath` _optional_           | string  | Download folder
`cookie` _optional_             | string  | Cookie sent to download the .torrent file
`category` _optional_           | string  | Category for the torrent
`tags` _optional_               | string  | Tags for the torrent, split by ','
`skip_checking` _optional_      | string  | Skip hash checking. Possible values are `true`, `false` (default)
`paused` _optional_             | string  | Add torrents in the paused state. Possible values are `true`, `false` (default)
`root_folder` _optional_        | string  | Create the root folder. Possible values are `true`, `false`, unset (default)
`rename` _optional_             | string  | Rename torrent
`upLimit` _optional_            | integer | Set torrent upload speed limit. Unit in bytes/second
`dlLimit` _optional_            | integer | Set torrent download speed limit. Unit in bytes/second
`ratioLimit` _optional_ <sup>since 2.8.1</sup> | float   | Set torrent share ratio limit
`seedingTimeLimit` _optional_ <sup>since 2.8.1</sup>  | integer | Set torrent seeding time limit. Unit in minutes
`autoTMM` _optional_         | bool    | Whether Automatic Torrent Management should be used
`sequentialDownload` _optional_ | string  | Enable sequential download. Possible values are `true`, `false` (default)
`firstLastPiecePrio` _optional_ | string  | Prioritize download first last piece. Possible values are `true`, `false` (default)

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
415                               | Torrent file is not valid
200                               | All other scenarios

## Add trackers to torrent ##

Requires knowing the torrent hash. You can get it from [torrent list](#get-torrent-list).

```http
POST /api/v2/torrents/addTrackers HTTP/1.1
User-Agent: Fiddler
Host: 127.0.0.1
Cookie: SID=your_sid
Content-Type: application/x-www-form-urlencoded
Content-Length: length

hash=8c212779b4abde7c6bc608063a0d008b7e40ce32&urls=http://192.168.0.1/announce%0Audp://192.168.0.1:3333/dummyAnnounce
```

This adds two trackers to torrent with hash `8c212779b4abde7c6bc608063a0d008b7e40ce32`. Note `%0A` (aka LF newline) between trackers. Ampersand in tracker urls **MUST** be escaped.

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
404                               | Torrent hash was not found
200                               | All other scenarios

## Edit trackers ##

Name: `editTracker`

**Parameters:**

Parameter                         | Type    | Description
----------------------------------|---------|------------
`hash`                            | string  | The hash of the torrent
`origUrl`                         | string  | The tracker URL you want to edit
`newUrl`                          | string  | The new URL to replace the `origUrl`

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
400                               | `newUrl` is not a valid URL
404                               | Torrent hash was not found
409                               | `newUrl` already exists for the torrent
409                               | `origUrl` was not found
200                               | All other scenarios

## Remove trackers ##

Name: `removeTrackers`

**Parameters:**

Parameter                         | Type    | Description
----------------------------------|---------|------------
`hash`                            | string  | The hash of the torrent
`urls`                            | string  | URLs to remove, separated by `\|`

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
404                               | Torrent hash was not found
409                               | All `urls` were not found
200                               | All other scenarios

## Add peers ##

Name: `addPeers`

**Parameters:**

Parameter                         | Type    | Description
----------------------------------|---------|------------
`hashes`                          | string  | The hash of the torrent, or multiple hashes separated by a pipe `\|`
`peers`                           | string  | The peer to add, or multiple peers separated by a pipe `\|`. Each peer is a colon-separated `host:port`

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
400                               | None of the supplied peers are valid
200                               | All other scenarios

## Increase torrent priority ##

Requires knowing the torrent hash. You can get it from [torrent list](#get-torrent-list).

Name: `increasePrio`

**Parameters:**

Parameter   | Type     | Description
------------|----------|------------
`hashes`    | string   | The hashes of the torrents you want to increase the priority of. `hashes` can contain multiple hashes separated by `\|`, to increase the priority of multiple torrents, or set to `all`, to increase the priority of all torrents.

Example:

```http
/api/v2/torrents/increasePrio?hashes=8c212779b4abde7c6bc608063a0d008b7e40ce32|54eddd830a5b58480a6143d616a97e3a6c23c439
```

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
409                               | Torrent queueing is not enabled
200                               | All other scenarios

## Decrease torrent priority ##

Requires knowing the torrent hash. You can get it from [torrent list](#get-torrent-list).

Name: `decreasePrio`

**Parameters:**

Parameter   | Type     | Description
------------|----------|------------
`hashes`    | string   | The hashes of the torrents you want to decrease the priority of. `hashes` can contain multiple hashes separated by `\|`, to decrease the priority of multiple torrents, or set to `all`, to decrease the priority of all torrents.

Example:

```http
/api/v2/torrents/decreasePrio?hashes=8c212779b4abde7c6bc608063a0d008b7e40ce32|54eddd830a5b58480a6143d616a97e3a6c23c439
```

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
409                               | Torrent queueing is not enabled
200                               | All other scenarios

## Maximal torrent priority ##

Requires knowing the torrent hash. You can get it from [torrent list](#get-torrent-list).

Name: `topPrio`

**Parameters:**

Parameter   | Type     | Description
------------|----------|------------
`hashes`    | string   | The hashes of the torrents you want to set to the maximum priority. `hashes` can contain multiple hashes separated by `\|`, to set multiple torrents to the maximum priority, or set to `all`, to set all torrents to the maximum priority.

Example:

```http
/api/v2/torrents/topPrio?hashes=8c212779b4abde7c6bc608063a0d008b7e40ce32|54eddd830a5b58480a6143d616a97e3a6c23c439
```

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
409                               | Torrent queueing is not enabled
200                               | All other scenarios

## Minimal torrent priority ##

Requires knowing the torrent hash. You can get it from [torrent list](#get-torrent-list).

Name: `bottomPrio`

**Parameters:**

Parameter   | Type     | Description
------------|----------|------------
`hashes`    | string   | The hashes of the torrents you want to set to the minimum priority. `hashes` can contain multiple hashes separated by `\|`, to set multiple torrents to the minimum priority, or set to `all`, to set all torrents to the minimum priority.

Example:

```http
/api/v2/torrents/bottomPrio?hashes=8c212779b4abde7c6bc608063a0d008b7e40ce32|54eddd830a5b58480a6143d616a97e3a6c23c439
```

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
409                               | Torrent queueing is not enabled
200                               | All other scenarios

## Set file priority ##

Name: `filePrio`

**Parameters:**

Parameter                         | Type    | Description
----------------------------------|---------|------------
`hash`                            | string  | The hash of the torrent
`id`                              | string  | File ids, separated by `\|`
`priority`                        | number  | File priority to set (consult [torrent contents API](#get-torrent-contents) for possible values)

`id` values correspond to file position inside the array returned by [torrent contents API](#get-torrent-contents), e.g. `id=0` for first file, `id=1` for second file, etc.

Since 2.8.2 it is reccomended to use `index` field returned by [torrent contents API](#get-torrent-contents) (since the files can be filtered and the `index` value may differ from the position inside the response array).

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
400                               | Priority is invalid
400                               | At least one file `id` is not a valid integer
404                               | Torrent hash was not found
409                               | Torrent metadata hasn't downloaded yet
409                               | At least one file `id` was not found
200                               | All other scenarios

## Get torrent download limit ##

Requires knowing the torrent hash. You can get it from [torrent list](#get-torrent-list).

```http
POST /api/v2/torrents/downloadLimit HTTP/1.1
User-Agent: Fiddler
Host: 127.0.0.1
Cookie: SID=your_sid
Content-Type: application/x-www-form-urlencoded
Content-Length: length

hashes=8c212779b4abde7c6bc608063a0d008b7e40ce32|284b83c9c7935002391129fd97f43db5d7cc2ba0
```

`hashes` can contain multiple hashes separated by `|` or set to `all`

Server reply (example):

```http
HTTP/1.1 200 OK
content-type: application/json
content-length: length

{"8c212779b4abde7c6bc608063a0d008b7e40ce32":338944,"284b83c9c7935002391129fd97f43db5d7cc2ba0":123}
```

`8c212779b4abde7c6bc608063a0d008b7e40ce32` is the hash of the torrent and `338944` its download speed limit in bytes per second; this value will be zero if no limit is applied.

## Set torrent download limit ##

Requires knowing the torrent hash. You can get it from [torrent list](#get-torrent-list).

```http
POST /api/v2/torrents/setDownloadLimit HTTP/1.1
User-Agent: Fiddler
Host: 127.0.0.1
Cookie: SID=your_sid
Content-Type: application/x-www-form-urlencoded
Content-Length: length

hashes=8c212779b4abde7c6bc608063a0d008b7e40ce32|284b83c9c7935002391129fd97f43db5d7cc2ba0&limit=131072
```

`hashes` can contain multiple hashes separated by `|` or set to `all`
`limit` is the download speed limit in bytes per second you want to set.

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

## Set torrent share limit ##

Requires knowing the torrent hash. You can get it from [torrent list](#get-torrent-list).

```http
POST /api/v2/torrents/setShareLimits HTTP/1.1
User-Agent: Fiddler
Host: 127.0.0.1
Cookie: SID=your_sid
Content-Type: application/x-www-form-urlencoded
Content-Length: length

hashes=8c212779b4abde7c6bc608063a0d008b7e40ce32|284b83c9c7935002391129fd97f43db5d7cc2ba0&ratioLimit=1.0&seedingTimeLimit=60
```

`hashes` can contain multiple hashes separated by `|` or set to `all`
`ratioLimit` is the max ratio the torrent should be seeded until. `-2` means the global limit should be used, `-1` means no limit.
`seedingTimeLimit` is the max amount of time (minutes) the torrent should be seeded. `-2` means the global limit should be used, `-1` means no limit.

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

## Get torrent upload limit ##

Requires knowing the torrent hash. You can get it from [torrent list](#get-torrent-list).

```http
POST /api/v2/torrents/uploadLimit HTTP/1.1
User-Agent: Fiddler
Host: 127.0.0.1
Cookie: SID=your_sid
Content-Type: application/x-www-form-urlencoded
Content-Length: length

hashes=8c212779b4abde7c6bc608063a0d008b7e40ce32|284b83c9c7935002391129fd97f43db5d7cc2ba0
```

`hashes` can contain multiple hashes separated by `|` or set to `all`

Server reply (example):

```http
HTTP/1.1 200 OK
content-type: application/json
content-length: length

{"8c212779b4abde7c6bc608063a0d008b7e40ce32":338944,"284b83c9c7935002391129fd97f43db5d7cc2ba0":123}
```

`8c212779b4abde7c6bc608063a0d008b7e40ce32` is the hash of the torrent in the request and `338944` its upload speed limit in bytes per second; this value will be zero if no limit is applied.

## Set torrent upload limit ##

Requires knowing the torrent hash. You can get it from [torrent list](#get-torrent-list).

```http
POST /api/v2/torrents/setUploadLimit HTTP/1.1
User-Agent: Fiddler
Host: 127.0.0.1
Cookie: SID=your_sid
Content-Type: application/x-www-form-urlencoded
Content-Length: length

hashes=8c212779b4abde7c6bc608063a0d008b7e40ce32|284b83c9c7935002391129fd97f43db5d7cc2ba0&limit=131072
```

`hashes` can contain multiple hashes separated by `|` or set to `all`
`limit` is the upload speed limit in bytes per second you want to set.

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

## Set torrent location ##

Requires knowing the torrent hash. You can get it from [torrent list](#get-torrent-list).

```http
POST /api/v2/torrents/setLocation HTTP/1.1
User-Agent: Fiddler
Host: 127.0.0.1
Cookie: SID=your_sid
Content-Type: application/x-www-form-urlencoded
Content-Length: length

hashes=8c212779b4abde7c6bc608063a0d008b7e40ce32|284b83c9c7935002391129fd97f43db5d7cc2ba0&location=/mnt/nfs/media
```

`hashes` can contain multiple hashes separated by `|` or set to `all`
`location` is the location to download the torrent to. If the location doesn't exist, the torrent's location is unchanged.

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
400                               | Save path is empty
403                               | User does not have write access to directory
409                               | Unable to create save path directory
200                               | All other scenarios

## Set torrent name ##

Requires knowing the torrent hash. You can get it from [torrent list](#get-torrent-list).

```http
POST /api/v2/torrents/rename HTTP/1.1
User-Agent: Fiddler
Host: 127.0.0.1
Cookie: SID=your_sid
Content-Type: application/x-www-form-urlencoded
Content-Length: length

hash=8c212779b4abde7c6bc608063a0d008b7e40ce32&name=This%20is%20a%20test
```

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
404                               | Torrent hash is invalid
409                               | Torrent name is empty
200                               | All other scenarios

## Set torrent category ##

Requires knowing the torrent hash. You can get it from [torrent list](#get-torrent-list).

```http
POST /api/v2/torrents/setCategory HTTP/1.1
User-Agent: Fiddler
Host: 127.0.0.1
Cookie: SID=your_sid
Content-Type: application/x-www-form-urlencoded
Content-Length: length

hashes=8c212779b4abde7c6bc608063a0d008b7e40ce32|284b83c9c7935002391129fd97f43db5d7cc2ba0&category=CategoryName
```

`hashes` can contain multiple hashes separated by `|` or set to `all`

`category` is the torrent category you want to set.

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
409                               | Category name does not exist
200                               | All other scenarios

## Get all categories ##

Name: `categories`

Parameters:

None

Returns all categories in JSON format, e.g.:

```JSON
{
    "Video": {
        "name": "Video",
        "savePath": "/home/user/torrents/video/"
    },
    "eBooks": {
        "name": "eBooks",
        "savePath": "/home/user/torrents/eBooks/"
    }
}
```
**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

## Add new category ##

```http
POST /api/v2/torrents/createCategory HTTP/1.1
User-Agent: Fiddler
Host: 127.0.0.1
Cookie: SID=your_sid
Content-Type: application/x-www-form-urlencoded
Content-Length: length

category=CategoryName&savePath=/path/to/dir
```

`category` is the category you want to create.

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
400                               | Category name is empty
409                               | Category name is invalid
200                               | All other scenarios

## Edit category ##

```http
POST /api/v2/torrents/editCategory HTTP/1.1
User-Agent: Fiddler
Host: 127.0.0.1
Cookie: SID=your_sid
Content-Type: application/x-www-form-urlencoded
Content-Length: length

category=CategoryName&savePath=/path/to/save/torrents/to
```

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
400                               | Category name is empty
409                               | Category editing failed
200                               | All other scenarios

## Remove categories ##

```http
POST /api/v2/torrents/removeCategories HTTP/1.1
User-Agent: Fiddler
Host: 127.0.0.1
Cookie: SID=your_sid
Content-Type: application/x-www-form-urlencoded
Content-Length: length

categories=Category1%0ACategory2
```

`categories` can contain multiple cateogies separated by `\n` (%0A urlencoded)

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

## Add torrent tags ##

Requires knowing the torrent hash. You can get it from [torrent list](#get-torrent-list).

```http
POST /api/v2/torrents/addTags HTTP/1.1
User-Agent: Fiddler
Host: 127.0.0.1
Cookie: SID=your_sid
Content-Type: application/x-www-form-urlencoded
Content-Length: length

hashes=8c212779b4abde7c6bc608063a0d008b7e40ce32|284b83c9c7935002391129fd97f43db5d7cc2ba0&tags=TagName1,TagName2
```

`hashes` can contain multiple hashes separated by `|` or set to `all`

`tags` is the list of tags you want to add to passed torrents.

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

## Remove torrent tags ##

Requires knowing the torrent hash. You can get it from [torrent list](#get-torrent-list).

```http
POST /api/v2/torrents/removeTags HTTP/1.1
User-Agent: Fiddler
Host: 127.0.0.1
Cookie: SID=your_sid
Content-Type: application/x-www-form-urlencoded
Content-Length: length

hashes=8c212779b4abde7c6bc608063a0d008b7e40ce32|284b83c9c7935002391129fd97f43db5d7cc2ba0&tags=TagName1,TagName2
```

`hashes` can contain multiple hashes separated by `|` or set to `all`

`tags` is the list of tags you want to remove from passed torrents.
Empty list removes all tags from relevant torrents.

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

## Get all tags ##

Name: `tags`

Parameters:

None

Returns all tags in JSON format, e.g.:

```JSON
[
    "Tag 1",
    "Tag 2"
]
```
**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

## Create tags ##

```http
POST /api/v2/torrents/createTags HTTP/1.1
User-Agent: Fiddler
Host: 127.0.0.1
Cookie: SID=your_sid
Content-Type: application/x-www-form-urlencoded
Content-Length: length

tags=TagName1,TagName2
```
`tags` is a list of tags you want to create.
Can contain multiple tags separated by `,`.

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

## Delete tags ##

```http
POST /api/v2/torrents/deleteTags HTTP/1.1
User-Agent: Fiddler
Host: 127.0.0.1
Cookie: SID=your_sid
Content-Type: application/x-www-form-urlencoded
Content-Length: length

tags=TagName1,TagName2
```

`tags` is a list of tags you want to delete.
Can contain multiple tags separated by `,`.

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

## Set automatic torrent management ##

Requires knowing the torrent hash. You can get it from [torrent list](#get-torrent-list).

```http
POST /api/v2/torrents/setAutoManagement HTTP/1.1
User-Agent: Fiddler
Host: 127.0.0.1
Cookie: SID=your_sid
Content-Type: application/x-www-form-urlencoded
Content-Length: length

hashes=8c212779b4abde7c6bc608063a0d008b7e40ce32|284b83c9c7935002391129fd97f43db5d7cc2ba0&enable=true
```

`hashes` can contain multiple hashes separated by `|` or set to `all`
`enable` is a boolean, affects the torrents listed in `hashes`, default is `false`

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

## Toggle sequential download ##

Requires knowing the torrent hash. You can get it from [torrent list](#get-torrent-list).

Name: `toggleSequentialDownload`

**Parameters:**

Parameter   | Type     | Description
------------|----------|------------
`hashes`    | string   | The hashes of the torrents you want to toggle sequential download for. `hashes` can contain multiple hashes separated by `\|`, to toggle sequential download for multiple torrents, or set to `all`, to toggle sequential download for all torrents.

Example:

```http
/api/v2/torrents/toggleSequentialDownload?hashes=8c212779b4abde7c6bc608063a0d008b7e40ce32|54eddd830a5b58480a6143d616a97e3a6c23c439
```

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

## Set first/last piece priority ##

Requires knowing the torrent hash. You can get it from [torrent list](#get-torrent-list).

Name: `toggleFirstLastPiecePrio`

**Parameters:**

Parameter   | Type     | Description
------------|----------|------------
`hashes`    | string   | The hashes of the torrents you want to toggle the first/last piece priority for. `hashes` can contain multiple hashes separated by `\|`, to toggle the first/last piece priority for multiple torrents, or set to `all`, to toggle the first/last piece priority for all torrents.

Example:

```http
/api/v2/torrents/toggleFirstLastPiecePrio?hashes=8c212779b4abde7c6bc608063a0d008b7e40ce32|54eddd830a5b58480a6143d616a97e3a6c23c439
```

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

## Set force start ##

Requires knowing the torrent hash. You can get it from [torrent list](#get-torrent-list).

```http
POST /api/v2/torrents/setForceStart HTTP/1.1
User-Agent: Fiddler
Host: 127.0.0.1
Cookie: SID=your_sid
Content-Type: application/x-www-form-urlencoded
Content-Length: length

hashes=8c212779b4abde7c6bc608063a0d008b7e40ce32?value=true
```

`hashes` can contain multiple hashes separated by `|` or set to `all`
`value` is a boolean, affects the torrents listed in `hashes`, default is `false`

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

## Set super seeding ##

Requires knowing the torrent hash. You can get it from [torrent list](#get-torrent-list).

```http
POST /api/v2/torrents/setSuperSeeding HTTP/1.1
User-Agent: Fiddler
Host: 127.0.0.1
Cookie: SID=your_sid
Content-Type: application/x-www-form-urlencoded
Content-Length: length

hashes=8c212779b4abde7c6bc608063a0d008b7e40ce32?value=true
```

`hashes` can contain multiple hashes separated by `|` or set to `all`
`value` is a boolean, affects the torrents listed in `hashes`, default is `false`

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

## Rename file ##

Name: `renameFile`

**Parameters:**

Parameter                         | Type     | Description
----------------------------------|----------|------------
`hash`                            | string   | The hash of the torrent
`oldPath`                         | string   | The old path of the torrent
`newPath`                         | string   | The new path to use for the file

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
400                               | Missing `newPath` parameter
409                               | Invalid `newPath` or `oldPath`, or `newPath` already in use
200                               | All other scenarios

## Rename folder ##

Name: `renameFolder`

**Parameters:**

Parameter                         | Type     | Description
----------------------------------|----------|------------
`hash`                            | string   | The hash of the torrent
`oldPath`                         | string   | The old path of the torrent
`newPath`                         | string   | The new path to use for the file

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
400                               | Missing `newPath` parameter
409                               | Invalid `newPath` or `oldPath`, or `newPath` already in use
200                               | All other scenarios

# RSS (experimental) #

All RSS API methods are under "rss", e.g.: `/api/v2/rss/methodName`.

## Add folder ##

Name: `addFolder`

Parameters:

Parameter                         | Type    | Description
----------------------------------|---------|------------
`path`                            | string  | Full path of added folder (e.g. "The Pirate Bay\Top100")

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
409                               | Failure to add folder
200                               | All other scenarios

## Add feed ##

Name: `addFeed`

Parameters:

Parameter                         | Type    | Description
----------------------------------|---------|------------
`url`                             | string  | URL of RSS feed (e.g. "[http://thepiratebay.org/rss//top100/200](http://thepiratebay.org/rss//top100/200)")
`path` _optional_                 | string  | Full path of added folder (e.g. "The Pirate Bay\Top100\Video")

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
409                               | Failure to add feed
200                               | All other scenarios

## Remove item ##

Removes folder or feed.

Name: `removeItem`

Parameters:

Parameter                         | Type    | Description
----------------------------------|---------|------------
`path`                            | string  | Full path of removed item (e.g. "The Pirate Bay\Top100")

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
409                               | Failure to remove item
200                               | All other scenarios

## Move item ##

Moves/renames folder or feed.

Name: `moveItem`

Parameters:

Parameter                         | Type    | Description
----------------------------------|---------|------------
`itemPath`                        | string  | Current full path of item (e.g. "The Pirate Bay\Top100")
`destPath`                        | string  | New full path of item (e.g. "The Pirate Bay")

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
409                               | Failure to move item
200                               | All other scenarios

## Get all items ##

Name: `items`

Parameters:

Parameter                         | Type    | Description
----------------------------------|---------|------------
`withData` _optional_             | bool    | True if you need current feed articles

Returns all RSS items in JSON format, e.g.:

```JSON
{
    "HD-Torrents.org": "https://hd-torrents.org/rss.php",
    "PowerfulJRE": "https://www.youtube.com/feeds/videos.xml?channel_id=UCzQUP1qoWDoEbmsQxvdjxgQ",
    "The Pirate Bay": {
        "Audio": "https://thepiratebay.org/rss//top100/100",
        "Video": "https://thepiratebay.org/rss//top100/200"
    }
}
```

## Mark as read ##

If `articleId` is provided only the article is marked as read otherwise the whole feed is going to be marked as read.

Name: `markAsRead`

Parameters:

Parameter                         | Type    | Description
----------------------------------|---------|------------
`itemPath`                        | string  | Current full path of item (e.g. "The Pirate Bay\Top100")
`articleId` _optional_            | string  | ID of article

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

## Refresh item ##

Refreshes folder or feed.

Name: `refreshItem`

Parameters:

Parameter                         | Type    | Description
----------------------------------|---------|------------
`itemPath`                        | string  | Current full path of item (e.g. "The Pirate Bay\Top100")

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

## Set auto-downloading rule ##

Name: `setRule`

Parameters:

Parameter                         | Type    | Description
----------------------------------|---------|------------
`ruleName`                        | string  | Rule name (e.g. "Punisher")
`ruleDef`                         | string  | JSON encoded rule definition

Rule definition is JSON encoded dictionary with the following fields:

Field                             | Type    | Description
----------------------------------|---------|------------
`enabled`                         | bool    | Whether the rule is enabled
`mustContain`                     | string  | The substring that the torrent name must contain
`mustNotContain`                  | string  | The substring that the torrent name must not contain
`useRegex`                        | bool    | Enable regex mode in "mustContain" and "mustNotContain"
`episodeFilter`                   | string  | Episode filter definition
`smartFilter`                     | bool    | Enable smart episode filter
`previouslyMatchedEpisodes`       | list    | The list of episode IDs already matched by smart filter
`affectedFeeds`                   | list    | The feed URLs the rule applied to
`ignoreDays`                      | number  | Ignore sunsequent rule matches
`lastMatch`                       | string  | The rule last match time
`addPaused`                       | bool    | Add matched torrent in paused mode
`assignedCategory`                | string  | Assign category to the torrent
`savePath`                        | string  | Save torrent to the given directory

E.g.:

```JSON
{
    "enabled": false,
    "mustContain": "The *Punisher*",
    "mustNotContain": "",
    "useRegex": false,
    "episodeFilter": "1x01-;",
    "smartFilter": false,
    "previouslyMatchedEpisodes": [
    ],
    "affectedFeeds": [
        "http://showrss.info/user/134567.rss?magnets=true"
    ],
    "ignoreDays": 0,
    "lastMatch": "20 Nov 2017 09:05:11",
    "addPaused": true,
    "assignedCategory": "",
    "savePath": "C:/Users/JohnDoe/Downloads/Punisher"
}
```

## Rename auto-downloading rule ##

Name: `renameRule`

Parameters:

Parameter                         | Type    | Description
----------------------------------|---------|------------
`ruleName`                        | string  | Rule name (e.g. "Punisher")
`newRuleName`                     | string  | New rule name (e.g. "The Punisher")

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

## Remove auto-downloading rule ##

Name: `removeRule`

Parameters:

Parameter                         | Type    | Description
----------------------------------|---------|------------
`ruleName`                        | string  | Rule name (e.g. "Punisher")

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios


## Get all auto-downloading rules ##

Name: `rules`

Returns all auto-downloading rules in JSON format, e.g.:

```JSON
{
    "The Punisher": {
        "enabled": false,
        "mustContain": "The *Punisher*",
        "mustNotContain": "",
        "useRegex": false,
        "episodeFilter": "1x01-;",
        "smartFilter": false,
        "previouslyMatchedEpisodes": [
        ],
        "affectedFeeds": [
            "http://showrss.info/user/134567.rss?magnets=true"
        ],
        "ignoreDays": 0,
        "lastMatch": "20 Nov 2017 09:05:11",
        "addPaused": true,
        "assignedCategory": "",
        "savePath": "C:/Users/JohnDoe/Downloads/Punisher"
    }
}
```
**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

## Get all articles matching a rule ##

Name: `matchingArticles`

Parameter                         | Type    | Description
----------------------------------|---------|------------
`ruleName`                        | string  | Rule name (e.g. "Linux")


Returns all articles that match a rule by feed name in JSON format, e.g.:

```JSON
{
    "DistroWatch":[
        "sparkylinux-5.11-i686-minimalgui.iso.torrent",
        "sparkylinux-5.11-x86_64-minimalgui.iso.torrent",
        "sparkylinux-5.11-i686-xfce.iso.torrent",
        "bluestar-linux-5.6.3-2020.04.09-x86_64.iso.torrent",
        "robolinux64-mate3d-v10.10.iso.torrent",
    ],
    "Linuxtracker":[
        "[Alpine Linux] alpine-extended-3.11.6",
        "[Alpine Linux] alpine-standard-3.11.6",
        "[Linuxfx] linuxfx10-wxs-lts-beta5.iso",
        "[Linux Lite] linux-lite-5.0-rc1-64bit.iso (MULTI)",
        "[Scientific Linux] SL-7.8-x86_64-Pack",
        "[NixOS] nixos-plasma5-20.03.1418.5272327b81e-x86_64-linux.iso"
    ]
}
```

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios


# Search #

All Search API methods are under "search", e.g.: `/api/v2/search/methodName`.

## Start search ##

Name: `start`

**Parameters:**

Parameter                         | Type    | Description
----------------------------------|---------|------------
`pattern`                         | string  | Pattern to search for (e.g. "Ubuntu 18.04")
`plugins`                         | string  | Plugins to use for searching (e.g. "legittorrents"). Supports multiple plugins separated by `\|`. Also supports `all` and `enabled`
`category`                        | string  | Categories to limit your search to (e.g. "legittorrents"). Available categories depend on the specified `plugins`. Also supports `all`

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
409                               | User has reached the limit of max `Running` searches (currently set to 5)
200                               | All other scenarios- see JSON below

The response is a JSON object with the following fields

Field                             | Type    | Description
----------------------------------|---------|------------
`id`                              | number  | ID of the search job

Example:

```JSON
{
    "id": 12345
}
```

## Stop search ##

Name: `stop`

**Parameters:**

Parameter                         | Type    | Description
----------------------------------|---------|------------
`id`                              | number  | ID of the search job

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
404                               | Search job was not found
200                               | All other scenarios

## Get search status ##

Name: `status`

**Parameters:**

Parameter                         | Type    | Description
----------------------------------|---------|------------
`id` _optional_                   | number  | ID of the search job. If not specified, all search jobs are returned

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
404                               | Search job was not found
200                               | All other scenarios- see JSON below

The response is a JSON array of objects containing the following fields

Field                             | Type    | Description
----------------------------------|---------|------------
`id`                              | number  | ID of the search job
`status`                          | string  | Current status of the search job (either `Running` or `Stopped`)
`total`                           | number  | Total number of results. If the status is `Running` this number may contineu to increase

Example:

```JSON
[
    {
        "id": 12345,
        "status": "Running",
        "total": 170
    }
]
```

## Get search results ##

Name: `results`

**Parameters:**

Parameter                         | Type    | Description
----------------------------------|---------|------------
`id`                              | number  | ID of the search job
`limit` _optional_                | number  | max number of results to return. 0 or negative means no limit
`offset` _optional_               | number  | result to start at. A negative number means count backwards (e.g. `-2` returns the 2 most recent results)

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
404                               | Search job was not found
409                               | Offset is too large, or too small (e.g. absolute value of negative number is greater than # results)
200                               | All other scenarios- see JSON below

The response is a JSON object with the following fields

Field                             | Type    | Description
----------------------------------|---------|------------
`results`                         | array   | Array of `result` objects- see table below
`status`                          | string  | Current status of the search job (either `Running` or `Stopped`)
`total`                           | number  | Total number of results. If the status is `Running` this number may continue to increase

**Result object:**

Field                             | Type    | Description
----------------------------------|---------|------------
`descrLink`                       | string  | URL of the torrent's description page
`fileName`                        | string  | Name of the file
`fileSize`                        | number  | Size of the file in Bytes
`fileUrl`                         | string  | Torrent download link (usually either .torrent file or magnet link)
`nbLeechers`                      | number  | Number of leechers
`nbSeeders`                       | number  | Number of seeders
`siteUrl`                         | string  | URL of the torrent site

Example:

```JSON
{
    "results": [
        {
            "descrLink": "http://www.legittorrents.info/index.php?page=torrent-details&id=8d5f512e1acb687029b8d7cc6c5a84dce51d7a41",
            "fileName": "Ubuntu-10.04-32bit-NeTV.ova",
            "fileSize": -1,
            "fileUrl": "http://www.legittorrents.info/download.php?id=8d5f512e1acb687029b8d7cc6c5a84dce51d7a41&f=Ubuntu-10.04-32bit-NeTV.ova.torrent",
            "nbLeechers": 1,
            "nbSeeders": 0,
            "siteUrl": "http://www.legittorrents.info"
        },
        {
            "descrLink": "http://www.legittorrents.info/index.php?page=torrent-details&id=d5179f53e105dc2c2401bcfaa0c2c4936a6aa475",
            "fileName": "mangOH-Legato-17_06-Ubuntu-16_04.ova",
            "fileSize": -1,
            "fileUrl": "http://www.legittorrents.info/download.php?id=d5179f53e105dc2c2401bcfaa0c2c4936a6aa475&f=mangOH-Legato-17_06-Ubuntu-16_04.ova.torrent",
            "nbLeechers": 0,
            "nbSeeders": 59,
            "siteUrl": "http://www.legittorrents.info"
        }
    ],
    "status": "Running",
    "total": 2
}
```

## Delete search ##

Name: `delete`

**Parameters:**

Parameter                         | Type    | Description
----------------------------------|---------|------------
`id`                              | number  | ID of the search job

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
404                               | Search job was not found
200                               | All other scenarios

## Get search plugins ##

Name: `plugins`

**Parameters:**

None

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios- see JSON below

The response is a JSON array of objects containing the following fields

Field                             | Type    | Description
----------------------------------|---------|------------
`enabled`                         | bool    | Whether the plugin is enabled
`fullName`                        | string  | Full name of the plugin
`name`                            | string  | Short name of the plugin
`supportedCategories`             | array   | List of category objects
`url`                             | string  | URL of the torrent site
`version`                         | string  | Installed version of the plugin

```JSON
[
    {
        "enabled": true,
        "fullName": "Legit Torrents",
        "name": "legittorrents",
        "supportedCategories": [{
            "id": "all",
            "name": "All categories"
        }, {
            "id": "anime",
            "name": "Anime"
        }, {
            "id": "books",
            "name": "Books"
        }, {
            "id": "games",
            "name": "Games"
        }, {
            "id": "movies",
            "name": "Movies"
        }, {
            "id": "music",
            "name": "Music"
        }, {
            "id": "tv",
            "name": "TV shows"
        }],
        "url": "http://www.legittorrents.info",
        "version": "2.3"
    }
]
```

## Install search plugin ##

Name: `installPlugin`

**Parameters:**

Parameter                         | Type    | Description
----------------------------------|---------|------------
`sources`                         | string  | Url or file path of the plugin to install (e.g. "[https://raw.githubusercontent.com/qbittorrent/search-plugins/master/nova3/engines/legittorrents.py](https://raw.githubusercontent.com/qbittorrent/search-plugins/master/nova3/engines/legittorrents.py)"). Supports multiple sources separated by `\|`

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

## Uninstall search plugin ##

Name: `uninstallPlugin`

**Parameters:**

Parameter                         | Type    | Description
----------------------------------|---------|------------
`names`                           | string  | Name of the plugin to uninstall (e.g. "legittorrents"). Supports multiple names separated by `\|`

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

## Enable search plugin ##

Name: `enablePlugin`

**Parameters:**

Parameter                         | Type    | Description
----------------------------------|---------|------------
`names`                           | string  | Name of the plugin to enable/disable (e.g. "legittorrents"). Supports multiple names separated by `\|`
`enable`                          | bool    | Whether the plugins should be enabled

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

## Update search plugins ##

Name: `updatePlugins`

**Parameters:**

None

**Returns:**

HTTP Status Code                  | Scenario
----------------------------------|---------------------
200                               | All scenarios

# WebAPI versioning #
WebAPI uses the following versioning: `1.2.3`:
1. Main version. Should be changed only on some global changes (e.g. total redesign/relayout)
2. Changed on incompatible API changes (i.e. if it breaks outdated clients). E.g. if you change/remove something
3. Changed on compatible API changes (i.e. if it doesn't break outdated clients). E.g. if you add something new outdated clients still can access old subset of API.
