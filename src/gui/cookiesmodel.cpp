/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016  Vladimir Golovnev <glassez@yandex.ru>
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

#include "cookiesmodel.h"

#include <QDateTime>

CookiesModel::CookiesModel(const QList<QNetworkCookie> &cookies, QObject *parent)
    : QAbstractItemModel(parent)
    , m_cookies(cookies)
{
}

QList<QNetworkCookie> CookiesModel::cookies() const
{
    return m_cookies;
}

QVariant CookiesModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if ((role == Qt::DisplayRole) && (orientation == Qt::Horizontal))
    {
        switch (section)
        {
        case COL_DOMAIN:
            return tr("Domain");
        case COL_PATH:
            return tr("Path");
        case COL_NAME:
            return tr("Name");
        case COL_VALUE:
            return tr("Value");
        case COL_EXPDATE:
            return tr("Expiration Date");
        }
    }

    return {};
}

QModelIndex CookiesModel::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid() // no items with valid parent
        || (row < 0) || (row >= m_cookies.size())
        || (column < 0) || (column >= NB_COLUMNS))
        return {};

    return createIndex(row, column, &m_cookies[row]);
}

QModelIndex CookiesModel::parent([[maybe_unused]] const QModelIndex &index) const
{
    return {};
}

int CookiesModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;

    return m_cookies.size();
}

int CookiesModel::columnCount([[maybe_unused]] const QModelIndex &parent) const
{
    return NB_COLUMNS;
}

QVariant CookiesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || (index.row() >= m_cookies.size())
        || ((role != Qt::DisplayRole) && (role != Qt::EditRole)))
        return {};

    switch (index.column())
    {
    case COL_DOMAIN:
        return m_cookies[index.row()].domain();
    case COL_PATH:
        return m_cookies[index.row()].path();
    case COL_NAME:
        return QString::fromLatin1(m_cookies[index.row()].name());
    case COL_VALUE:
        return QString::fromLatin1(m_cookies[index.row()].value());
    case COL_EXPDATE:
        return m_cookies[index.row()].expirationDate();
    }

    return {};
}

bool CookiesModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role != Qt::EditRole) return false;

    switch (index.column())
    {
    case COL_DOMAIN:
        m_cookies[index.row()].setDomain(value.toString());
        break;
    case COL_PATH:
        m_cookies[index.row()].setPath(value.toString());
        break;
    case COL_NAME:
        m_cookies[index.row()].setName(value.toString().toLatin1());
        break;
    case COL_VALUE:
        m_cookies[index.row()].setValue(value.toString().toLatin1());
        break;
    case COL_EXPDATE:
        m_cookies[index.row()].setExpirationDate(value.toDateTime());
        break;
    default:
        return false;
    }

    emit dataChanged(index, index);
    return true;
}

bool CookiesModel::insertRows(int row, int count, const QModelIndex &parent)
{
    if ((row < 0) || (row > m_cookies.size())) return false;

    QNetworkCookie newCookie;
    newCookie.setExpirationDate(QDateTime::currentDateTime().addYears(2));

    beginInsertRows(parent, row, row + count - 1);
    while (count-- > 0)
        m_cookies.insert(row, newCookie);
    endInsertRows();

    return true;
}

bool CookiesModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if ((m_cookies.isEmpty())
        || (row >= m_cookies.size())
        || ((row + count) > m_cookies.size()))
        return false;

    beginRemoveRows(parent, row, row + count - 1);
    while (count-- > 0)
        m_cookies.removeAt(row);
    endRemoveRows();

    return true;
}

Qt::ItemFlags CookiesModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;

    return Qt::ItemIsEditable | QAbstractItemModel::flags(index);
}
