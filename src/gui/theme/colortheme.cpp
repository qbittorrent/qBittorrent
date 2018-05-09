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

#include "colortheme.h"

#include <QColor>
#include <QLoggingCategory>
#include <QMetaEnum>
#include <QSettings>

#include "base/bittorrent/torrenthandle.h"
#include "base/logger.h"
#include "colorprovider_p.h"
#include "colortheme_impl.h"
#include "themeprovider.h"

namespace
{
    Theme::ColorTheme::Element toThemeElement(Theme::DownloadProgressBarElement element)
    {
        switch (element) {
        case Theme::DownloadProgressBarElement::Background:
            return Theme::ColorTheme::Element::DownloadProgressBarElement_Background;
        case Theme::DownloadProgressBarElement::Border:
            return Theme::ColorTheme::Element::DownloadProgressBarElement_Border;
        case Theme::DownloadProgressBarElement::Complete:
            return Theme::ColorTheme::Element::DownloadProgressBarElement_Complete;
        case Theme::DownloadProgressBarElement::Incomplete:
            return Theme::ColorTheme::Element::DownloadProgressBarElement_Incomplete;
        default:
            throw std::logic_error("Unexpected DownloadProgressBarElement value");
        }
    }

    Theme::ColorTheme::Element toThemeElement(Log::MsgType messageType)
    {
        switch (messageType) {
        case Log::MsgType::ALL:
            return Theme::ColorTheme::Element::MsgType_ALL;
        case Log::MsgType::NORMAL:
            return Theme::ColorTheme::Element::MsgType_NORMAL;
        case Log::MsgType::INFO:
            return Theme::ColorTheme::Element::MsgType_INFO;
        case Log::MsgType::WARNING:
            return Theme::ColorTheme::Element::MsgType_WARNING;
        case Log::MsgType::CRITICAL:
            return Theme::ColorTheme::Element::MsgType_CRITICAL;
        default:
            throw std::logic_error("Unexpected Log::MsgType value");
        }
    }

    Theme::ColorTheme::Element toThemeElement(BitTorrent::TorrentState state)
    {
        switch (state) {
        case BitTorrent::TorrentState::Unknown:
            return Theme::ColorTheme::Element::TorrentState_Unknown;
        case BitTorrent::TorrentState::Error:
            return Theme::ColorTheme::Element::TorrentState_Error;
        case BitTorrent::TorrentState::MissingFiles:
            return Theme::ColorTheme::Element::TorrentState_MissingFiles;
        case BitTorrent::TorrentState::Uploading:
            return Theme::ColorTheme::Element::TorrentState_Uploading;
        case BitTorrent::TorrentState::PausedUploading:
            return Theme::ColorTheme::Element::TorrentState_PausedUploading;
        case BitTorrent::TorrentState::QueuedUploading:
            return Theme::ColorTheme::Element::TorrentState_QueuedUploading;
        case BitTorrent::TorrentState::StalledUploading:
            return Theme::ColorTheme::Element::TorrentState_StalledUploading;
        case BitTorrent::TorrentState::CheckingUploading:
            return Theme::ColorTheme::Element::TorrentState_CheckingUploading;
        case BitTorrent::TorrentState::ForcedUploading:
            return Theme::ColorTheme::Element::TorrentState_ForcedUploading;
        case BitTorrent::TorrentState::Allocating:
            return Theme::ColorTheme::Element::TorrentState_Allocating;
        case BitTorrent::TorrentState::Downloading:
            return Theme::ColorTheme::Element::TorrentState_Downloading;
        case BitTorrent::TorrentState::DownloadingMetadata:
            return Theme::ColorTheme::Element::TorrentState_DownloadingMetadata;
        case BitTorrent::TorrentState::PausedDownloading:
            return Theme::ColorTheme::Element::TorrentState_PausedDownloading;
        case BitTorrent::TorrentState::QueuedDownloading:
            return Theme::ColorTheme::Element::TorrentState_QueuedDownloading;
        case BitTorrent::TorrentState::StalledDownloading:
            return Theme::ColorTheme::Element::TorrentState_StalledDownloading;
        case BitTorrent::TorrentState::CheckingDownloading:
            return Theme::ColorTheme::Element::TorrentState_CheckingDownloading;
        case BitTorrent::TorrentState::ForcedDownloading:
            return Theme::ColorTheme::Element::TorrentState_ForcedDownloading;
    #if LIBTORRENT_VERSION_NUM < 10100
        case BitTorrent::TorrentState::QueuedForChecking:
            return Theme::ColorTheme::Element::TorrentState_QueuedForChecking;
    #endif
        case BitTorrent::TorrentState::CheckingResumeData:
            return Theme::ColorTheme::Element::TorrentState_CheckingResumeData;
        default:
            throw std::logic_error("Unexpected TorrentState value");
        }
    }
}

const Theme::ColorTheme &Theme::ColorTheme::current()
{
    return ThemeProvider::instance().colorTheme();
}

QColor Theme::ColorTheme::downloadProgressBarColor(const DownloadProgressBarElement elem) const
{
    return color(toThemeElement(elem));
}

QColor Theme::ColorTheme::logMessageColor(const Log::MsgType messageType) const
{
    return color(toThemeElement(messageType));
}

QColor Theme::ColorTheme::torrentStateColor(const BitTorrent::TorrentState state) const
{
    return color(toThemeElement(state));
}

Theme::SerializableColorTheme::SerializableColorTheme(const QString &name)
    : BaseSerializableTheme(name, elementNames())
{
}

void Theme::SerializableColorTheme::applicationPaletteChanged()
{
    updateCache();
}

Theme::SerializableColorTheme::BaseSerializableTheme::NamesMap
Theme::SerializableColorTheme::elementNames()
{
    const QMetaEnum meta = QMetaEnum::fromType<Element>();
    NamesMap res;
    for (int i = 0; i < meta.keyCount(); ++i) {
        QByteArray key = meta.key(i);
        Q_ASSERT(key.indexOf('_') > 0);
        key.replace('_', '/');
        res.insert({key, meta.value(i)});
    }
    return res;
}

const Theme::ThemeInfo &Theme::SerializableColorTheme::info() const
{
    return BaseSerializableTheme::info();
}

QColor Theme::SerializableColorTheme::color(Theme::ColorTheme::Element element) const
{
    return this->element(element);
}
