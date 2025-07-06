/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2025  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2020  Prince Gupta <jagannatharjun11@gmail.com>
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

#include "progressbarpainter.h"

#include <QPainter>
#include <QPalette>
#include <QStyleOptionProgressBar>
#include <QStyleOptionViewItem>

#if (defined(Q_OS_WIN) || defined(Q_OS_MACOS))
#include <QProxyStyle>
#endif

#include "base/global.h"
#include "base/bittorrent/torrent.h"
#include "gui/uithememanager.h"

namespace
{
    QHash<BitTorrent::TorrentState, QColor> torrentStateColorsFromUITheme()
    {
        struct TorrentStateColorDescriptor
        {
            const BitTorrent::TorrentState state;
            const QString id;
        };

        const TorrentStateColorDescriptor colorDescriptors[] =
        {
            {BitTorrent::TorrentState::Downloading, u"TransferList.Downloading"_s},
            {BitTorrent::TorrentState::StalledDownloading, u"TransferList.StalledDownloading"_s},
            {BitTorrent::TorrentState::DownloadingMetadata, u"TransferList.DownloadingMetadata"_s},
            {BitTorrent::TorrentState::ForcedDownloadingMetadata, u"TransferList.ForcedDownloadingMetadata"_s},
            {BitTorrent::TorrentState::ForcedDownloading, u"TransferList.ForcedDownloading"_s},
            {BitTorrent::TorrentState::Uploading, u"TransferList.Uploading"_s},
            {BitTorrent::TorrentState::StalledUploading, u"TransferList.StalledUploading"_s},
            {BitTorrent::TorrentState::ForcedUploading, u"TransferList.ForcedUploading"_s},
            {BitTorrent::TorrentState::QueuedDownloading, u"TransferList.QueuedDownloading"_s},
            {BitTorrent::TorrentState::QueuedUploading, u"TransferList.QueuedUploading"_s},
            {BitTorrent::TorrentState::CheckingDownloading, u"TransferList.CheckingDownloading"_s},
            {BitTorrent::TorrentState::CheckingUploading, u"TransferList.CheckingUploading"_s},
            {BitTorrent::TorrentState::CheckingResumeData, u"TransferList.CheckingResumeData"_s},
            {BitTorrent::TorrentState::StoppedDownloading, u"TransferList.StoppedDownloading"_s},
            {BitTorrent::TorrentState::StoppedUploading, u"TransferList.StoppedUploading"_s},
            {BitTorrent::TorrentState::Moving, u"TransferList.Moving"_s},
            {BitTorrent::TorrentState::MissingFiles, u"TransferList.MissingFiles"_s},
            {BitTorrent::TorrentState::Error, u"TransferList.Error"_s}
        };

        QHash<BitTorrent::TorrentState, QColor> colors;
        for (const TorrentStateColorDescriptor &colorDescriptor : colorDescriptors)
        {
            const QColor themeColor = UIThemeManager::instance()->getColor(colorDescriptor.id);
            colors.insert(colorDescriptor.state, themeColor);
        }
        return colors;
    }
}

ProgressBarPainter::ProgressBarPainter(QObject *parent)
    : QObject(parent)
{
#if (defined(Q_OS_WIN) || defined(Q_OS_MACOS))
    auto *fusionStyle = new QProxyStyle(u"fusion"_s);
    fusionStyle->setParent(&m_dummyProgressBar);
    m_dummyProgressBar.setStyle(fusionStyle);
#endif

    loadUIThemeResources();
    applyUITheme();
    connect(UIThemeManager::instance(), &UIThemeManager::themeChanged, this, &ProgressBarPainter::applyUITheme);
}

void ProgressBarPainter::paint(QPainter *painter, const QStyleOptionViewItem &option, const QString &text, const int progress, const BitTorrent::TorrentState torrentState) const
{
    QStyleOptionProgressBar styleOption;
    styleOption.initFrom(&m_dummyProgressBar);
    // QStyleOptionProgressBar fields
    styleOption.maximum = 100;
    styleOption.minimum = 0;
    styleOption.progress = progress;
    styleOption.text = text;
    styleOption.textVisible = true;
    // QStyleOption fields
    styleOption.rect = option.rect;
    // Qt 6 requires QStyle::State_Horizontal to be set for correctly drawing horizontal progress bar
    styleOption.state = option.state | QStyle::State_Horizontal;

    const bool isEnabled = option.state.testFlag(QStyle::State_Enabled);
    styleOption.palette.setCurrentColorGroup(isEnabled ? QPalette::Active : QPalette::Disabled);

    if (torrentState != BitTorrent::TorrentState::Unknown)
    {
        styleOption.palette.setColor(QPalette::Highlight, m_stateThemeColors.value(torrentState));
    }
    else if (m_chunkColor.isValid())
    {
        styleOption.palette.setColor(QPalette::Highlight, m_chunkColor);
    }

    painter->save();
    const QStyle *style = m_dummyProgressBar.style();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);
    style->drawControl(QStyle::CE_ProgressBar, &styleOption, painter, &m_dummyProgressBar);
    painter->restore();
}

void ProgressBarPainter::applyUITheme()
{
    m_chunkColor = UIThemeManager::instance()->getColor(u"ProgressBar"_s);
}

void ProgressBarPainter::loadUIThemeResources()
{
    m_stateThemeColors = torrentStateColorsFromUITheme();
}

