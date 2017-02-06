/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017  Tim Delaney <timothy.c.delaney@gmail.com>
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2011  Christophe Dumez <chris@qbittorrent.org>
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

#ifndef GUIICONPROVIDER_H
#define GUIICONPROVIDER_H

#include <QFlags>

#include "base/iconprovider.h"

template <class T, class U> class QHash;
class QIcon;
template <class T, class U> class QPair;
class QString;

namespace BitTorrent
{
    class TorrentState;
}

class GuiIconProvider: public IconProvider
{
    Q_DISABLE_COPY(GuiIconProvider)
    Q_OBJECT

public:

    enum ThemeFlag
    {
        SystemTheme = 1,
        DarkTheme = 2,
    };

    Q_DECLARE_FLAGS(ThemeFlags, ThemeFlag)

    static void initInstance();
    static GuiIconProvider *instance();

    ThemeFlags getThemeFlags();
    void updateTheme();

    QIcon getIcon(const QString &iconId);
    QIcon getIcon(const QString &iconId, const QString &fallback);
    QIcon getFlagIcon(const QString &countryIsoCode);
    QIcon getStatusIcon(const QString &iconId);
    QIcon getStatusIcon(const QString &iconId, const BitTorrent::TorrentState &state);
    QString getIconPath(const QString &iconId);

signals:
    void themeChanged();

private slots:
    void configure(bool firstRun = false);

private:
    explicit GuiIconProvider(QObject *parent = 0);
    ~GuiIconProvider();
    QIcon getStatusIcon(const QString &path, const BitTorrent::TorrentState &state, bool ignoreState);
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC))
    QIcon generateDifferentSizes(const QIcon &icon);
#endif

    ThemeFlags m_themeFlags;
    QHash<QPair<QString, BitTorrent::TorrentState>, QIcon> *m_iconCache;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(GuiIconProvider::ThemeFlags)

#endif // GUIICONPROVIDER_H
