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

#ifdef Q_OS_WIN
#include <Objbase.h>
#include <Shlobj.h>
#endif

#include <QApplication>
#include <QDesktopServices>
#include <QFileInfo>
#include <QIcon>
#include <QPixmap>
#include <QPixmapCache>
#include <QPoint>
#include <QProcess>
#include <QRegularExpression>
#include <QScreen>
#include <QStyle>
#include <QUrl>
#include <QWidget>
#include <QWindow>

#include "base/path.h"
#include "base/utils/fs.h"
#include "base/utils/version.h"

void Utils::Gui::resize(QWidget *widget, const QSize &newSize)
{
    if (newSize.isValid())
        widget->resize(newSize);
    else  // depends on screen DPI
        widget->resize(widget->size() * screenScalingFactor(widget));
}

qreal Utils::Gui::screenScalingFactor(const QWidget *widget)
{
    Q_UNUSED(widget);
    return 1;
}

QPixmap Utils::Gui::scaledPixmap(const QIcon &icon, const QWidget *widget, const int height)
{
    Q_ASSERT(height > 0);
    const int scaledHeight = height * Utils::Gui::screenScalingFactor(widget);
    return icon.pixmap(scaledHeight);
}

QPixmap Utils::Gui::scaledPixmap(const Path &path, const QWidget *widget, const int height)
{
    const QPixmap pixmap {path.data()};
    const int scaledHeight = ((height > 0) ? height : pixmap.height()) * Utils::Gui::screenScalingFactor(widget);
    return pixmap.scaledToHeight(scaledHeight, Qt::SmoothTransformation);
}

QPixmap Utils::Gui::scaledPixmapSvg(const Path &path, const QWidget *widget, const int baseHeight)
{
    const int scaledHeight = baseHeight * Utils::Gui::screenScalingFactor(widget);
    const QString normalizedKey = path.data() + '@' + QString::number(scaledHeight);

    QPixmap pm;
    QPixmapCache cache;
    if (!cache.find(normalizedKey, &pm))
    {
        pm = QIcon(path.data()).pixmap(scaledHeight);
        cache.insert(normalizedKey, pm);
    }
    return pm;
}

QSize Utils::Gui::smallIconSize(const QWidget *widget)
{
    // Get DPI scaled icon size (device-dependent), see QT source
    // under a 1080p screen is usually 16x16
    const int s = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, widget);
    return {s, s};
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
    return {s, s};
}

QPoint Utils::Gui::screenCenter(const QWidget *w)
{
    // Returns the QPoint which the widget will be placed center on screen (where parent resides)

    if (!w)
        return {};

    QRect r = QGuiApplication::primaryScreen()->availableGeometry();
    const QPoint primaryScreenCenter {(r.x() + (r.width() - w->frameSize().width()) / 2), (r.y() + (r.height() - w->frameSize().height()) / 2)};

    const QWidget *parent = w->parentWidget();
    if (!parent)
        return primaryScreenCenter;

    const QWindow *window = parent->window()->windowHandle();
    if (!window)
        return primaryScreenCenter;

    const QScreen *screen = window->screen();
    if (!screen)
        return primaryScreenCenter;

    r = screen->availableGeometry();
    return {(r.x() + (r.width() - w->frameSize().width()) / 2), (r.y() + (r.height() - w->frameSize().height()) / 2)};
}

// Open the given path with an appropriate application
void Utils::Gui::openPath(const Path &path)
{
    // Hack to access samba shares with QDesktopServices::openUrl
    if (path.data().startsWith("//"))
        QDesktopServices::openUrl(QUrl(QString::fromLatin1("file:") + path.toString()));
    else
        QDesktopServices::openUrl(QUrl::fromLocalFile(path.data()));
}

// Open the parent directory of the given path with a file manager and select
// (if possible) the item at the given path
void Utils::Gui::openFolderSelect(const Path &path)
{
    // If the item to select doesn't exist, try to open its parent
    if (!path.exists())
    {
        openPath(path.parentPath());
        return;
    }

#ifdef Q_OS_WIN
    HRESULT hresult = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    PIDLIST_ABSOLUTE pidl = ::ILCreateFromPathW(reinterpret_cast<PCTSTR>(path.toString().utf16()));
    if (pidl)
    {
        ::SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
        ::ILFree(pidl);
    }
    if ((hresult == S_OK) || (hresult == S_FALSE))
        ::CoUninitialize();
#elif defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    QProcess proc;
    proc.start("xdg-mime", {"query", "default", "inode/directory"});
    proc.waitForFinished();
    const QString output = proc.readLine().simplified();
    if ((output == "dolphin.desktop") || (output == "org.kde.dolphin.desktop"))
    {
        proc.startDetached("dolphin", {"--select", path.toString()});
    }
    else if ((output == "nautilus.desktop") || (output == "org.gnome.Nautilus.desktop")
                 || (output == "nautilus-folder-handler.desktop"))
    {
        proc.start("nautilus", {"--version"});
        proc.waitForFinished();
        const QString nautilusVerStr = QString(proc.readLine()).remove(QRegularExpression("[^0-9.]"));
        using NautilusVersion = Utils::Version<int, 3>;
        if (NautilusVersion::tryParse(nautilusVerStr, {1, 0, 0}) > NautilusVersion {3, 28})
            proc.startDetached("nautilus", {(Fs::isDir(path) ? path.parentPath() : path).toString()});
        else
            proc.startDetached("nautilus", {"--no-desktop", (Fs::isDir(path) ? path.parentPath() : path).toString()});
    }
    else if (output == "nemo.desktop")
    {
        proc.startDetached("nemo", {"--no-desktop", (Fs::isDir(path) ? path.parentPath() : path).toString()});
    }
    else if ((output == "konqueror.desktop") || (output == "kfmclient_dir.desktop"))
    {
        proc.startDetached("konqueror", {"--select", path.toString()});
    }
    else
    {
        // "caja" manager can't pinpoint the file, see: https://github.com/qbittorrent/qBittorrent/issues/5003
        openPath(path.parentPath());
    }
#else
    openPath(path.parentPath());
#endif
}
