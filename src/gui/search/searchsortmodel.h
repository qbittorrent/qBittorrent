/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2013  sledgehammer999 <hammered999@gmail.com>
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

#pragma once

#include <QSortFilterProxyModel>
#include <QStringList>

#include "base/utils/compare.h"

class SearchSortModel final : public QSortFilterProxyModel
{
    using base = QSortFilterProxyModel;

public:
    enum SearchColumn
    {
        NAME,
        SIZE,
        SEEDS,
        LEECHES,
        ENGINE_NAME,
        ENGINE_URL,
        PUB_DATE,
        DL_LINK,
        DESC_LINK,
        NB_SEARCH_COLUMNS
    };

    enum SearchDataRole
    {
        UnderlyingDataRole = Qt::UserRole
    };

    explicit SearchSortModel(QObject *parent = nullptr);

    void enableNameFilter(bool enabled);
    void setNameFilter(const QString &searchTerm = {});

    //! \brief Sets parameters for filtering by size
    //! \param minSize minimal size in bytes
    //! \param maxSize maximal size in bytes, negative value to disable filtering
    void setSizeFilter(qint64 minSize, qint64 maxSize);

    //! \brief Sets parameters for filtering by seeds number
    //! \param minSeeds minimal number of seeders
    //! \param maxSeeds maximal number of seeders, negative value to disable filtering
    void setSeedsFilter(int minSeeds, int maxSeeds);

    //! \brief Sets parameters for filtering by leeches number
    //! \param minLeeches minimal number of leechers
    //! \param maxLeeches maximal number of leechers, negative value to disable filtering
    void setLeechesFilter(int minLeeches, int maxLeeches);

    bool isNameFilterEnabled() const;

    QString searchTerm() const;

    int minSeeds() const;
    int maxSeeds() const;

    qint64 minSize() const;
    qint64 maxSize() const;

protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    bool m_isNameFilterEnabled = false;
    QString m_searchTerm;
    QStringList m_searchTermWords;
    int m_minSeeds = 0, m_maxSeeds = -1;
    int m_minLeeches = 0, m_maxLeeches = -1;
    qint64 m_minSize = 0, m_maxSize = -1;

    Utils::Compare::NaturalLessThan<Qt::CaseInsensitive> m_naturalLessThan;
};
