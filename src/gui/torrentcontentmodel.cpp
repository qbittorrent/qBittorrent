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
#include "base/exceptions.h"

#include <algorithm>

#include <QFileIconProvider>
#include <QFileInfo>
#include <QIcon>
#include <QMimeData>
#include <QSet>

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
    const QString QB_DRAGNDROP_MIME = QLatin1String("application/x-qbittorrent-paths");

    class UnifiedFileIconProvider : public QFileIconProvider
    {
    public:
        using QFileIconProvider::icon;

        QIcon icon(const QFileInfo &info) const override
        {
            Q_UNUSED(info);
            static QIcon cached = UIThemeManager::instance()->getIcon("text-plain");
            return cached;
        }
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
                if (QPixmapCache::find(ext, &cached)) return {cached};

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
            const QString extWithDot = QLatin1Char('.') + ext;
            SHFILEINFO sfi {};
            HRESULT hr = ::SHGetFileInfoW(extWithDot.toStdWString().c_str(),
                FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_USEFILEATTRIBUTES);
            if (FAILED(hr))
                return {};

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
            auto iconPixmap = QPixmap::fromImage(QImage::fromHICON(sfi.hIcon));
#else
            QPixmap iconPixmap = QtWin::fromHICON(sfi.hIcon);
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
        QFileIconProvider provider;
        const char PSEUDO_UNIQUE_FILE_NAME[] = "/tmp/qBittorrent-test-QFileIconProvider-845eb448-7ad5-4cdb-b764-b3f322a266a9";
        QIcon testIcon1 = provider.icon(QFileInfo(
            QLatin1String(PSEUDO_UNIQUE_FILE_NAME) + QLatin1String(".pdf")));
        QIcon testIcon2 = provider.icon(QFileInfo(
            QLatin1String(PSEUDO_UNIQUE_FILE_NAME) + QLatin1String(".png")));

        return (!testIcon1.isNull() || !testIcon2.isNull());
    }

    class MimeFileIconProvider : public UnifiedFileIconProvider
    {
        using QFileIconProvider::icon;

        QIcon icon(const QFileInfo &info) const override
        {
            const QMimeType mimeType = m_db.mimeTypeForFile(info, QMimeDatabase::MatchExtension);
            QIcon res = QIcon::fromTheme(mimeType.iconName());
            if (!res.isNull())
            {
                return res;
            }

            res = QIcon::fromTheme(mimeType.genericIconName());
            if (!res.isNull())
            {
                return res;
            }

            return UnifiedFileIconProvider::icon(info);
        }

    private:
        QMimeDatabase m_db;
    };
#endif // Q_OS_WIN
}

TorrentContentModel::TorrentContentModel(QObject *parent)
    : QAbstractItemModel(parent)
{
    initializeRootItem();
#if defined(Q_OS_WIN)
    m_fileIconProvider = new WinShellFileIconProvider();
#elif defined(Q_OS_MACOS)
    m_fileIconProvider = new MacFileIconProvider();
#else
    static bool doesBuiltInProviderWork = doesQFileIconProviderWork();
    m_fileIconProvider = doesBuiltInProviderWork ? new QFileIconProvider() : new MimeFileIconProvider();
#endif
}

TorrentContentModel::~TorrentContentModel()
{
    delete m_fileIconProvider;
    delete m_rootItem;
}

void TorrentContentModel::initializeRootItem()
{
    m_rootItem = new TorrentContentModelFolder(QVector<QString>({ tr("Name"), tr("Size"), tr("Progress"), tr("Download Priority"), tr("Remaining"), tr("Availability") }));
}

void TorrentContentModel::updateFilesProgress(const QVector<qreal> &fp)
{
    Q_ASSERT(m_filesIndex.size() == fp.size());
    // XXX: Why is this necessary?
    if (m_filesIndex.size() != fp.size()) return;

    for (int i = 0; i < fp.size(); ++i)
        m_filesIndex[i]->setProgress(fp[i]);
    // Update folders progress in the tree
    m_rootItem->recalculateProgress();
    m_rootItem->recalculateAvailability();
    emit dataChanged(index(0, 0), index((rowCount() - 1), (columnCount() - 1)));
}

void TorrentContentModel::updateFilesPriorities(const QVector<BitTorrent::DownloadPriority> &fprio)
{
    Q_ASSERT(m_filesIndex.size() == fprio.size());
    // XXX: Why is this necessary?
    if (m_filesIndex.size() != fprio.size())
        return;

    for (int i = 0; i < fprio.size(); ++i)
        m_filesIndex[i]->setPriority(static_cast<BitTorrent::DownloadPriority>(fprio[i]));
    emit dataChanged(index(0, 0), index((rowCount() - 1), (columnCount() - 1)));
}

void TorrentContentModel::updateFilesAvailability(const QVector<qreal> &fa)
{
    Q_ASSERT(m_filesIndex.size() == fa.size());
    // XXX: Why is this necessary?
    if (m_filesIndex.size() != fa.size()) return;

    for (int i = 0; i < m_filesIndex.size(); ++i)
        m_filesIndex[i]->setAvailability(fa[i]);
    // Update folders progress in the tree
    m_rootItem->recalculateProgress();
    emit dataChanged(index(0, 0), index((rowCount() - 1), (columnCount() - 1)));
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
    if (parent.isValid())
        return this->citem(parent)->columnCount();

    return m_rootItem->columnCount();
}

bool TorrentContentModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid())
        return false;

    if ((index.column() == TorrentContentModelItem::COL_NAME) && (role == Qt::CheckStateRole))
    {
        auto *item = this->item(index);
        qDebug("setData(%s, %d)", qUtf8Printable(item->name()), value.toInt());

        BitTorrent::DownloadPriority prio = BitTorrent::DownloadPriority::Normal;
        if (value.toInt() == Qt::PartiallyChecked)
            prio = BitTorrent::DownloadPriority::Mixed;
        else if (value.toInt() == Qt::Unchecked)
            prio = BitTorrent::DownloadPriority::Ignored;

        if (item->priority() != prio)
        {
            item->setPriority(prio);
            // Update folders progress in the tree
            m_rootItem->recalculateProgress();
            m_rootItem->recalculateAvailability();
            emit dataChanged(this->index(0, 0), this->index((rowCount() - 1), (columnCount() - 1)));
            emit prioritiesChanged();
        }
        return true;
    }

    if (role == Qt::EditRole)
    {
        Q_ASSERT(index.isValid());
        auto *item = this->item(index);
        switch (index.column())
        {
        case TorrentContentModelItem::COL_NAME:
            item->setName(value.toString());
            break;
        case TorrentContentModelItem::COL_PRIO:
            item->setPriority(static_cast<BitTorrent::DownloadPriority>(value.toInt()));
            break;
        default:
            return false;
        }
        emit dataChanged(index, index);

        // This isn't as efficient as it could be, since prioritiesChanged will be emitted many
        // times if a bulk operation such as "prioritize in displayed order" is executed. However,
        // the "select all" and "select none" buttons historically emitted this signal for every
        // item in the torrent without major performance issues.
        if (index.column() == TorrentContentModelItem::COL_PRIO)
            emit prioritiesChanged();

        return true;
    }

    return false;
}

const TorrentContentModelItem *TorrentContentModel::citem(const QModelIndex &index) const
{
    return static_cast<const TorrentContentModelItem *>(
        index.isValid() ? index.internalPointer() : m_rootItem
        );
}

TorrentContentModelItem *TorrentContentModel::item(const QModelIndex &index)
{
    return static_cast<TorrentContentModelItem *>(
        index.isValid() ? index.internalPointer() : m_rootItem
        );
}

int TorrentContentModel::getFileIndex(const QModelIndex &index) const
{
    const TorrentContentModelItem *item = this->citem(index);
    if (item->itemType() == TorrentContentModelItem::FileType)
        return static_cast<const TorrentContentModelFile*>(item)->fileIndex();

    Q_ASSERT(item->itemType() == TorrentContentModelItem::FileType);
    return -1;
}

QVariant TorrentContentModel::data(const QModelIndex &index, const int role) const
{
    if (!index.isValid())
        return {};

    auto *item = this->citem(index);

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
        return {};
    }
}

Qt::ItemFlags TorrentContentModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::ItemIsDropEnabled; // TODO: can we handle top-level drops?

    Qt::ItemFlags flags {Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsDragEnabled};
    if (citem(index)->itemType() == TorrentContentModelItem::FolderType)
        flags |= Qt::ItemIsAutoTristate | Qt::ItemIsDropEnabled;
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

QModelIndex TorrentContentModel::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid() && (parent.column() != 0))
        return {};

    if (column >= TorrentContentModelItem::NB_COL)
        return {};

    TorrentContentModelFolder *parentItem;
    if (!parent.isValid())
        parentItem = m_rootItem;
    else
        parentItem = static_cast<TorrentContentModelFolder*>(parent.internalPointer());
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

    auto *childItem = this->citem(index);
    if (!childItem)
        return {};

    TorrentContentModelItem *parentItem = childItem->parent();
    if (parentItem == m_rootItem)
        return {};

    return createIndex(parentItem->row(), 0, parentItem);
}

int TorrentContentModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;

    const TorrentContentModelFolder *parentItem;
    if (!parent.isValid())
        parentItem = m_rootItem;
    else
        parentItem = dynamic_cast<const TorrentContentModelFolder*>(this->citem(parent));

    return parentItem ? parentItem->childCount() : 0;
}

// Mime data binary format: File index, followed by stub of path that will be preserved on rename
QMimeData *TorrentContentModel::mimeData(const QModelIndexList &indexes) const
{
    // folders take precedence over files in the sense that if a folder and some files inside of it
    // are selected, the whole folder is moved and the folder structure is preserved.
    QHash<int, QString> stubs;
    QSet<QString> foldersProcessed;
    std::function<void (const TorrentContentModelItem *, const QString &)> addStubs;
    addStubs = [&stubs, &foldersProcessed, &addStubs](const TorrentContentModelItem *item, const QString &stubSoFar)
        {
            switch (item->itemType())
            {
            case TorrentContentModelItem::ItemType::FileType:
            {
                // don't override existing put there by folders
                int fileIndex = dynamic_cast<const TorrentContentModelFile *>(item)->fileIndex();
                if (!stubSoFar.isEmpty() || !stubs.contains(fileIndex))
                {
                    stubs.insert(fileIndex, Utils::Fs::combinePaths(stubSoFar, item->name()));
                }
                break;
            }
            case TorrentContentModelItem::ItemType::FolderType:
            {
                // ok to override existing put there by files
                // if any parent folders have been processed, they take precedence
                for (const QString &parent : Utils::Fs::parentFolders(item->path()))
                {
                    if (foldersProcessed.contains(parent) && stubSoFar.isEmpty())
                        return;
                }

                for (const TorrentContentModelItem *child : dynamic_cast<const TorrentContentModelFolder *>(item)->children())
                {
                    addStubs(child, Utils::Fs::combinePaths(stubSoFar, item->name()));
                }
                foldersProcessed.insert(item->path());
                break;
            }
            }
        };
    for (const QModelIndex &index : indexes)
    {
        addStubs(citem(index), "");
    }

    QByteArray encoded;
    QDataStream stream(&encoded, QIODevice::WriteOnly);
    for (int fileIndex : stubs.keys())
    {
        stream << fileIndex;
        stream << stubs.value(fileIndex);
    }
    QMimeData *result = new QMimeData();
    result->setData(QB_DRAGNDROP_MIME, encoded);
    return result;
}

bool TorrentContentModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int, int, const QModelIndex &parent)
{
    if (!data->hasFormat(QB_DRAGNDROP_MIME) || action != Qt::DropAction::MoveAction)
        return false;

    const QString newParentPath = parent.isValid() ? item(parent)->path() : "";
    QByteArray encoded = data->data(QB_DRAGNDROP_MIME);
    QDataStream stream(&encoded, QIODevice::ReadOnly);

    BitTorrent::AbstractFileStorage::RenameList renameList;

    while (!stream.atEnd())
    {
        int fileIndex;
        QString stub;
        stream >> fileIndex;
        stream >> stub;
        qDebug("drop %d %s", fileIndex, qUtf8Printable(stub));
        renameList.insert(fileIndex, Utils::Fs::combinePaths(newParentPath, stub));
    }

    emit filesDropped(renameList);

    return true;
}

bool TorrentContentModel::canDropMimeData(const QMimeData *mimeData, Qt::DropAction, int, int, const QModelIndex &parent) const
{
    return !parent.isValid() // root is OK
        || (mimeData->hasFormat(QB_DRAGNDROP_MIME) && citem(parent)->itemType() == TorrentContentModelItem::FolderType);
}

Qt::DropActions TorrentContentModel::supportedDropActions() const
{
    return Qt::DropAction::MoveAction;
}

void TorrentContentModel::clear()
{
    qDebug("clear called");
    beginResetModel();
    m_filesIndex.clear();
    delete m_rootItem;
    initializeRootItem();
    endResetModel();
}

void TorrentContentModel::setupModelData(const BitTorrent::AbstractFileStorage &info)
{
    qDebug("setup model data called");

    // helper function: Change all indexes for the given row, the arguments being the indexes of
    // the rows to be operated on, with column zero.
    auto changePersistentRowIndexes = [this](const QModelIndex &oldIndex, const QModelIndex &newIndex)
        {
            Q_ASSERT(columnCount(oldIndex.parent()) == columnCount(newIndex.parent()));

            for (int i = 0; i < columnCount(oldIndex.parent()); i++)
            {
                changePersistentIndex(index(oldIndex.row(), i, oldIndex.parent()),
                                      index(newIndex.row(), i, newIndex.parent()));
            }
        };

    const int filesCount = info.filesCount();
    if (filesCount <= 0)
        return;
    qDebug("Torrent contains %d files", filesCount);

    bool haveFileIndexes = !m_filesIndex.isEmpty();

    // Map from each file and folder to the model index, so that we can call ChangePersistentIndex appropriately later
    // TODO: should we store pointers to the indexes instead of directly?
    QVector<QModelIndex> oldFileModelIndexes;
    QMap<QString, QModelIndex> oldFolderModelIndexes;
    oldFileModelIndexes.resize(filesCount);
    std::function<void (const QModelIndex &parent)> addIndexes;
    // TODO: when adding relocate, we'll need to handle the folder name of the root differently
    addIndexes = [this, &oldFileModelIndexes, &oldFolderModelIndexes, &addIndexes](const QModelIndex &parent)
        {
            TorrentContentModelItem *item = this->item(parent);
            switch (item->itemType())
            {
            case TorrentContentModelItem::FileType:
            {
                TorrentContentModelFile *fileItem = dynamic_cast<TorrentContentModelFile *>(item);
                oldFileModelIndexes[fileItem->fileIndex()] = parent;
            }
            break;
            case TorrentContentModelItem::FolderType:
            {
                if (parent.isValid())
                    oldFolderModelIndexes.insert(item->path(), parent);
                for (int i = 0; i < this->rowCount(parent); i++)
                {
                    addIndexes(this->index(i, 0, parent));
                }
            }
            break;
            }
        };
    addIndexes(QModelIndex());

    emit layoutAboutToBeChanged();

    // cannot delete m_rootItem because the old indexes store pointers into it. Once all the indexes
    // have been changed, we delete it.
    TorrentContentModelFolder *oldRootItem = m_rootItem;
    initializeRootItem();
    m_filesIndex.resize(filesCount);

    // Iterate over files
    for (int i = 0; i < filesCount; ++i)
    {
        QModelIndex currentFolderIndex;
        TorrentContentModelFolder *currentFolderItem = dynamic_cast<TorrentContentModelFolder *>(item(currentFolderIndex));
        const QString path = Utils::Fs::toUniformPath(info.filePath(i));

        // Iterate of parts of the path to create necessary folders
        QVector<QString> pathFolders = Utils::Fs::parentFolders(path);

        for (const QString &parentFolderPath : pathFolders)
        {
            QString parentFolderName = Utils::Fs::fileName(parentFolderPath);
            auto currentFolderChildren = currentFolderItem->children();
            auto childFolderIt = std::find_if(currentFolderChildren.cbegin(), currentFolderChildren.cend(),
                                              [&parentFolderName](const TorrentContentModelItem *item) -> bool {
                                                  return item->name() == parentFolderName;
                                              });
            int childFolderRow = childFolderIt - currentFolderChildren.cbegin();
            if (childFolderIt == currentFolderChildren.cend())
            {
                // need to create new folder
                auto *newParent = new TorrentContentModelFolder(parentFolderName, currentFolderItem);
                currentFolderItem->appendChild(newParent);
            }
            currentFolderIndex = index(childFolderRow, 0, currentFolderIndex);
            currentFolderItem = dynamic_cast<TorrentContentModelFolder *>(item(currentFolderIndex));

            auto oldIndexIt = oldFolderModelIndexes.constFind(parentFolderPath);
            if (oldIndexIt != oldFolderModelIndexes.cend())
            {
                Q_ASSERT(oldIndexIt->isValid());
                Q_ASSERT(currentFolderIndex.isValid());
                changePersistentRowIndexes(*oldIndexIt, currentFolderIndex);
                oldFolderModelIndexes.remove(parentFolderPath);
            }
        }

        // Add the new file
        TorrentContentModelFile *fileItem = new TorrentContentModelFile(Utils::Fs::fileName(info.filePath(i))
                                                                        , info.fileSize(i), currentFolderItem, i);
        currentFolderItem->appendChild(fileItem);
        m_filesIndex[i] = fileItem;
        if (haveFileIndexes)
        {
            QModelIndex oldFileIndex = oldFileModelIndexes[i];
            QModelIndex newFileIndex = index(currentFolderItem->childCount() - 1, 0, currentFolderIndex);
            Q_ASSERT(oldFileIndex.isValid());
            Q_ASSERT(newFileIndex.isValid());
            changePersistentRowIndexes(oldFileIndex, newFileIndex);
        }
    }

    // remove deleted folders
    for (const QString &folderName : oldFolderModelIndexes.keys())
    {
        Q_ASSERT(oldFolderModelIndexes[folderName].isValid());
        changePersistentRowIndexes(oldFolderModelIndexes[folderName], QModelIndex());
    }

    delete oldRootItem;

    emit layoutChanged();
}

void TorrentContentModel::selectAll()
{
    for (int i = 0; i < m_rootItem->childCount(); ++i)
    {
        TorrentContentModelItem* child = m_rootItem->child(i);
        if (child->priority() == BitTorrent::DownloadPriority::Ignored)
            child->setPriority(BitTorrent::DownloadPriority::Normal);
    }
    emit dataChanged(index(0, 0), index((rowCount() - 1), (columnCount() - 1)));
}

void TorrentContentModel::selectNone()
{
    for (int i = 0; i < m_rootItem->childCount(); ++i)
        m_rootItem->child(i)->setPriority(BitTorrent::DownloadPriority::Ignored);
    emit dataChanged(index(0, 0), index((rowCount() - 1), (columnCount() - 1)));
}
