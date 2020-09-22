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
#include <QtWin>
#else
#include <QMimeDatabase>
#include <QMimeType>
#endif

#if defined Q_OS_WIN || defined Q_OS_MACOS
#define QBT_PIXMAP_CACHE_FOR_FILE_ICONS
#include <QPixmapCache>
#endif

#include "base/bittorrent/downloadpriority.h"
#include "base/bittorrent/torrentinfo.h"
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
            if (!ext.isEmpty()) {
                QPixmap cached;
                if (QPixmapCache::find(ext, &cached)) return {cached};

                const QPixmap pixmap = pixmapForExtension(ext);
                if (!pixmap.isNull()) {
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

            QPixmap iconPixmap = QtWin::fromHICON(sfi.hIcon);
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
            if (!res.isNull()) {
                return res;
            }

            res = QIcon::fromTheme(mimeType.genericIconName());
            if (!res.isNull()) {
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
    , m_rootItem(new TorrentContentModelFolder(QVector<QVariant>({ tr("Name"), tr("Size"), tr("Progress"), tr("Download Priority"), tr("Remaining"), tr("Availability") })))
{
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
    emit dataChanged(index(0, 0), index((rowCount() - 1), (columnCount() - 1)));
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
    emit dataChanged(index(0, 0), index((rowCount() - 1), (columnCount() - 1)));
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
        return static_cast<TorrentContentModelItem*>(parent.internalPointer())->columnCount();

    return m_rootItem->columnCount();
}

bool TorrentContentModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid())
        return false;

    if ((index.column() == TorrentContentModelItem::COL_NAME) && (role == Qt::CheckStateRole)) {
        auto *item = static_cast<TorrentContentModelItem*>(index.internalPointer());
        qDebug("setData(%s, %d", qUtf8Printable(item->name()), value.toInt());
        if (static_cast<int>(item->priority()) != value.toInt()) {
            BitTorrent::DownloadPriority prio = BitTorrent::DownloadPriority::Normal;
            if (value.toInt() == Qt::PartiallyChecked)
                prio = BitTorrent::DownloadPriority::Mixed;
            else if (value.toInt() == Qt::Unchecked)
                prio = BitTorrent::DownloadPriority::Ignored;

            item->setPriority(prio);
            // Update folders progress in the tree
            m_rootItem->recalculateProgress();
            m_rootItem->recalculateAvailability();
            emit dataChanged(this->index(0, 0), this->index((rowCount() - 1), (columnCount() - 1)));
            emit filteredFilesChanged();
        }
        return true;
    }

    if (role == Qt::EditRole) {
        Q_ASSERT(index.isValid());
        auto *item = static_cast<TorrentContentModelItem*>(index.internalPointer());
        switch (index.column()) {
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
        return true;
    }

    return false;
}

TorrentContentModelItem::ItemType TorrentContentModel::itemType(const QModelIndex &index) const
{
    return static_cast<const TorrentContentModelItem*>(index.internalPointer())->itemType();
}

int TorrentContentModel::getFileIndex(const QModelIndex &index)
{
    auto *item = static_cast<TorrentContentModelItem*>(index.internalPointer());
    if (item->itemType() == TorrentContentModelItem::FileType)
        return static_cast<TorrentContentModelFile*>(item)->fileIndex();

    Q_ASSERT(item->itemType() == TorrentContentModelItem::FileType);
    return -1;
}

QVariant TorrentContentModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return {};

    auto *item = static_cast<TorrentContentModelItem*>(index.internalPointer());

    if ((index.column() == TorrentContentModelItem::COL_NAME) && (role == Qt::DecorationRole)) {
        if (item->itemType() == TorrentContentModelItem::FolderType)
            return m_fileIconProvider->icon(QFileIconProvider::Folder);

        return m_fileIconProvider->icon(QFileInfo(item->name()));
    }

    if ((index.column() == TorrentContentModelItem::COL_NAME) && (role == Qt::CheckStateRole)) {
        if (item->data(TorrentContentModelItem::COL_PRIO).toInt() == static_cast<int>(BitTorrent::DownloadPriority::Ignored))
            return Qt::Unchecked;
        if (item->data(TorrentContentModelItem::COL_PRIO).toInt() == static_cast<int>(BitTorrent::DownloadPriority::Mixed))
            return Qt::PartiallyChecked;
        return Qt::Checked;
    }

    if (role == Qt::DisplayRole)
        return item->data(index.column());

    return {};
}

Qt::ItemFlags TorrentContentModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    if (itemType(index) == TorrentContentModelItem::FolderType)
        return Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsTristate;

    return Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable;
}

QVariant TorrentContentModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if ((orientation == Qt::Horizontal) && (role == Qt::DisplayRole))
        return m_rootItem->data(section);

    return {};
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

    auto *childItem = static_cast<TorrentContentModelItem*>(index.internalPointer());
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

    TorrentContentModelFolder *parentItem;
    if (!parent.isValid())
        parentItem = m_rootItem;
    else
        parentItem = dynamic_cast<TorrentContentModelFolder*>(static_cast<TorrentContentModelItem*>(parent.internalPointer()));

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

void TorrentContentModel::setupModelData(const BitTorrent::TorrentInfo &info)
{
    qDebug("setup model data called");
    const int filesCount = info.filesCount();
    if (filesCount <= 0)
        return;

    emit layoutAboutToBeChanged();
    // Initialize files_index array
    qDebug("Torrent contains %d files", filesCount);
    m_filesIndex.reserve(filesCount);

    TorrentContentModelFolder *currentParent;
    // Iterate over files
    for (int i = 0; i < filesCount; ++i) {
        currentParent = m_rootItem;
        const QString path = Utils::Fs::toUniformPath(info.filePath(i));

        // Iterate of parts of the path to create necessary folders
        QVector<QStringRef> pathFolders = path.splitRef('/', QString::SkipEmptyParts);
        pathFolders.removeLast();

        for (const QStringRef &pathPartRef : asConst(pathFolders)) {
            const QString pathPart = pathPartRef.toString();
            TorrentContentModelFolder *newParent = currentParent->childFolderWithName(pathPart);
            if (!newParent) {
                newParent = new TorrentContentModelFolder(pathPart, currentParent);
                currentParent->appendChild(newParent);
            }
            currentParent = newParent;
        }
        // Actually create the file
        TorrentContentModelFile *fileItem = new TorrentContentModelFile(info.fileName(i), info.fileSize(i), currentParent, i);
        currentParent->appendChild(fileItem);
        m_filesIndex.push_back(fileItem);
    }
    emit layoutChanged();
}

void TorrentContentModel::selectAll()
{
    for (int i = 0; i < m_rootItem->childCount(); ++i) {
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
