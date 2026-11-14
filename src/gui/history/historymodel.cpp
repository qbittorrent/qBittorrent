/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  qBittorrent contributors
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

#include "historymodel.h"

#include <cmath>

#include <QColor>
#include <QLocale>

#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/utils/misc.h"
#include "base/utils/string.h"
#include "gui/uithememanager.h"

using namespace Qt::Literals::StringLiterals;

HistoryModel::HistoryModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    reload();
}

int HistoryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_torrents.size();
}

int HistoryModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : NB_COLUMNS;
}

QVariant HistoryModel::data(const QModelIndex &index, const int role) const
{
    if (!index.isValid() || (index.row() < 0) || (index.row() >= m_torrents.size()))
        return {};

    const BitTorrent::ArchivedTorrent &torrent = m_torrents.at(index.row());

    switch (role)
    {
    case Qt::DisplayRole:
        switch (index.column())
        {
        case Name:
            return torrent.name;
        case Size:
            return torrent.hasMetadata ? Utils::Misc::friendlyUnit(torrent.totalSize) : u"-"_s;
        case Progress:
            return torrent.hasMetadata
                    ? u"%1%"_s.arg(Utils::String::fromDouble(torrent.progress * 100, 1))
                    : u"-"_s;
        case Category:
            return torrent.category;
        case Tags:
            {
                QStringList tags;
                for (const Tag &tag : torrent.tags)
                    tags.append(tag.toString());
                return tags.join(u", ");
            }
        case AddedOn:
            return torrent.addedTime.isValid()
                    ? QLocale().toString(torrent.addedTime.toLocalTime(), QLocale::ShortFormat) : u"-"_s;
        case CompletedOn:
            return torrent.completedTime.isValid()
                    ? QLocale().toString(torrent.completedTime.toLocalTime(), QLocale::ShortFormat) : u"-"_s;
        case Ratio:
            return std::isinf(torrent.ratio)
                    ? u"∞"_s : Utils::String::fromDouble(torrent.ratio, 2);
        case Downloaded:
            return Utils::Misc::friendlyUnit(torrent.totalDownload);
        case Uploaded:
            return Utils::Misc::friendlyUnit(torrent.totalUpload);
        case SavePath:
            return torrent.savePath.toString();
        default:
            break;
        }
        break;

    case Qt::TextAlignmentRole:
        switch (index.column())
        {
        case Size:
        case Progress:
        case Ratio:
        case Downloaded:
        case Uploaded:
            return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
        default:
            break;
        }
        break;

    case Qt::ForegroundRole:
        return UIThemeManager::instance()->getColor(u"TransferList.Archived"_s);

    case Qt::ToolTipRole:
        if (index.column() == SavePath)
            return torrent.savePath.toString();
        break;

    default:
        break;
    }

    return {};
}

QVariant HistoryModel::headerData(const int section, const Qt::Orientation orientation, const int role) const
{
    if ((orientation != Qt::Horizontal) || (role != Qt::DisplayRole))
        return {};

    switch (section)
    {
    case Name:
        return tr("Name", "Torrent name");
    case Size:
        return tr("Size", "Torrent size");
    case Progress:
        return tr("Progress", "Torrent progress");
    case Category:
        return tr("Category", "Torrent category");
    case Tags:
        return tr("Tags", "Torrent tags");
    case AddedOn:
        return tr("Added On", "Torrent was added on 01/01/2010 08:00");
    case CompletedOn:
        return tr("Completed On", "Torrent was completed on 01/01/2010 08:00");
    case Ratio:
        return tr("Ratio", "Torrent share ratio");
    case Downloaded:
        return tr("Downloaded", "Amount of data downloaded");
    case Uploaded:
        return tr("Uploaded", "Amount of data uploaded");
    case SavePath:
        return tr("Save Path", "Torrent save path");
    default:
        break;
    }

    return {};
}

Qt::ItemFlags HistoryModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

std::optional<BitTorrent::ArchivedTorrent> HistoryModel::archivedTorrent(const QModelIndex &index) const
{
    if (!index.isValid() || (index.row() < 0) || (index.row() >= m_torrents.size()))
        return std::nullopt;

    return m_torrents.at(index.row());
}

void HistoryModel::reload()
{
    beginResetModel();
    m_torrents = BitTorrent::Session::instance()->archivedTorrents();
    endResetModel();
}
