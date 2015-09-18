/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015 Eugene Shalygin
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

#include "torrentmodel_p.h"

#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC))
#include "base/preferences.h"
#include "utils/kdecolorscheme.h"
#include "utils/colorutils.h"
#endif

#include <QApplication>
#include <QPalette>
#include <QProcessEnvironment>

Q_GLOBAL_STATIC(TorrentStateColors, torrentStateColors)

TorrentStateColors * TorrentStateColors::instance(){
#if QT_VERSION >= 0x050000
    return torrentStateColors;
#else
    return torrentStateColors();
#endif
}

TorrentStateColors::TorrentStateColors()
{
    setupColors();
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC)) // there is no need to reload colors if we do not support it
    connect(Preferences::instance(), SIGNAL(changed()), this, SLOT(setupColors()));
#endif
}

void TorrentStateColors::setupColors()
{
    QScopedPointer<StateColorsProvider> provider;
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC))
    if (Preferences::instance()->useSystemColorTheme()) {
        QProcessEnvironment env(QProcessEnvironment::systemEnvironment());
        if (env.contains(QLatin1String("XDG_CURRENT_DESKTOP"))) {
            QString desktop = env.value(QLatin1String("XDG_CURRENT_DESKTOP"));
            if (desktop == QLatin1String("KDE"))
                provider.reset(new KDEColorsProvider());
        }
    }
#endif
    if (!provider)
        provider.reset(new DefaultColorsProvider());
    provider->setColors(stateColors);
}

bool StateColorsProvider::isDarkTheme()
{
    QPalette pal = QApplication::palette();
    // QPalette::Base is used for the background of the Treeview
    QColor color = pal.color(QPalette::Active, QPalette::Base);
    return (color.lightnessF() < 0.5);
}

void DefaultColorsProvider::setColors(TorrentStateColorsMap &m)
{
    using BitTorrent::TorrentState;
    // Color names taken from http://cloford.com/resources/colours/500col.htm
    bool dark = isDarkTheme();

    m[TorrentState::Downloading] =
    m[TorrentState::ForcedDownloading] =
    m[TorrentState::DownloadingMetadata] =
        !dark ? QColor(34, 139, 34) : // Forest Green
                QColor(50, 205, 50);  // Lime Green

    m[TorrentState::Allocating] =
    m[TorrentState::StalledDownloading] =
    m[TorrentState::StalledUploading] =
        !dark ? QColor(0, 0, 0) :      // Black
                QColor(204, 204, 204); // Gray 80

    m[TorrentState::Uploading] =
    m[TorrentState::ForcedUploading] =
        !dark ? QColor(65, 105, 225) : // Royal Blue
                QColor(99, 184, 255);  // Steel Blue 1

    m[TorrentState::PausedDownloading] =
        QColor(250, 128, 114); // Salmon

    m[TorrentState::PausedUploading] =
        !dark ? QColor(0, 0, 139) :   // Dark Blue
                QColor(79, 148, 205); // Steel Blue 3

    m[TorrentState::Error] =
    m[TorrentState::MissingFiles] =
        QColor(255, 0, 0); // red

    m[TorrentState::QueuedDownloading] =
    m[TorrentState::QueuedUploading] =
    m[TorrentState::CheckingDownloading] =
    m[TorrentState::CheckingUploading] =
    m[TorrentState::QueuedForChecking] =
    m[TorrentState::CheckingResumeData] =
        !dark ? QColor(0, 128, 128) : // Teal
                QColor(0, 205, 205);  // Cyan 3

    m[TorrentState::Unknown] =
        QColor(255, 0, 0); // red
}

#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC))
namespace
{
    QColor lighten(const QColor &c)
    {
        return Utils::Color::lighten(c, 0.25);
    }

    QColor darken(const QColor &c)
    {
        return Utils::Color::darken(c, 0.25);
    }

    typedef QColor (*ColorCorrectionFunc)(const QColor&);
}

void KDEColorsProvider::setColors(TorrentStateColorsMap &m)
{
    using BitTorrent::TorrentState;

    const KDEColorScheme* const scheme = KDEColorScheme::instance();
    bool dark = isDarkTheme();

    ColorCorrectionFunc towardsBackground  = dark ? &darken  : &lighten;
    ColorCorrectionFunc awayFromBackground = dark ? &lighten : &darken;

    m[TorrentState::Downloading] =
        scheme->color(KDEColorScheme::ForegroundPositive);
    m[TorrentState::ForcedDownloading] =
        awayFromBackground(scheme->color(KDEColorScheme::ForegroundPositive));

    m[TorrentState::DownloadingMetadata] =
         towardsBackground(scheme->color(KDEColorScheme::ForegroundActive));

    m[TorrentState::Allocating] =
         scheme->color(KDEColorScheme::ForegroundInactive);

    m[TorrentState::StalledDownloading] =
    m[TorrentState::StalledUploading] =
        scheme->color(KDEColorScheme::ForegroundInactive);

    m[TorrentState::Uploading] =
        scheme->color(KDEColorScheme::ForegroundNeutral);
    m[TorrentState::ForcedUploading] =
        awayFromBackground(scheme->color(KDEColorScheme::ForegroundNeutral));

    m[TorrentState::PausedDownloading] =
        scheme->color(KDEColorScheme::ForegroundPositive, QPalette::Disabled);

    m[TorrentState::PausedUploading] =
        scheme->color(KDEColorScheme::ForegroundNeutral, QPalette::Disabled);

    m[TorrentState::Error] =
    m[TorrentState::MissingFiles] =
        scheme->color(KDEColorScheme::ForegroundNegative);

    m[TorrentState::QueuedDownloading] =
        scheme->color(KDEColorScheme::ForegroundPositive, QPalette::Inactive);

    m[TorrentState::QueuedUploading] =
        scheme->color(KDEColorScheme::ForegroundNeutral, QPalette::Inactive);

    m[TorrentState::CheckingDownloading] =
    m[TorrentState::CheckingUploading] =
    m[TorrentState::QueuedForChecking] =
    m[TorrentState::CheckingResumeData] =
        scheme->color(KDEColorScheme::ForegroundActive, QPalette::Inactive);

    m[TorrentState::Unknown] =
        scheme->color(KDEColorScheme::ForegroundNegative, QPalette::Disabled);
}

#endif
