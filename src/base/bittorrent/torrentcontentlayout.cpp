/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2020-2021  Vladimir Golovnev <glassez@yandex.ru>
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

#include "torrentcontentlayout.h"

#include "base/utils/fs.h"

namespace
{
    QString removeExtension(const QString &fileName)
    {
        const QString extension = Utils::Fs::fileExtension(fileName);
        return extension.isEmpty()
                ? fileName
                : fileName.chopped(extension.size() + 1);
    }
}

BitTorrent::TorrentContentLayout BitTorrent::detectContentLayout(const QStringList &filePaths)
{
    const QString rootFolder = Utils::Fs::findRootFolder(filePaths);
    return (rootFolder.isEmpty()
            ? TorrentContentLayout::NoSubfolder
            : TorrentContentLayout::Subfolder);
}

void BitTorrent::applyContentLayout(QStringList &filePaths, const BitTorrent::TorrentContentLayout contentLayout, const QString &rootFolder)
{
    Q_ASSERT(!filePaths.isEmpty());

    switch (contentLayout)
    {
    case TorrentContentLayout::Subfolder:
        if (Utils::Fs::findRootFolder(filePaths).isEmpty())
            Utils::Fs::addRootFolder(filePaths, !rootFolder.isEmpty() ? rootFolder : removeExtension(filePaths.at(0)));
        break;

    case TorrentContentLayout::NoSubfolder:
        Utils::Fs::stripRootFolder(filePaths);
        break;

    default:
        break;
    }
}
