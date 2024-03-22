/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2023-2024  Vladimir Golovnev <glassez@yandex.ru>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If
 * you modify file(s), you may extend this exception to your version of the
 * file(s), but you are not obligated to do so. If you do not wish to do so,
 * delete this exception statement from your version.
 */

#pragma once

#include <QColor>
#include <QHash>
#include <QPalette>
#include <QSet>
#include <QString>

#include "base/global.h"
#include "color.h"

inline const QString CONFIG_FILE_NAME = u"config.json"_s;
inline const QString STYLESHEET_FILE_NAME = u"stylesheet.qss"_s;
inline const QString KEY_COLORS = u"colors"_s;
inline const QString KEY_COLORS_LIGHT = u"colors.light"_s;
inline const QString KEY_COLORS_DARK = u"colors.dark"_s;

struct UIThemeColor
{
    QColor light;
    QColor dark;
};

inline QHash<QString, UIThemeColor> defaultUIThemeColors()
{
    return {
        {u"Log.TimeStamp"_s, {Color::Primer::Light::fgSubtle, Color::Primer::Dark::fgSubtle}},
        {u"Log.Normal"_s, {{}, {}}},
        {u"Log.Info"_s, {Color::Primer::Light::accentFg, Color::Primer::Dark::accentFg}},
        {u"Log.Warning"_s, {Color::Primer::Light::severeFg, Color::Primer::Dark::severeFg}},
        {u"Log.Critical"_s, {Color::Primer::Light::dangerFg, Color::Primer::Dark::dangerFg}},
        {u"Log.BannedPeer"_s, {Color::Primer::Light::dangerFg, Color::Primer::Dark::dangerFg}},

        {u"RSS.ReadArticle"_s, {{}, {}}},
        {u"RSS.UnreadArticle"_s, {{}, {}}},

        {u"TransferList.Downloading"_s, {Color::Primer::Light::successFg, Color::Primer::Dark::successFg}},
        {u"TransferList.StalledDownloading"_s, {Color::Primer::Light::successEmphasis, Color::Primer::Dark::successEmphasis}},
        {u"TransferList.DownloadingMetadata"_s, {Color::Primer::Light::successFg, Color::Primer::Dark::successFg}},
        {u"TransferList.ForcedDownloadingMetadata"_s, {Color::Primer::Light::successFg, Color::Primer::Dark::successFg}},
        {u"TransferList.ForcedDownloading"_s, {Color::Primer::Light::successFg, Color::Primer::Dark::successFg}},
        {u"TransferList.Uploading"_s, {Color::Primer::Light::accentFg, Color::Primer::Dark::accentFg}},
        {u"TransferList.StalledUploading"_s, {Color::Primer::Light::accentEmphasis, Color::Primer::Dark::accentEmphasis}},
        {u"TransferList.ForcedUploading"_s, {Color::Primer::Light::accentFg, Color::Primer::Dark::accentFg}},
        {u"TransferList.QueuedDownloading"_s, {Color::Primer::Light::scaleYellow6, Color::Primer::Dark::scaleYellow6}},
        {u"TransferList.QueuedUploading"_s, {Color::Primer::Light::scaleYellow6, Color::Primer::Dark::scaleYellow6}},
        {u"TransferList.CheckingDownloading"_s, {Color::Primer::Light::successFg, Color::Primer::Dark::successFg}},
        {u"TransferList.CheckingUploading"_s, {Color::Primer::Light::successFg, Color::Primer::Dark::successFg}},
        {u"TransferList.CheckingResumeData"_s, {Color::Primer::Light::successFg, Color::Primer::Dark::successFg}},
        {u"TransferList.StoppedDownloading"_s, {Color::Primer::Light::fgMuted, Color::Primer::Dark::fgMuted}},
        {u"TransferList.StoppedUploading"_s, {Color::Primer::Light::doneFg, Color::Primer::Dark::doneFg}},
        {u"TransferList.Moving"_s, {Color::Primer::Light::successFg, Color::Primer::Dark::successFg}},
        {u"TransferList.MissingFiles"_s, {Color::Primer::Light::dangerFg, Color::Primer::Dark::dangerFg}},
        {u"TransferList.Error"_s, {Color::Primer::Light::dangerFg, Color::Primer::Dark::dangerFg}}
    };
}

inline QSet<QString> defaultUIThemeIcons()
{
    return {
        u"application-exit"_s,
        u"application-rss"_s,
        u"application-url"_s,
        u"browser-cookies"_s,
        u"chart-line"_s,
        u"checked-completed"_s,
        u"configure"_s,
        u"connected"_s,
        u"dialog-warning"_s,
        u"directory"_s,
        u"disconnected"_s,
        u"download"_s,
        u"downloading"_s,
        u"edit-clear"_s,
        u"edit-copy"_s,
        u"edit-find"_s,
        u"edit-rename"_s,
        u"error"_s,
        u"fileicon"_s,
        u"filter-active"_s,
        u"filter-all"_s,
        u"filter-inactive"_s,
        u"filter-stalled"_s,
        u"firewalled"_s,
        u"folder-documents"_s,
        u"folder-new"_s,
        u"folder-remote"_s,
        u"force-recheck"_s,
        u"go-bottom"_s,
        u"go-down"_s,
        u"go-top"_s,
        u"go-up"_s,
        u"hash"_s,
        u"help-about"_s,
        u"help-contents"_s,
        u"insert-link"_s,
        u"ip-blocked"_s,
        u"list-add"_s,
        u"list-remove"_s,
        u"loading"_s,
        u"mail-inbox"_s,
        u"name"_s,
        u"network-connect"_s,
        u"network-server"_s,
        u"object-locked"_s,
        u"pause-session"_s,
        u"paused"_s,
        u"peers"_s,
        u"peers-add"_s,
        u"peers-remove"_s,
        u"plugins"_s,
        u"preferences-advanced"_s,
        u"preferences-bittorrent"_s,
        u"preferences-desktop"_s,
        u"preferences-webui"_s,
        u"qbittorrent-tray"_s,
        u"qbittorrent-tray-dark"_s,
        u"qbittorrent-tray-light"_s,
        u"queued"_s,
        u"ratio"_s,
        u"reannounce"_s,
        u"rss_read_article"_s,
        u"rss_unread_article"_s,
        u"security-high"_s,
        u"security-low"_s,
        u"set-location"_s,
        u"slow"_s,
        u"slow_off"_s,
        u"speedometer"_s,
        u"stalledDL"_s,
        u"stalledUP"_s,
        u"stopped"_s,
        u"system-log-out"_s,
        u"tags"_s,
        u"task-complete"_s,
        u"task-reject"_s,
        u"torrent-creator"_s,
        u"torrent-magnet"_s,
        u"torrent-start"_s,
        u"torrent-start-forced"_s,
        u"torrent-stop"_s,
        u"tracker-error"_s,
        u"tracker-warning"_s,
        u"trackerless"_s,
        u"trackers"_s,
        u"upload"_s,
        u"view-categories"_s,
        u"view-preview"_s,
        u"view-refresh"_s,
        u"view-statistics"_s,
        u"wallet-open"_s
    };
}
