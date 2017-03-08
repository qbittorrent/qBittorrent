/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017  Eugene Shalygin <eugene.shalygin@gmail.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

#ifndef QBT_THEME_COLORTHEME_H
#define QBT_THEME_COLORTHEME_H

#include <map>
#include <memory>

#include <QMetaEnum>
#include <QString>

#include <libtorrent/version.hpp>

#include "themecommon.h"

class QColor;
class QSettings;

namespace BitTorrent
{
    enum class TorrentState;
}

namespace Log
{
    enum MsgType : int;
}

namespace Theme
{
    enum class DownloadProgressBarElement
    {
        Background,
        Border,
        Complete,
        Incomplete
    };

    /*! \brief qBittorrent color theme
     *
     * Contains all colors that are needed to render a qBt UI, either using widgets or Web
     */
    class ColorTheme : public VisualTheme
    {
        Q_GADGET

    public:
        /**
        * @brief Elements of the color theme
        *
        * @note Underscores in the element names mark positions of group separators, i.e. will be
        * replaced by '/' when saving to QSettings
        */
        enum class Element
        {
            TorrentState_Unknown,
            TorrentState_ForcedDownloading,
            TorrentState_Downloading,
            TorrentState_DownloadingMetadata,
            TorrentState_Allocating,
            TorrentState_StalledDownloading,
            TorrentState_ForcedUploading,
            TorrentState_Uploading,
            TorrentState_StalledUploading,
    #if LIBTORRENT_VERSION_NUM < 10100
            TorrentState_QueuedForChecking,
    #endif
            TorrentState_CheckingResumeData,
            TorrentState_QueuedDownloading,
            TorrentState_QueuedUploading,
            TorrentState_CheckingUploading,
            TorrentState_CheckingDownloading,
            TorrentState_PausedDownloading,
            TorrentState_PausedUploading,
            TorrentState_MissingFiles,
            TorrentState_Error,

            MsgType_ALL,
            MsgType_NORMAL,
            MsgType_INFO,
            MsgType_WARNING,
            MsgType_CRITICAL,

            DownloadProgressBarElement_Background,
            DownloadProgressBarElement_Border,
            DownloadProgressBarElement_Complete,
            DownloadProgressBarElement_Incomplete
        };

        Q_ENUM(Element)

        virtual QColor color(Element element) const = 0;

        QColor torrentStateColor(const BitTorrent::TorrentState state) const;
        QColor logMessageColor(const Log::MsgType messageType) const;
        QColor downloadProgressBarColor(const DownloadProgressBarElement element) const;

        static const ColorTheme &current();
    };
}

#endif // QBT_THEME_COLORTHEME_H
