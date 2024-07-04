/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2023  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2017  Tony Gregerson <tony.gregerson@gmail.com>
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

#include "tagfiltermodel.h"

#include <QIcon>
#include <QList>

#include "base/bittorrent/session.h"
#include "base/global.h"
#include "gui/uithememanager.h"

const int ROW_ALL = 0;
const int ROW_UNTAGGED = 1;

class TagModelItem
{
public:
    TagModelItem(const Tag &tag, int torrentsCount = 0)
        : m_tag {tag}
        , m_torrentsCount {torrentsCount}
    {
    }

    Tag tag() const
    {
        return m_tag;
    }

    int torrentsCount() const
    {
        return m_torrentsCount;
    }

    void increaseTorrentsCount()
    {
        ++m_torrentsCount;
    }

    void decreaseTorrentsCount()
    {
        Q_ASSERT(m_torrentsCount > 0);
        --m_torrentsCount;
    }

private:
    Tag m_tag;
    int m_torrentsCount;
};

TagFilterModel::TagFilterModel(QObject *parent)
    : QAbstractListModel(parent)
{
    using Session = BitTorrent::Session;
    const auto *session = Session::instance();

    connect(session, &Session::tagAdded, this, &TagFilterModel::tagAdded);
    connect(session, &Session::tagRemoved, this, &TagFilterModel::tagRemoved);
    connect(session, &Session::torrentTagAdded, this, &TagFilterModel::torrentTagAdded);
    connect(session, &Session::torrentTagRemoved, this, &TagFilterModel::torrentTagRemoved);
    connect(session, &Session::torrentsLoaded, this, &TagFilterModel::torrentsLoaded);
    connect(session, &Session::torrentAboutToBeRemoved, this, &TagFilterModel::torrentAboutToBeRemoved);
    populate();
}

TagFilterModel::~TagFilterModel() = default;

bool TagFilterModel::isSpecialItem(const QModelIndex &index)
{
    // the first two items are special items: 'All' and 'Untagged'
    return (!index.parent().isValid() && (index.row() <= 1));
}

QVariant TagFilterModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || (index.column() != 0))
        return {};

    const int row = index.internalId();
    Q_ASSERT(isValidRow(row));
    const TagModelItem &item = m_tagItems[row];

    const auto displayName = [&row, tag = item.tag()]
    {
        switch (row)
        {
        case 0:
            return tr("All");
        case 1:
            return tr("Untagged");
        default:
            return tag.toString();
        }
    };

    switch (role)
    {
    case Qt::DecorationRole:
        return UIThemeManager::instance()->getIcon(u"tags"_s, u"inode-directory"_s);
    case Qt::DisplayRole:
        return u"%1 (%2)"_s.arg(displayName(), QString::number(item.torrentsCount()));
    case Qt::UserRole:
        return item.torrentsCount();
    default:
        return {};
    }
}

Qt::ItemFlags TagFilterModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant TagFilterModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if ((orientation == Qt::Horizontal) && (role == Qt::DisplayRole))
        if (section == 0)
            return tr("Tags");
    return {};
}

QModelIndex TagFilterModel::index(int row, int, const QModelIndex &) const
{
    if (!isValidRow(row))
        return {};
    return createIndex(row, 0, row);
}

int TagFilterModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return m_tagItems.count();
    return 0;
}

bool TagFilterModel::isValidRow(int row) const
{
    return (row >= 0) && (row < m_tagItems.size());
}

QModelIndex TagFilterModel::index(const Tag &tag) const
{
    const int row = findRow(tag);
    if (!isValidRow(row))
        return {};
    return index(row, 0, QModelIndex());
}

Tag TagFilterModel::tag(const QModelIndex &index) const
{
    if (!index.isValid())
        return {};
    const int row = index.internalId();
    Q_ASSERT(isValidRow(row));
    return m_tagItems[row].tag();
}

void TagFilterModel::tagAdded(const Tag &tag)
{
    const int row = m_tagItems.count();
    beginInsertRows(QModelIndex(), row, row);
    addToModel(tag, 0);
    endInsertRows();
}

void TagFilterModel::tagRemoved(const Tag &tag)
{
    QModelIndex i = index(tag);
    beginRemoveRows(i.parent(), i.row(), i.row());
    removeFromModel(i.row());
    endRemoveRows();
}

void TagFilterModel::torrentTagAdded(BitTorrent::Torrent *const torrent, const Tag &tag)
{
    if (torrent->tags().count() == 1)
    {
        untaggedItem()->decreaseTorrentsCount();
        const QModelIndex i = index(ROW_UNTAGGED, 0);
        emit dataChanged(i, i);
    }

    const int row = findRow(tag);
    Q_ASSERT(isValidRow(row));
    TagModelItem &item = m_tagItems[row];

    item.increaseTorrentsCount();
    const QModelIndex i = index(row, 0);
    emit dataChanged(i, i);
}

void TagFilterModel::torrentTagRemoved(BitTorrent::Torrent *const torrent, const Tag &tag)
{
    if (torrent->tags().empty())
    {
        untaggedItem()->increaseTorrentsCount();
        const QModelIndex i = index(ROW_UNTAGGED, 0);
        emit dataChanged(i, i);
    }

    const int row = findRow(tag);
    if (row < 0)
        return;

    m_tagItems[row].decreaseTorrentsCount();

    const QModelIndex i = index(row, 0);
    emit dataChanged(i, i);
}

void TagFilterModel::torrentsLoaded(const QList<BitTorrent::Torrent *> &torrents)
{
    for (const BitTorrent::Torrent *torrent : torrents)
    {
        allTagsItem()->increaseTorrentsCount();

        const QList<TagModelItem *> items = findItems(torrent->tags());
        if (items.isEmpty())
            untaggedItem()->increaseTorrentsCount();

        for (TagModelItem *item : items)
            item->increaseTorrentsCount();
    }

    emit dataChanged(index(0, 0), index((rowCount() - 1), 0));
}

void TagFilterModel::torrentAboutToBeRemoved(BitTorrent::Torrent *const torrent)
{
    allTagsItem()->decreaseTorrentsCount();

    {
        const QModelIndex i = index(ROW_ALL, 0);
        emit dataChanged(i, i);
    }

    if (torrent->tags().isEmpty())
    {
        untaggedItem()->decreaseTorrentsCount();
        const QModelIndex i = index(ROW_UNTAGGED, 0);
        emit dataChanged(i, i);
    }
    else
    {
        for (const Tag &tag : asConst(torrent->tags()))
        {
            const int row = findRow(tag);
            Q_ASSERT(isValidRow(row));
            if (!isValidRow(row)) [[unlikely]]
                continue;

            m_tagItems[row].decreaseTorrentsCount();
            const QModelIndex i = index(row, 0);
            emit dataChanged(i, i);
        }
    }
}

void TagFilterModel::populate()
{
    using Torrent = BitTorrent::Torrent;

    const auto *session = BitTorrent::Session::instance();
    const auto torrents = session->torrents();

    // All torrents
    addToModel(Tag(), torrents.count());

    const int untaggedCount = std::count_if(torrents.cbegin(), torrents.cend()
            , [](Torrent *torrent) { return torrent->tags().isEmpty(); });
    addToModel(Tag(), untaggedCount);

    for (const Tag &tag : asConst(session->tags()))
    {
        const int count = std::count_if(torrents.cbegin(), torrents.cend()
                , [tag](Torrent *torrent) { return torrent->hasTag(tag); });
        addToModel(tag, count);
    }
}

void TagFilterModel::addToModel(const Tag &tag, const int count)
{
    m_tagItems.append(TagModelItem(tag, count));
}

void TagFilterModel::removeFromModel(int row)
{
    Q_ASSERT(isValidRow(row));
    m_tagItems.removeAt(row);
}

int TagFilterModel::findRow(const Tag &tag) const
{
    if (!tag.isValid())
        return -1;

    for (int i = 0; i < m_tagItems.size(); ++i)
    {
        if (m_tagItems[i].tag() == tag)
            return i;
    }

    return -1;
}

TagModelItem *TagFilterModel::findItem(const Tag &tag)
{
    const int row = findRow(tag);
    if (!isValidRow(row))
        return nullptr;
    return &m_tagItems[row];
}

QList<TagModelItem *> TagFilterModel::findItems(const TagSet &tags)
{
    QList<TagModelItem *> items;
    items.reserve(tags.count());
    for (const Tag &tag : tags)
    {
        TagModelItem *item = findItem(tag);
        if (item)
            items.push_back(item);
        else
            qWarning() << u"Requested tag '%1' missing from the model."_s.arg(tag.toString());
    }
    return items;
}

TagModelItem *TagFilterModel::allTagsItem()
{
    Q_ASSERT(!m_tagItems.isEmpty());
    return &m_tagItems[ROW_ALL];
}

TagModelItem *TagFilterModel::untaggedItem()
{
    Q_ASSERT(m_tagItems.size() > ROW_UNTAGGED);
    return &m_tagItems[ROW_UNTAGGED];
}
