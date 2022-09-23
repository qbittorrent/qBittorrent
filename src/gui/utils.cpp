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
#include <Shellapi.h>
#endif

#include <QApplication>
#include <QColor>
#include <QDesktopServices>
#include <QIcon>
#include <QPalette>
#include <QPixmap>
#include <QPixmapCache>
#include <QPoint>
#include <QProcess>
#include <QRegularExpression>
#include <QScreen>
#include <QSize>
#include <QStyle>
#include <QThread>
#include <QUrl>
#include <QWidget>
#include <QWindow>

#include "base/global.h"
#include "base/path.h"
#include "base/utils/fs.h"
#include "base/utils/version.h"

bool Utils::Gui::isDarkTheme()
{
    const QPalette palette = qApp->palette();
    const QColor &color = palette.color(QPalette::Active, QPalette::Base);
    return (color.lightness() < 127);
}

QPixmap Utils::Gui::scaledPixmap(const QIcon &icon, const QWidget *widget, const int height)
{
    Q_UNUSED(widget);  // TODO: remove it
    Q_ASSERT(height > 0);

    return icon.pixmap(height);
}

QPixmap Utils::Gui::scaledPixmap(const Path &path, const QWidget *widget, const int height)
{
    Q_UNUSED(widget);
    Q_ASSERT(height >= 0);

    const QPixmap pixmap {path.data()};
    return (height == 0) ? pixmap : pixmap.scaledToHeight(height, Qt::SmoothTransformation);
}

QPixmap Utils::Gui::scaledPixmapSvg(const Path &path, const QWidget *widget, const int height)
{
    // (workaround) svg images require the use of `QIcon()` to load and scale losslessly,
    // otherwise other image classes will convert it to pixmap first and follow-up scaling will become lossy.

    Q_UNUSED(widget);
    Q_ASSERT(height > 0);

    const QString cacheKey = path.data() + u'@' + QString::number(height);

    QPixmap pixmap;
    QPixmapCache cache;
    if (!cache.find(cacheKey, &pixmap))
    {
        pixmap = QIcon(path.data()).pixmap(height);
        cache.insert(cacheKey, pixmap);
    }
    return pixmap;
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
    const QUrl url = path.data().startsWith(u"//")
        ? QUrl(u"file:" + path.data())
        : QUrl::fromLocalFile(path.data());

#ifdef Q_OS_WIN
    auto *thread = QThread::create([path]()
    {
        if (SUCCEEDED(::CoInitializeEx(NULL, (COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))))
        {
            const std::wstring pathWStr = path.toString().toStdWString();

            ::ShellExecuteW(nullptr, nullptr, pathWStr.c_str(), nullptr, nullptr, SW_SHOWNORMAL);

            ::CoUninitialize();
        }
    });
    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
#else
    QDesktopServices::openUrl(url);
#endif
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
    auto *thread = QThread::create([path]()
    {
        if (SUCCEEDED(::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)))
        {
            const std::wstring pathWStr = path.toString().toStdWString();
            PIDLIST_ABSOLUTE pidl = ::ILCreateFromPathW(pathWStr.c_str());
            if (pidl)
            {
                ::SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
                ::ILFree(pidl);
            }

            ::CoUninitialize();
        }
    });
    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
#elif defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    QProcess proc;
    proc.start(u"xdg-mime"_qs, {u"query"_qs, u"default"_qs, u"inode/directory"_qs});
    proc.waitForFinished();
    const auto output = QString::fromLocal8Bit(proc.readLine().simplified());
    if ((output == u"dolphin.desktop") || (output == u"org.kde.dolphin.desktop"))
    {
        proc.startDetached(u"dolphin"_qs, {u"--select"_qs, path.toString()});
    }
    else if ((output == u"nautilus.desktop") || (output == u"org.gnome.Nautilus.desktop")
                 || (output == u"nautilus-folder-handler.desktop"))
    {
        proc.start(u"nautilus"_qs, {u"--version"_qs});
        proc.waitForFinished();
        const auto nautilusVerStr = QString::fromLocal8Bit(proc.readLine()).remove(QRegularExpression(u"[^0-9.]"_qs));
        using NautilusVersion = Utils::Version<3>;
        if (NautilusVersion::fromString(nautilusVerStr, {1, 0, 0}) > NautilusVersion(3, 28, 0))
            proc.startDetached(u"nautilus"_qs, {(Fs::isDir(path) ? path.parentPath() : path).toString()});
        else
            proc.startDetached(u"nautilus"_qs, {u"--no-desktop"_qs, (Fs::isDir(path) ? path.parentPath() : path).toString()});
    }
    else if (output == u"nemo.desktop")
    {
        proc.startDetached(u"nemo"_qs, {u"--no-desktop"_qs, (Fs::isDir(path) ? path.parentPath() : path).toString()});
    }
    else if ((output == u"konqueror.desktop") || (output == u"kfmclient_dir.desktop"))
    {
        proc.startDetached(u"konqueror"_qs, {u"--select"_qs, path.toString()});
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
