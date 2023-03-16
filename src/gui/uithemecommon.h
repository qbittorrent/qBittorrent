/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2023  Vladimir Golovnev <glassez@yandex.ru>
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

#include <QtGlobal>
#include <QApplication>
#include <QColor>
#include <QHash>
#include <QPalette>
#include <QSet>
#include <QString>

#include "base/global.h"
#include "color.h"

inline const QString CONFIG_FILE_NAME = u"config.json"_qs;
inline const QString STYLESHEET_FILE_NAME = u"stylesheet.qss"_qs;
inline const QString KEY_COLORS = u"colors"_qs;
inline const QString KEY_COLORS_LIGHT = u"colors.light"_qs;
inline const QString KEY_COLORS_DARK = u"colors.dark"_qs;

struct UIThemeColor
{
    QColor light;
    QColor dark;
};

inline QHash<QString, UIThemeColor> defaultUIThemeColors()
{
    const QPalette palette = QApplication::palette();
    return {
        {u"Log.TimeStamp"_qs, {Color::Primer::Light::fgSubtle, Color::Primer::Dark::fgSubtle}},
        {u"Log.Normal"_qs, {palette.color(QPalette::Active, QPalette::WindowText), palette.color(QPalette::Active, QPalette::WindowText)}},
        {u"Log.Info"_qs, {Color::Primer::Light::accentFg, Color::Primer::Dark::accentFg}},
        {u"Log.Warning"_qs, {Color::Primer::Light::severeFg, Color::Primer::Dark::severeFg}},
        {u"Log.Critical"_qs, {Color::Primer::Light::dangerFg, Color::Primer::Dark::dangerFg}},
        {u"Log.BannedPeer"_qs, {Color::Primer::Light::dangerFg, Color::Primer::Dark::dangerFg}},

        {u"RSS.ReadArticle"_qs, {palette.color(QPalette::Inactive, QPalette::WindowText), palette.color(QPalette::Inactive, QPalette::WindowText)}},
        {u"RSS.UnreadArticle"_qs, {palette.color(QPalette::Active, QPalette::Link), palette.color(QPalette::Active, QPalette::Link)}},

        {u"TransferList.Downloading"_qs, {Color::Primer::Light::successFg, Color::Primer::Dark::successFg}},
        {u"TransferList.StalledDownloading"_qs, {Color::Primer::Light::successEmphasis, Color::Primer::Dark::successEmphasis}},
        {u"TransferList.DownloadingMetadata"_qs, {Color::Primer::Light::successFg, Color::Primer::Dark::successFg}},
        {u"TransferList.ForcedDownloadingMetadata"_qs, {Color::Primer::Light::successFg, Color::Primer::Dark::successFg}},
        {u"TransferList.ForcedDownloading"_qs, {Color::Primer::Light::successFg, Color::Primer::Dark::successFg}},
        {u"TransferList.Uploading"_qs, {Color::Primer::Light::accentFg, Color::Primer::Dark::accentFg}},
        {u"TransferList.StalledUploading"_qs, {Color::Primer::Light::accentEmphasis, Color::Primer::Dark::accentEmphasis}},
        {u"TransferList.ForcedUploading"_qs, {Color::Primer::Light::accentFg, Color::Primer::Dark::accentFg}},
        {u"TransferList.QueuedDownloading"_qs, {Color::Primer::Light::scaleYellow6, Color::Primer::Dark::scaleYellow6}},
        {u"TransferList.QueuedUploading"_qs, {Color::Primer::Light::scaleYellow6, Color::Primer::Dark::scaleYellow6}},
        {u"TransferList.CheckingDownloading"_qs, {Color::Primer::Light::successFg, Color::Primer::Dark::successFg}},
        {u"TransferList.CheckingUploading"_qs, {Color::Primer::Light::successFg, Color::Primer::Dark::successFg}},
        {u"TransferList.CheckingResumeData"_qs, {Color::Primer::Light::successFg, Color::Primer::Dark::successFg}},
        {u"TransferList.PausedDownloading"_qs, {Color::Primer::Light::fgMuted, Color::Primer::Dark::fgMuted}},
        {u"TransferList.PausedUploading"_qs, {Color::Primer::Light::doneFg, Color::Primer::Dark::doneFg}},
        {u"TransferList.Moving"_qs, {Color::Primer::Light::successFg, Color::Primer::Dark::successFg}},
        {u"TransferList.MissingFiles"_qs, {Color::Primer::Light::dangerFg, Color::Primer::Dark::dangerFg}},
        {u"TransferList.Error"_qs, {Color::Primer::Light::dangerFg, Color::Primer::Dark::dangerFg}}
    };
}

inline QSet<QString> defaultUIThemeIcons()
{
    return {
        u"application-exit"_qs,
        u"application-rss"_qs,
        u"application-url"_qs,
        u"browser-cookies"_qs,
        u"chart-line"_qs,
        u"checked-completed"_qs,
        u"configure"_qs,
        u"connected"_qs,
        u"dialog-warning"_qs,
        u"directory"_qs,
        u"disconnected"_qs,
        u"download"_qs,
        u"downloading"_qs,
        u"edit-clear"_qs,
        u"edit-copy"_qs,
        u"edit-find"_qs,
        u"edit-rename"_qs,
        u"error"_qs,
        u"fileicon"_qs,
        u"filter-active"_qs,
        u"filter-all"_qs,
        u"filter-inactive"_qs,
        u"filter-stalled"_qs,
        u"firewalled"_qs,
        u"folder-documents"_qs,
        u"folder-new"_qs,
        u"folder-remote"_qs,
        u"force-recheck"_qs,
        u"go-bottom"_qs,
        u"go-down"_qs,
        u"go-top"_qs,
        u"go-up"_qs,
        u"hash"_qs,
        u"help-about"_qs,
        u"help-contents"_qs,
        u"insert-link"_qs,
        u"ip-blocked"_qs,
        u"list-add"_qs,
        u"list-remove"_qs,
        u"loading"_qs,
        u"mail-inbox"_qs,
        u"name"_qs,
        u"network-connect"_qs,
        u"network-server"_qs,
        u"object-locked"_qs,
        u"peers"_qs,
        u"peers-add"_qs,
        u"peers-remove"_qs,
        u"plugins"_qs,
        u"preferences-advanced"_qs,
        u"preferences-bittorrent"_qs,
        u"preferences-desktop"_qs,
        u"preferences-webui"_qs,
        u"qbittorrent-tray"_qs,
        u"qbittorrent-tray-dark"_qs,
        u"qbittorrent-tray-light"_qs,
        u"queued"_qs,
        u"ratio"_qs,
        u"reannounce"_qs,
        u"security-high"_qs,
        u"security-low"_qs,
        u"set-location"_qs,
        u"slow"_qs,
        u"slow_off"_qs,
        u"speedometer"_qs,
        u"stalledDL"_qs,
        u"stalledUP"_qs,
        u"stopped"_qs,
        u"system-log-out"_qs,
        u"tags"_qs,
        u"task-complete"_qs,
        u"task-reject"_qs,
        u"torrent-creator"_qs,
        u"torrent-magnet"_qs,
        u"torrent-start"_qs,
        u"torrent-start-forced"_qs,
        u"torrent-stop"_qs,
        u"tracker-error"_qs,
        u"tracker-warning"_qs,
        u"trackerless"_qs,
        u"trackers"_qs,
        u"upload"_qs,
        u"view-categories"_qs,
        u"view-preview"_qs,
        u"view-refresh"_qs,
        u"view-statistics"_qs,
        u"wallet-open"_qs
    };
}
