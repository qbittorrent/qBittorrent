/*
 * Bittorrent Client using Qt and libtorrent.
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

#if defined(Q_OS_WIN)
#include <Windows.h>
#include <Shellapi.h>
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
#include <QtWin>
#endif
#else
#include <QMimeDatabase>
#include <QMimeType>
#endif

#if defined Q_OS_WIN || defined Q_OS_MACOS
#define QBT_PIXMAP_CACHE_FOR_FILE_ICONS
#include <QPixmapCache>
#endif

#include "base/bittorrent/abstractfilestorage.h"
#include "base/bittorrent/downloadpriority.h"
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
            : m_textPlainIcon {UIThemeManager::instance()->getIcon(u"help-about"_qs)}
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

#if defined(Q_OS_WIN)
    // See QTBUG-25319 for explanation why this is required
    class WinShellFileIconProvider final : public CachingFileIconProvider
    {
        QPixmap pixmapForExtension(const QString &ext) const override
        {
            const std::wstring extWStr = QString(u'.' + ext).toStdWString();

            SHFILEINFOW sfi {};
            const HRESULT hr = ::SHGetFileInfoW(extWStr.c_str(),
                FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), (SHGFI_ICON | SHGFI_USEFILEATTRIBUTES));
            if (FAILED(hr))
                return {};

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
            const auto iconPixmap = QPixmap::fromImage(QImage::fromHICON(sfi.hIcon));
#else
            const QPixmap iconPixmap = QtWin::fromHICON(sfi.hIcon);
#endif
            ::DestroyIcon(sfi.hIcon);
            return iconPixmap;
        }
    };
#elif defined(Q_OS_MACOS)
    // There is a similar bug on macOS, to be reported to Qt
    // https://github.com/qbittorrent/qBittorrent/pull/6156#issuecomment-316302615
    class MacFileIconProvider final : public CachingFileIconProvider
    {
        QPixmap pixmapForExtension(const QString &ext) const override
        {
            return MacUtils::pixmapForExtension(ext, QSize(32, 32));
        }
    };
#else
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
        const Path PSEUDO_UNIQUE_FILE_NAME = Utils::Fs::tempPath() / Path(u"qBittorrent-test-QFileIconProvider-845eb448-7ad5-4cdb-b764-b3f322a266a9"_qs);
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
    , m_rootItem(new TorrentContentModelFolder(QVector<QString>({ tr("Name"), tr("Total Size"), tr("Progress"), tr("Download Priority"), tr("Remaining"), tr("Availability") })))
#if defined(Q_OS_WIN)
    , m_fileIconProvider {new WinShellFileIconProvider}
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

void TorrentContentModel::updateFilesProgress(const QVector<qreal> &fp)
{
    Q_ASSERT(m_filesIndex.size() == fp.size());
    // XXX: Why is this necessary?
    if (m_filesIndex.size() != fp.size()) return;

    emit layoutAboutToBeChanged();
    for (int i = 0; i < fp.size(); ++i)
        m_filesIndex[i]->setProgress(fp[i]);
    // Update folders progress in the tree
    m_rootItem->recalculateProgress();
    m_rootItem->recalculateAvailability();

    const QVector<ColumnInterval> columns =
    {
        {TorrentContentModelItem::COL_PROGRESS, TorrentContentModelItem::COL_PROGRESS}
    };
    notifySubtreeUpdated(index(0, 0), columns);
}

void TorrentContentModel::updateFilesPriorities(const QVector<BitTorrent::DownloadPriority> &fprio)
{
    Q_ASSERT(m_filesIndex.size() == fprio.size());
    // XXX: Why is this necessary?
    if (m_filesIndex.size() != fprio.size())
        return;

    emit layoutAboutToBeChanged();
    for (int i = 0; i < fprio.size(); ++i)
        m_filesIndex[i]->setPriority(static_cast<BitTorrent::DownloadPriority>(fprio[i]));

    const QVector<ColumnInterval> columns =
    {
        {TorrentContentModelItem::COL_NAME, TorrentContentModelItem::COL_NAME},
        {TorrentContentModelItem::COL_PRIO, TorrentContentModelItem::COL_PRIO}
    };
    notifySubtreeUpdated(index(0, 0), columns);
}

void TorrentContentModel::updateFilesAvailability(const QVector<qreal> &fa)
{
    Q_ASSERT(m_filesIndex.size() == fa.size());
    // XXX: Why is this necessary?
    if (m_filesIndex.size() != fa.size()) return;

    emit layoutAboutToBeChanged();
    for (int i = 0; i < m_filesIndex.size(); ++i)
        m_filesIndex[i]->setAvailability(fa[i]);
    // Update folders progress in the tree
    m_rootItem->recalculateProgress();

    const QVector<ColumnInterval> columns =
    {
        {TorrentContentModelItem::COL_AVAILABILITY, TorrentContentModelItem::COL_AVAILABILITY}
    };
    notifySubtreeUpdated(index(0, 0), columns);
}

QVector<BitTorrent::DownloadPriority> TorrentContentModel::getFilePriorities() const
{
    QVector<BitTorrent::DownloadPriority> prio;
    prio.reserve(m_filesIndex.size());
    for (const TorrentContentModelFile *file : asConst(m_filesIndex))
        prio.push_back(file->priority());
    return prio;
}

bool TorrentContentModel::allFiltered() const
{
    return std::all_of(m_filesIndex.cbegin(), m_filesIndex.cend(), [](const TorrentContentModelFile *fileItem)
    {
        return (fileItem->priority() == BitTorrent::DownloadPriority::Ignored);
    });
}

int TorrentContentModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return TorrentContentModelItem::NB_COL;
}

bool TorrentContentModel::setData(const QModelIndex &index, const QVariant &value, const int role)
{
    if (!index.isValid())
        return false;

    if ((index.column() == TorrentContentModelItem::COL_NAME) && (role == Qt::CheckStateRole))
    {
        auto *item = static_cast<TorrentContentModelItem *>(index.internalPointer());

        const BitTorrent::DownloadPriority currentPrio = item->priority();
        const auto checkState = static_cast<Qt::CheckState>(value.toInt());
        const BitTorrent::DownloadPriority newPrio = (checkState == Qt::PartiallyChecked)
            ? BitTorrent::DownloadPriority::Mixed
            : ((checkState == Qt::Unchecked)
                ? BitTorrent::DownloadPriority::Ignored
                : BitTorrent::DownloadPriority::Normal);

        if (currentPrio != newPrio)
        {
            item->setPriority(newPrio);
            // Update folders progress in the tree
            m_rootItem->recalculateProgress();
            m_rootItem->recalculateAvailability();

            const QVector<ColumnInterval> columns =
            {
                {TorrentContentModelItem::COL_NAME, TorrentContentModelItem::COL_NAME},
                {TorrentContentModelItem::COL_PRIO, TorrentContentModelItem::COL_PRIO}
            };
            notifySubtreeUpdated(index, columns);
            emit filteredFilesChanged();

            return true;
        }
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
                    item->setName(newName);
                    emit dataChanged(index, index);
                    return true;
                }
            }
            break;

        case TorrentContentModelItem::COL_PRIO:
            {
                const BitTorrent::DownloadPriority currentPrio = item->priority();
                const auto newPrio = static_cast<BitTorrent::DownloadPriority>(value.toInt());
                if (currentPrio != newPrio)
                {
                    item->setPriority(newPrio);

                    const QVector<ColumnInterval> columns =
                    {
                        {TorrentContentModelItem::COL_NAME, TorrentContentModelItem::COL_NAME},
                        {TorrentContentModelItem::COL_PRIO, TorrentContentModelItem::COL_PRIO}
                    };
                    notifySubtreeUpdated(index, columns);

                    if ((newPrio == BitTorrent::DownloadPriority::Ignored)
                        || (currentPrio == BitTorrent::DownloadPriority::Ignored))
                    {
                        emit filteredFilesChanged();
                    }

                    return true;
                }
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

int TorrentContentModel::getFileIndex(const QModelIndex &index)
{
    auto *item = static_cast<TorrentContentModelItem *>(index.internalPointer());
    if (item->itemType() == TorrentContentModelItem::FileType)
        return static_cast<TorrentContentModelFile *>(item)->fileIndex();

    Q_ASSERT(item->itemType() == TorrentContentModelItem::FileType);
    return -1;
}

QVariant TorrentContentModel::data(const QModelIndex &index, const int role) const
{
    if (!index.isValid())
        return {};

    auto *item = static_cast<TorrentContentModelItem *>(index.internalPointer());

    switch (role)
    {
    case Qt::DecorationRole:
        {
            if (index.column() != TorrentContentModelItem::COL_NAME)
                return {};

            if (item->itemType() == TorrentContentModelItem::FolderType)
                return m_fileIconProvider->icon(QFileIconProvider::Folder);
            return m_fileIconProvider->icon(QFileInfo(item->name()));
        }
    case Qt::CheckStateRole:
        {
            if (index.column() != TorrentContentModelItem::COL_NAME)
                return {};

            if (item->priority() == BitTorrent::DownloadPriority::Ignored)
                return Qt::Unchecked;
            if (item->priority() == BitTorrent::DownloadPriority::Mixed)
                return Qt::PartiallyChecked;
            return Qt::Checked;
        }
    case Qt::TextAlignmentRole:
        if ((index.column() == TorrentContentModelItem::COL_SIZE)
            || (index.column() == TorrentContentModelItem::COL_REMAINING))
            return QVariant {Qt::AlignRight | Qt::AlignVCenter};
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

    Qt::ItemFlags flags {Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable};
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
            return QVariant {Qt::AlignRight | Qt::AlignVCenter};
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

void TorrentContentModel::clear()
{
    qDebug("clear called");
    beginResetModel();
    m_filesIndex.clear();
    m_rootItem->deleteAllChildren();
    endResetModel();
}

void TorrentContentModel::setupModelData(const BitTorrent::AbstractFileStorage &info)
{
    qDebug("setup model data called");
    const int filesCount = info.filesCount();
    if (filesCount <= 0)
        return;

    emit layoutAboutToBeChanged();
    // Initialize files_index array
    qDebug("Torrent contains %d files", filesCount);
    m_filesIndex.reserve(filesCount);

    QHash<TorrentContentModelFolder *, QHash<QString, TorrentContentModelFolder *>> folderMap;
    QVector<QString> lastParentPath;
    TorrentContentModelFolder *lastParent = m_rootItem;
    // Iterate over files
    for (int i = 0; i < filesCount; ++i)
    {
        const QString path = info.filePath(i).data();

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
        TorrentContentModelFile *fileItem = new TorrentContentModelFile(
                    fileName, info.fileSize(i), lastParent, i);
        lastParent->appendChild(fileItem);
        m_filesIndex.push_back(fileItem);
    }
    emit layoutChanged();
}

void TorrentContentModel::notifySubtreeUpdated(const QModelIndex &index, const QVector<ColumnInterval> &columns)
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
    QVector<QModelIndex> parentIndexes;

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
