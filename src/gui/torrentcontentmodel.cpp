/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2022-2024  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006-2012  Christophe Dumez <chris@qbittorrent.org>
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

#include "torrentcontentmodel.h"

#include <algorithm>

#include <QFileIconProvider>
#include <QFileInfo>
#include <QIcon>
#include <QMimeData>
#include <QPointer>
#include <QScopeGuard>
#include <QUrl>

#if defined(Q_OS_MACOS)
#define QBT_PIXMAP_CACHE_FOR_FILE_ICONS
#include <QPixmapCache>
#elif !defined(Q_OS_WIN)
#include <QMimeDatabase>
#include <QMimeType>
#endif

#include "base/bittorrent/downloadpriority.h"
#include "base/bittorrent/torrentcontenthandler.h"
#include "base/exceptions.h"
#include "base/global.h"
#include "base/path.h"
#include "base/utils/fs.h"
#include "torrentcontentmodelfile.h"
#include "torrentcontentmodelfolder.h"
#include "torrentcontentmodelitem.h"
#include "uithememanager.h"

#ifdef Q_OS_MACOS
#include "macutilities.h"
#endif

namespace
{
    class UnifiedFileIconProvider : public QFileIconProvider
    {
    public:
        UnifiedFileIconProvider()
            : m_textPlainIcon {UIThemeManager::instance()->getIcon(u"help-about"_s, u"text-plain"_s)}
        {
        }

        using QFileIconProvider::icon;

        QIcon icon(const QFileInfo &) const override
        {
            return m_textPlainIcon;
        }

    private:
        QIcon m_textPlainIcon;
    };

#ifdef QBT_PIXMAP_CACHE_FOR_FILE_ICONS
    class CachingFileIconProvider : public UnifiedFileIconProvider
    {
    public:
        using QFileIconProvider::icon;

        QIcon icon(const QFileInfo &info) const final
        {
            const QString ext = info.suffix();
            if (!ext.isEmpty())
            {
                QPixmap cached;
                if (QPixmapCache::find(ext, &cached))
                    return {cached};

                const QPixmap pixmap = pixmapForExtension(ext);
                if (!pixmap.isNull())
                {
                    QPixmapCache::insert(ext, pixmap);
                    return {pixmap};
                }
            }
            return UnifiedFileIconProvider::icon(info);
        }

    protected:
        virtual QPixmap pixmapForExtension(const QString &ext) const = 0;
    };
#endif // QBT_PIXMAP_CACHE_FOR_FILE_ICONS

#if defined(Q_OS_MACOS)
    // There is a bug on macOS, to be reported to Qt
    // https://github.com/qbittorrent/qBittorrent/pull/6156#issuecomment-316302615
    class MacFileIconProvider final : public CachingFileIconProvider
    {
        QPixmap pixmapForExtension(const QString &ext) const override
        {
            return MacUtils::pixmapForExtension(ext, QSize(32, 32));
        }
    };
#elif !defined(Q_OS_WIN)
    /**
     * @brief Tests whether QFileIconProvider actually works
     *
     * Some QPA plugins do not implement QPlatformTheme::fileIcon(), and
     * QFileIconProvider::icon() returns empty icons as the result. Here we ask it for
     * two icons for probably absent files and when both icons are null, we assume that
     * the current QPA plugin does not implement QPlatformTheme::fileIcon().
     */
    bool doesQFileIconProviderWork()
    {
        const Path PSEUDO_UNIQUE_FILE_NAME = Utils::Fs::tempPath() / Path(u"qBittorrent-test-QFileIconProvider-845eb448-7ad5-4cdb-b764-b3f322a266a9"_s);
        QFileIconProvider provider;
        const QIcon testIcon1 = provider.icon(QFileInfo((PSEUDO_UNIQUE_FILE_NAME + u".pdf").data()));
        const QIcon testIcon2 = provider.icon(QFileInfo((PSEUDO_UNIQUE_FILE_NAME + u".png").data()));
        return (!testIcon1.isNull() || !testIcon2.isNull());
    }

    class MimeFileIconProvider final : public UnifiedFileIconProvider
    {
        using QFileIconProvider::icon;

        QIcon icon(const QFileInfo &info) const override
        {
            const QMimeType mimeType = QMimeDatabase().mimeTypeForFile(info, QMimeDatabase::MatchExtension);

            const auto mimeIcon = QIcon::fromTheme(mimeType.iconName());
            if (!mimeIcon.isNull())
                return mimeIcon;

            const auto genericIcon = QIcon::fromTheme(mimeType.genericIconName());
            if (!genericIcon.isNull())
                return genericIcon;

            return UnifiedFileIconProvider::icon(info);
        }
    };
#endif // Q_OS_WIN
}

TorrentContentModel::TorrentContentModel(QObject *parent)
    : QAbstractItemModel(parent)
    , m_rootItem(new TorrentContentModelFolder(QList<QString>({ tr("Name"), tr("Total Size"), tr("Progress"), tr("Download Priority"), tr("Remaining"), tr("Availability") })))
#if defined(Q_OS_WIN)
    , m_fileIconProvider {new QFileIconProvider}
#elif defined(Q_OS_MACOS)
    , m_fileIconProvider {new MacFileIconProvider}
#else
    , m_fileIconProvider {doesQFileIconProviderWork() ? new QFileIconProvider : new MimeFileIconProvider}
#endif
{
    m_fileIconProvider->setOptions(QFileIconProvider::DontUseCustomDirectoryIcons);
}

TorrentContentModel::~TorrentContentModel()
{
    delete m_fileIconProvider;
    delete m_rootItem;
}

void TorrentContentModel::updateFilesProgress()
{
    Q_ASSERT(m_contentHandler && m_contentHandler->hasMetadata());

    const QList<qreal> &filesProgress = m_contentHandler->filesProgress();
    Q_ASSERT(m_filesIndex.size() == filesProgress.size());
    // XXX: Why is this necessary?
    if (m_filesIndex.size() != filesProgress.size()) [[unlikely]]
        return;

    for (int i = 0; i < filesProgress.size(); ++i)
        m_filesIndex[i]->setProgress(filesProgress[i]);
    // Update folders progress in the tree
    m_rootItem->recalculateProgress();
    m_rootItem->recalculateAvailability();
}

void TorrentContentModel::updateFilesPriorities()
{
    Q_ASSERT(m_contentHandler && m_contentHandler->hasMetadata());

    const QList<BitTorrent::DownloadPriority> fprio = m_contentHandler->filePriorities();
    Q_ASSERT(m_filesIndex.size() == fprio.size());
    // XXX: Why is this necessary?
    if (m_filesIndex.size() != fprio.size())
        return;

    for (int i = 0; i < fprio.size(); ++i)
        m_filesIndex[i]->setPriority(static_cast<BitTorrent::DownloadPriority>(fprio[i]));
}

void TorrentContentModel::updateFilesAvailability()
{
    Q_ASSERT(m_contentHandler && m_contentHandler->hasMetadata());

    using HandlerPtr = QPointer<BitTorrent::TorrentContentHandler>;
    m_contentHandler->fetchAvailableFileFractions([this, handler = HandlerPtr(m_contentHandler)](const QList<qreal> &availableFileFractions)
    {
        if (handler != m_contentHandler)
            return;

        Q_ASSERT(m_filesIndex.size() == availableFileFractions.size());
        // XXX: Why is this necessary?
        if (m_filesIndex.size() != availableFileFractions.size()) [[unlikely]]
            return;

        for (int i = 0; i < m_filesIndex.size(); ++i)
            m_filesIndex[i]->setAvailability(availableFileFractions[i]);
        // Update folders progress in the tree
        m_rootItem->recalculateProgress();
    });
}

bool TorrentContentModel::setItemPriority(const QModelIndex &index, BitTorrent::DownloadPriority priority)
{
    Q_ASSERT(index.isValid());

    auto *item = static_cast<TorrentContentModelItem *>(index.internalPointer());
    const BitTorrent::DownloadPriority currentPriority = item->priority();
    if (currentPriority == priority)
        return false;

    item->setPriority(priority);
    m_contentHandler->prioritizeFiles(getFilePriorities());

    // Update folders progress in the tree
    m_rootItem->recalculateProgress();
    m_rootItem->recalculateAvailability();

    const QList<ColumnInterval> columns =
    {
        {TorrentContentModelItem::COL_NAME, TorrentContentModelItem::COL_NAME},
        {TorrentContentModelItem::COL_PRIO, TorrentContentModelItem::COL_PRIO}
    };
    notifySubtreeUpdated(index, columns);

    return true;
}

QList<BitTorrent::DownloadPriority> TorrentContentModel::getFilePriorities() const
{
    QList<BitTorrent::DownloadPriority> prio;
    prio.reserve(m_filesIndex.size());
    for (const TorrentContentModelFile *file : asConst(m_filesIndex))
        prio.push_back(file->priority());
    return prio;
}

int TorrentContentModel::columnCount([[maybe_unused]] const QModelIndex &parent) const
{
    return TorrentContentModelItem::NB_COL;
}

bool TorrentContentModel::setData(const QModelIndex &index, const QVariant &value, const int role)
{
    if (!index.isValid())
        return false;

    if ((index.column() == TorrentContentModelItem::COL_NAME) && (role == Qt::CheckStateRole))
    {
        const auto checkState = static_cast<Qt::CheckState>(value.toInt());
        const BitTorrent::DownloadPriority newPrio = (checkState == Qt::PartiallyChecked)
            ? BitTorrent::DownloadPriority::Mixed
            : ((checkState == Qt::Unchecked)
                ? BitTorrent::DownloadPriority::Ignored
                : BitTorrent::DownloadPriority::Normal);

        return setItemPriority(index, newPrio);
    }

    if (role == Qt::EditRole)
    {
        auto *item = static_cast<TorrentContentModelItem *>(index.internalPointer());

        switch (index.column())
        {
        case TorrentContentModelItem::COL_NAME:
            {
                const QString currentName = item->name();
                const QString newName = value.toString();
                if (currentName != newName)
                {
                    try
                    {
                        const Path parentPath = getItemPath(index.parent());
                        const Path oldPath = parentPath / Path(currentName);
                        const Path newPath = parentPath / Path(newName);

                        if (item->itemType() == TorrentContentModelItem::FileType)
                            m_contentHandler->renameFile(oldPath, newPath);
                        else
                            m_contentHandler->renameFolder(oldPath, newPath);
                    }
                    catch (const RuntimeError &error)
                    {
                        emit renameFailed(error.message());
                        return false;
                    }

                    item->setName(newName);
                    emit dataChanged(index, index);
                    return true;
                }
            }
            break;

        case TorrentContentModelItem::COL_PRIO:
            {
                const auto newPrio = static_cast<BitTorrent::DownloadPriority>(value.toInt());
                return setItemPriority(index, newPrio);
            }
            break;

        default:
            break;
        }
    }

    return false;
}

TorrentContentModelItem::ItemType TorrentContentModel::itemType(const QModelIndex &index) const
{
    return static_cast<const TorrentContentModelItem *>(index.internalPointer())->itemType();
}

int TorrentContentModel::getFileIndex(const QModelIndex &index) const
{
    auto *item = static_cast<TorrentContentModelItem *>(index.internalPointer());
    if (item->itemType() == TorrentContentModelItem::FileType)
        return static_cast<TorrentContentModelFile *>(item)->fileIndex();

    return -1;
}

Path TorrentContentModel::getItemPath(const QModelIndex &index) const
{
    Path path;
    for (QModelIndex i = index; i.isValid(); i = i.parent())
        path = Path(i.data().toString()) / path;
    return path;
}

QVariant TorrentContentModel::data(const QModelIndex &index, const int role) const
{
    if (!index.isValid())
        return {};

    auto *item = static_cast<TorrentContentModelItem *>(index.internalPointer());

    switch (role)
    {
    case Qt::DecorationRole:
        if (index.column() != TorrentContentModelItem::COL_NAME)
            return {};

        if (item->itemType() == TorrentContentModelItem::FolderType)
            return m_fileIconProvider->icon(QFileIconProvider::Folder);

        return m_fileIconProvider->icon(QFileInfo(item->name()));

    case Qt::CheckStateRole:
        if (index.column() != TorrentContentModelItem::COL_NAME)
            return {};

        if (item->priority() == BitTorrent::DownloadPriority::Ignored)
            return Qt::Unchecked;

        if (item->priority() == BitTorrent::DownloadPriority::Mixed)
        {
            Q_ASSERT(item->itemType() == TorrentContentModelItem::FolderType);

            const auto *folder = static_cast<TorrentContentModelFolder *>(item);
            const auto childItems = folder->children();
            const bool hasIgnored = std::any_of(childItems.cbegin(), childItems.cend()
                         , [](const TorrentContentModelItem *childItem)
            {
                const auto prio = childItem->priority();
                return ((prio == BitTorrent::DownloadPriority::Ignored)
                    || (prio == BitTorrent::DownloadPriority::Mixed));
            });

            return hasIgnored ? Qt::PartiallyChecked : Qt::Checked;
        }

        return Qt::Checked;

    case Qt::TextAlignmentRole:
        if ((index.column() == TorrentContentModelItem::COL_SIZE)
            || (index.column() == TorrentContentModelItem::COL_REMAINING))
        {
            return QVariant {Qt::AlignRight | Qt::AlignVCenter};
        }

        return {};

    case Qt::DisplayRole:
    case Qt::ToolTipRole:
        return item->displayData(index.column());

    case Roles::UnderlyingDataRole:
        return item->underlyingData(index.column());

    default:
        break;
    }

    return {};
}

Qt::ItemFlags TorrentContentModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    Qt::ItemFlags flags {Qt::ItemIsDragEnabled | Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable};
    if (itemType(index) == TorrentContentModelItem::FolderType)
        flags |= Qt::ItemIsAutoTristate;
    if (index.column() == TorrentContentModelItem::COL_PRIO)
        flags |= Qt::ItemIsEditable;

    return flags;
}

QVariant TorrentContentModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal)
        return {};

    switch (role)
    {
    case Qt::DisplayRole:
        return m_rootItem->displayData(section);

    case Qt::TextAlignmentRole:
        if ((section == TorrentContentModelItem::COL_SIZE)
            || (section == TorrentContentModelItem::COL_REMAINING))
        {
            return QVariant {Qt::AlignRight | Qt::AlignVCenter};
        }

        return {};

    default:
        return {};
    }
}

QModelIndex TorrentContentModel::index(const int row, const int column, const QModelIndex &parent) const
{
    if (column >= columnCount())
        return {};

    const TorrentContentModelFolder *parentItem = parent.isValid()
        ? static_cast<TorrentContentModelFolder *>(parent.internalPointer())
        : m_rootItem;
    Q_ASSERT(parentItem);

    if (row >= parentItem->childCount())
        return {};

    TorrentContentModelItem *childItem = parentItem->child(row);
    if (childItem)
        return createIndex(row, column, childItem);

    return {};
}

QModelIndex TorrentContentModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return {};

    const auto *item = static_cast<TorrentContentModelItem *>(index.internalPointer());
    if (!item)
        return {};

    TorrentContentModelItem *parentItem = item->parent();
    if (parentItem == m_rootItem)
        return {};

    // From https://doc.qt.io/qt-6/qabstractitemmodel.html#parent:
    // A common convention used in models that expose tree data structures is that only items
    // in the first column have children. For that case, when reimplementing this function in
    // a subclass the column of the returned QModelIndex would be 0.
    return createIndex(parentItem->row(), 0, parentItem);
}

int TorrentContentModel::rowCount(const QModelIndex &parent) const
{
    const TorrentContentModelFolder *parentItem = parent.isValid()
        ? dynamic_cast<TorrentContentModelFolder *>(static_cast<TorrentContentModelItem *>(parent.internalPointer()))
        : m_rootItem;
    return parentItem ? parentItem->childCount() : 0;
}

QMimeData *TorrentContentModel::mimeData(const QModelIndexList &indexes) const
{
    if (indexes.isEmpty())
        return nullptr;

    const Path storagePath = contentHandler()->actualStorageLocation();

    QList<QUrl> paths;
    paths.reserve(indexes.size());

    for (const QModelIndex &index : indexes)
    {
        if (!index.isValid())
            continue;
        if (index.column() != TorrentContentModelItem::COL_NAME)
            continue;

        if (itemType(index) == TorrentContentModelItem::FileType)
        {
            const int idx = getFileIndex(index);
            const Path fullPath = storagePath / contentHandler()->actualFilePath(idx);
            paths.append(QUrl::fromLocalFile(fullPath.data()));
        }
        else // folder type
        {
            const Path fullPath = storagePath / getItemPath(index);
            paths.append(QUrl::fromLocalFile(fullPath.data()));
        }
    }

    auto *mimeData = new QMimeData; // lifetime will be handled by Qt
    mimeData->setUrls(paths);

    return mimeData;
}

QStringList TorrentContentModel::mimeTypes() const
{
    return {u"text/uri-list"_s};
}

void TorrentContentModel::populate()
{
    Q_ASSERT(m_contentHandler && m_contentHandler->hasMetadata());

    const int filesCount = m_contentHandler->filesCount();
    m_filesIndex.reserve(filesCount);

    QHash<TorrentContentModelFolder *, QHash<QString, TorrentContentModelFolder *>> folderMap;
    QList<QString> lastParentPath;
    TorrentContentModelFolder *lastParent = m_rootItem;
    // Iterate over files
    for (int i = 0; i < filesCount; ++i)
    {
        const QString path = m_contentHandler->filePath(i).data();

        // Iterate of parts of the path to create necessary folders
        QList<QStringView> pathFolders = QStringView(path).split(u'/', Qt::SkipEmptyParts);
        const QString fileName = pathFolders.takeLast().toString();

        if (!std::equal(lastParentPath.begin(), lastParentPath.end()
                        , pathFolders.begin(), pathFolders.end()))
        {
            lastParentPath.clear();
            lastParentPath.reserve(pathFolders.size());

            // rebuild the path from the root
            lastParent = m_rootItem;
            for (const QStringView pathPart : asConst(pathFolders))
            {
                const QString folderName = pathPart.toString();
                lastParentPath.push_back(folderName);

                TorrentContentModelFolder *&newParent = folderMap[lastParent][folderName];
                if (!newParent)
                {
                    newParent = new TorrentContentModelFolder(folderName, lastParent);
                    lastParent->appendChild(newParent);
                }

                lastParent = newParent;
            }
        }

        // Actually create the file
        auto *fileItem = new TorrentContentModelFile(fileName, m_contentHandler->fileSize(i), lastParent, i);
        lastParent->appendChild(fileItem);
        m_filesIndex.push_back(fileItem);
    }

    updateFilesProgress();
    updateFilesPriorities();
    updateFilesAvailability();
}

void TorrentContentModel::setContentHandler(BitTorrent::TorrentContentHandler *contentHandler)
{
    beginResetModel();
    [[maybe_unused]] const auto modelResetGuard = qScopeGuard([this] { endResetModel(); });

    if (m_contentHandler)
    {
        m_filesIndex.clear();
        m_rootItem->deleteAllChildren();
    }

    m_contentHandler = contentHandler;

    if (m_contentHandler && m_contentHandler->hasMetadata())
        populate();
}

BitTorrent::TorrentContentHandler *TorrentContentModel::contentHandler() const
{
    return m_contentHandler;
}

void TorrentContentModel::refresh()
{
    if (!m_contentHandler || !m_contentHandler->hasMetadata())
        return;

    if (!m_filesIndex.isEmpty())
    {
        updateFilesProgress();
        updateFilesPriorities();
        updateFilesAvailability();

        const QList<ColumnInterval> columns =
        {
            {TorrentContentModelItem::COL_NAME, TorrentContentModelItem::COL_NAME},
            {TorrentContentModelItem::COL_PROGRESS, TorrentContentModelItem::COL_PROGRESS},
            {TorrentContentModelItem::COL_PRIO, TorrentContentModelItem::COL_PRIO},
            {TorrentContentModelItem::COL_AVAILABILITY, TorrentContentModelItem::COL_AVAILABILITY}
        };
        notifySubtreeUpdated(index(0, 0), columns);
    }
    else
    {
        beginResetModel();
        populate();
        endResetModel();
    }
}

void TorrentContentModel::notifySubtreeUpdated(const QModelIndex &index, const QList<ColumnInterval> &columns)
{
    // For best performance, `columns` entries should be arranged from left to right

    Q_ASSERT(index.isValid());

    // emit itself
    for (const ColumnInterval &column : columns)
        emit dataChanged(index.siblingAtColumn(column.first()), index.siblingAtColumn(column.last()));

    // propagate up the model
    QModelIndex parentIndex = parent(index);
    while (parentIndex.isValid())
    {
        for (const ColumnInterval &column : columns)
            emit dataChanged(parentIndex.siblingAtColumn(column.first()), parentIndex.siblingAtColumn(column.last()));
        parentIndex = parent(parentIndex);
    }

    // propagate down the model
    QList<QModelIndex> parentIndexes;

    if (hasChildren(index))
        parentIndexes.push_back(index);

    while (!parentIndexes.isEmpty())
    {
        const QModelIndex parent = parentIndexes.takeLast();

        const int childCount = rowCount(parent);
        const QModelIndex child = this->index(0, 0, parent);

        // emit this generation
        for (const ColumnInterval &column : columns)
        {
            const QModelIndex childTopLeft = child.siblingAtColumn(column.first());
            const QModelIndex childBottomRight = child.sibling((childCount - 1), column.last());
            emit dataChanged(childTopLeft, childBottomRight);
        }

        // check generations further down
        parentIndexes.reserve(childCount);
        for (int i = 0; i < childCount; ++i)
        {
            const QModelIndex sibling = child.siblingAtRow(i);
            if (hasChildren(sibling))
                parentIndexes.push_back(sibling);
        }
    }
}
