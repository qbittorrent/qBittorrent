/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Tim Sylvester <t.j.sylvester@gmail.com>
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

#include "discoveryrootsmodel.h"

#include <QDir>

#include "base/exceptions.h"

DiscoveryRootsModel::DiscoveryRootsModel(DiscoveryRoots *discoveryRoots, QObject *parent)
    : QAbstractListModel {parent}
    , m_discoveryRoots {discoveryRoots}
    , m_roots {m_discoveryRoots->roots()}
{
    connect(m_discoveryRoots, &DiscoveryRoots::rootsChanged, this, &DiscoveryRootsModel::onRootsChanged);
}

int DiscoveryRootsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_roots.size();
}

int DiscoveryRootsModel::columnCount([[maybe_unused]] const QModelIndex &parent) const
{
    return 1;
}

QVariant DiscoveryRootsModel::data(const QModelIndex &index, const int role) const
{
    if (!index.isValid() || (index.row() >= rowCount()) || (index.column() >= columnCount()))
        return {};

    if (role == Qt::DisplayRole)
        return m_roots.at(index.row()).path.toString();

    return {};
}

QVariant DiscoveryRootsModel::headerData(const int section, const Qt::Orientation orientation, const int role) const
{
    if ((orientation != Qt::Horizontal) || (role != Qt::DisplayRole)
            || (section < 0) || (section >= columnCount()))
    {
        return {};
    }

    return tr("Discovery root");
}

bool DiscoveryRootsModel::removeRows(const int row, const int count, const QModelIndex &parent)
{
    if (parent.isValid() || (row < 0) || (row >= rowCount())
        || (count <= 0) || ((row + count) > rowCount()))
    {
        return false;
    }

    const int firstRow = row;
    const int lastRow = row + (count - 1);

    beginRemoveRows(parent, firstRow, lastRow);
    m_roots.remove(firstRow, count);
    endRemoveRows();

    return true;
}

void DiscoveryRootsModel::addRoot(const Path &path, const DiscoveryRootOptions &options)
{
    if (path.isEmpty())
        throw InvalidArgument(tr("Discovery root path cannot be empty."));

    if (path.isRelative())
        throw InvalidArgument(tr("Discovery root path cannot be relative."));

    for (const DiscoveryRoot &root : m_roots)
    {
        if (root.path == path)
            throw RuntimeError(tr("Folder '%1' is already in the discovery root list.").arg(path.toString()));
    }

    const QDir rootDir {path.data()};
    if (!rootDir.exists())
        throw RuntimeError(tr("Folder '%1' doesn't exist.").arg(path.toString()));
    if (!rootDir.isReadable())
        throw RuntimeError(tr("Folder '%1' isn't readable.").arg(path.toString()));

    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    m_roots.append({.path = path, .options = options});
    endInsertRows();
}

DiscoveryRootOptions DiscoveryRootsModel::rootOptions(const int row) const
{
    Q_ASSERT((row >= 0) && (row < rowCount()));

    return m_roots.at(row).options;
}

void DiscoveryRootsModel::setRootOptions(const int row, const DiscoveryRootOptions &options)
{
    Q_ASSERT((row >= 0) && (row < rowCount()));

    m_roots[row].options = options;
}

void DiscoveryRootsModel::apply()
{
    m_discoveryRoots->setRoots(m_roots);
}

void DiscoveryRootsModel::onRootsChanged()
{
    beginResetModel();
    m_roots = m_discoveryRoots->roots();
    endResetModel();
}
