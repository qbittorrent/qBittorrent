/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017  Mike Tzou
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

#include "utils.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QPixmapCache>
#include <QScreen>
#include <QStyle>
#include <QWidget>
#include <QWindow>

void Utils::Gui::resize(QWidget *widget, const QSize &newSize)
{
    if (newSize.isValid())
        widget->resize(newSize);
    else  // depends on screen DPI
        widget->resize(widget->size() * screenScalingFactor(widget));
}

qreal Utils::Gui::screenScalingFactor(const QWidget *widget)
{
    if (!widget)
        return 1;

#ifdef Q_OS_WIN
    const int screen = qApp->desktop()->screenNumber(widget);
    return (QApplication::screens()[screen]->logicalDotsPerInch() / 96);
#elif defined(Q_OS_MAC)
    return 1;
#else
#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
    return widget->devicePixelRatioF();
#else
    return widget->devicePixelRatio();
#endif
#endif // Q_OS_WIN
}

QPixmap Utils::Gui::scaledPixmap(const QIcon &icon, const QWidget *widget, const int height)
{
    Q_ASSERT(height > 0);
    const int scaledHeight = height * Utils::Gui::screenScalingFactor(widget);
    return icon.pixmap(scaledHeight);
}

QPixmap Utils::Gui::scaledPixmap(const QString &path, const QWidget *widget, const int height)
{
    const QPixmap pixmap(path);
    const int scaledHeight = ((height > 0) ? height : pixmap.height()) * Utils::Gui::screenScalingFactor(widget);
    return pixmap.scaledToHeight(scaledHeight, Qt::SmoothTransformation);
}

QPixmap Utils::Gui::scaledPixmapSvg(const QString &path, const QWidget *widget, const int baseHeight)
{
    const int scaledHeight = baseHeight * Utils::Gui::screenScalingFactor(widget);
    const QString normalizedKey = path + '@' + QString::number(scaledHeight);

    QPixmap pm;
    QPixmapCache cache;
    if (!cache.find(normalizedKey, &pm)) {
        pm = QIcon(path).pixmap(scaledHeight);
        cache.insert(normalizedKey, pm);
    }
    return pm;
}

QSize Utils::Gui::smallIconSize(const QWidget *widget)
{
    // Get DPI scaled icon size (device-dependent), see QT source
    // under a 1080p screen is usually 16x16
    const int s = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, widget);
    return QSize(s, s);
}

QSize Utils::Gui::mediumIconSize(const QWidget *widget)
{
    // under a 1080p screen is usually 24x24
    return ((smallIconSize(widget) + largeIconSize(widget)) / 2);
}

QSize Utils::Gui::largeIconSize(const QWidget *widget)
{
    // Get DPI scaled icon size (device-dependent), see QT source
    // under a 1080p screen is usually 32x32
    const int s = QApplication::style()->pixelMetric(QStyle::PM_LargeIconSize, nullptr, widget);
    return QSize(s, s);
}
