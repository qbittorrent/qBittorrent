/*
 * Bittorrent Client using Qt and libtorrent.
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

#include "core/iconprovider.h"

class QIcon;

class GuiIconProvider : public IconProvider
{
    Q_DISABLE_COPY(GuiIconProvider)
    Q_OBJECT

public:
    static void initInstance();
    static GuiIconProvider *instance();

    QIcon getIcon(const QString &iconId);
    QString getIconPath(const QString &iconId);

private slots:
    void configure();

private:
    explicit GuiIconProvider(QObject *parent = 0);
    ~GuiIconProvider();
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC))
    QIcon generateDifferentSizes(const QIcon &icon);

    bool m_useSystemTheme;
#endif
};

#endif // GUIICONPROVIDER_H
