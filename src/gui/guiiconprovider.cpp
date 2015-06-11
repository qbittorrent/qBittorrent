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

#include "guiiconprovider.h"
#include "core/preferences.h"

#include <QIcon>
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC))
#include <QDir>
#include <QFile>
#endif

GuiIconProvider::GuiIconProvider(QObject *parent)
    : IconProvider(parent)
{
    configure();
    connect(Preferences::instance(), SIGNAL(changed()), SLOT(configure()));
}

GuiIconProvider::~GuiIconProvider() {}

void GuiIconProvider::initInstance()
{
    if (!m_instance)
        m_instance = new GuiIconProvider;
}

GuiIconProvider *GuiIconProvider::instance()
{
    return static_cast<GuiIconProvider *>(m_instance);
}

QIcon GuiIconProvider::getIcon(const QString &iconId)
{
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC))
    if (m_useSystemTheme) {
        QIcon icon = QIcon::fromTheme(iconId, QIcon(IconProvider::getIconPath(iconId)));
        icon = generateDifferentSizes(icon);
        return icon;
    }
#endif
    return QIcon(IconProvider::getIconPath(iconId));
}

// Makes sure the icon is at least available in 16px and 24px size
// It scales the icon from the theme if necessary
// Otherwise, the UI looks broken if the icon is not available
// in the correct size.
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC))
QIcon GuiIconProvider::generateDifferentSizes(const QIcon &icon)
{
    QIcon newIcon;
    QList<QSize> requiredSizes;
    requiredSizes << QSize(16, 16) << QSize(24, 24) << QSize(32, 32);
    QList<QIcon::Mode> modes;
    modes << QIcon::Normal << QIcon::Active << QIcon::Selected << QIcon::Disabled;
    foreach (const QSize &size, requiredSizes) {
        foreach (QIcon::Mode mode, modes) {
            QPixmap pixoff = icon.pixmap(size, mode, QIcon::Off);
            if (pixoff.height() > size.height())
                pixoff = pixoff.scaled(size, Qt::KeepAspectRatio,  Qt::SmoothTransformation);
            newIcon.addPixmap(pixoff, mode, QIcon::Off);
            QPixmap pixon = icon.pixmap(size, mode, QIcon::On);
            if (pixon.height() > size.height())
                pixon = pixoff.scaled(size, Qt::KeepAspectRatio,  Qt::SmoothTransformation);
            newIcon.addPixmap(pixon, mode, QIcon::On);
        }
    }

    return newIcon;
}
#endif

QString GuiIconProvider::getIconPath(const QString &iconId)
{
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC))
    if (m_useSystemTheme) {
        QString path = QDir::temp().absoluteFilePath(iconId + ".png");
        if (!QFile::exists(path)) {
            const QIcon icon = QIcon::fromTheme(iconId);
            if (!icon.isNull())
                icon.pixmap(32).save(path);
            else
                path = IconProvider::getIconPath(iconId);
        }

        return path;
    }
#endif
    return IconProvider::getIconPath(iconId);
}


void GuiIconProvider::configure()
{
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC))
    m_useSystemTheme = Preferences::instance()->useSystemIconTheme();
#endif
}
