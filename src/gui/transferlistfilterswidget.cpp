/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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

#include "transferlistfilterswidget.h"

#include <QCheckBox>
#include <QDebug>
#include <QIcon>
#include <QListWidgetItem>
#include <QMenu>
#include <QScrollArea>
#include <QUrl>
#include <QVBoxLayout>

#include "base/bittorrent/session.h"
#include "base/bittorrent/torrenthandle.h"
#include "base/bittorrent/trackerentry.h"
#include "base/global.h"
#include "base/logger.h"
#include "base/net/downloadhandler.h"
#include "base/net/downloadmanager.h"
#include "base/preferences.h"
#include "base/torrentfilter.h"
#include "base/utils/fs.h"
#include "base/utils/string.h"
#include "autoexpandabledialog.h"
#include "categoryfilterwidget.h"
#include "guiiconprovider.h"
#include "tagfilterwidget.h"
#include "transferlistdelegate.h"
#include "transferlistmodel.h"
#include "transferlistwidget.h"
#include "utils.h"

namespace
{
    QString getScheme(const QString &tracker)
    {
        const QUrl url {tracker};
        QString scheme = url.scheme();
        if (scheme.isEmpty())
            scheme = "http";
        return scheme;
    }
}

BaseFilterWidget::BaseFilterWidget(QWidget *parent, TransferListWidget *transferList)
    : QListWidget(parent)
    , transferList(transferList)
{
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setUniformItemSizes(true);
    setSpacing(0);

    setIconSize(Utils::Gui::smallIconSize());

#if defined(Q_OS_MAC)
    setAttribute(Qt::WA_MacShowFocusRect, false);
#endif

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &BaseFilterWidget::customContextMenuRequested, this, &BaseFilterWidget::showMenu);
    connect(this, &BaseFilterWidget::currentRowChanged, this, &BaseFilterWidget::applyFilter);

    connect(BitTorrent::Session::instance(), &BitTorrent::Session::torrentAdded
            , this, &BaseFilterWidget::handleNewTorrent);
    connect(BitTorrent::Session::instance(), &BitTorrent::Session::torrentAboutToBeRemoved
            , this, &BaseFilterWidget::torrentAboutToBeDeleted);
}

QSize BaseFilterWidget::sizeHint() const
{
    return {
        // Width should be exactly the width of the content
        sizeHintForColumn(0),
        // Height should be exactly the height of the content
        static_cast<int>((sizeHintForRow(0) + 2 * spacing()) * (count() + 0.5)),
    };
}

QSize BaseFilterWidget::minimumSizeHint() const
{
    QSize size = sizeHint();
    size.setWidth(6);
    return size;
}

void BaseFilterWidget::toggleFilter(bool checked)
{
    setVisible(checked);
    if (checked)
        applyFilter(currentRow());
    else
        applyFilter(0);
}

StatusFilterWidget::StatusFilterWidget(QWidget *parent, TransferListWidget *transferList)
    : BaseFilterWidget(parent, transferList)
{
    connect(BitTorrent::Session::instance(), &BitTorrent::Session::torrentsUpdated
            , this, &StatusFilterWidget::updateTorrentNumbers);

    // Add status filters
    QListWidgetItem *all = new QListWidgetItem(this);
    all->setData(Qt::DisplayRole, QVariant(tr("All (0)", "this is for the status filter")));
    all->setData(Qt::DecorationRole, QIcon(":/icons/skin/filterall.svg"));
    QListWidgetItem *downloading = new QListWidgetItem(this);
    downloading->setData(Qt::DisplayRole, QVariant(tr("Downloading (0)")));
    downloading->setData(Qt::DecorationRole, QIcon(":/icons/skin/downloading.svg"));
    QListWidgetItem *seeding = new QListWidgetItem(this);
    seeding->setData(Qt::DisplayRole, QVariant(tr("Seeding (0)")));
    seeding->setData(Qt::DecorationRole, QIcon(":/icons/skin/uploading.svg"));
    QListWidgetItem *completed = new QListWidgetItem(this);
    completed->setData(Qt::DisplayRole, QVariant(tr("Completed (0)")));
    completed->setData(Qt::DecorationRole, QIcon(":/icons/skin/completed.svg"));
    QListWidgetItem *resumed = new QListWidgetItem(this);
    resumed->setData(Qt::DisplayRole, QVariant(tr("Resumed (0)")));
    resumed->setData(Qt::DecorationRole, QIcon(":/icons/skin/resumed.svg"));
    QListWidgetItem *paused = new QListWidgetItem(this);
    paused->setData(Qt::DisplayRole, QVariant(tr("Paused (0)")));
    paused->setData(Qt::DecorationRole, QIcon(":/icons/skin/paused.svg"));
    QListWidgetItem *active = new QListWidgetItem(this);
    active->setData(Qt::DisplayRole, QVariant(tr("Active (0)")));
    active->setData(Qt::DecorationRole, QIcon(":/icons/skin/filteractive.svg"));
    QListWidgetItem *inactive = new QListWidgetItem(this);
    inactive->setData(Qt::DisplayRole, QVariant(tr("Inactive (0)")));
    inactive->setData(Qt::DecorationRole, QIcon(":/icons/skin/filterinactive.svg"));
    QListWidgetItem *errored = new QListWidgetItem(this);
    errored->setData(Qt::DisplayRole, QVariant(tr("Errored (0)")));
    errored->setData(Qt::DecorationRole, QIcon(":/icons/skin/error.svg"));

    const Preferences *const pref = Preferences::instance();
    setCurrentRow(pref->getTransSelFilter(), QItemSelectionModel::SelectCurrent);
    toggleFilter(pref->getStatusFilterState());
}

StatusFilterWidget::~StatusFilterWidget()
{
    Preferences::instance()->setTransSelFilter(currentRow());
}

void StatusFilterWidget::updateTorrentNumbers()
{
    auto report = BitTorrent::Session::instance()->torrentStatusReport();

    item(TorrentFilter::All)->setData(Qt::DisplayRole, QVariant(tr("All (%1)").arg(report.nbActive + report.nbInactive)));
    item(TorrentFilter::Downloading)->setData(Qt::DisplayRole, QVariant(tr("Downloading (%1)").arg(report.nbDownloading)));
    item(TorrentFilter::Seeding)->setData(Qt::DisplayRole, QVariant(tr("Seeding (%1)").arg(report.nbSeeding)));
    item(TorrentFilter::Completed)->setData(Qt::DisplayRole, QVariant(tr("Completed (%1)").arg(report.nbCompleted)));
    item(TorrentFilter::Paused)->setData(Qt::DisplayRole, QVariant(tr("Paused (%1)").arg(report.nbPaused)));
    item(TorrentFilter::Resumed)->setData(Qt::DisplayRole, QVariant(tr("Resumed (%1)").arg(report.nbResumed)));
    item(TorrentFilter::Active)->setData(Qt::DisplayRole, QVariant(tr("Active (%1)").arg(report.nbActive)));
    item(TorrentFilter::Inactive)->setData(Qt::DisplayRole, QVariant(tr("Inactive (%1)").arg(report.nbInactive)));
    item(TorrentFilter::Errored)->setData(Qt::DisplayRole, QVariant(tr("Errored (%1)").arg(report.nbErrored)));
}

void StatusFilterWidget::showMenu(QPoint) {}

void StatusFilterWidget::applyFilter(int row)
{
    transferList->applyStatusFilter(row);
}

void StatusFilterWidget::handleNewTorrent(BitTorrent::TorrentHandle *const) {}

void StatusFilterWidget::torrentAboutToBeDeleted(BitTorrent::TorrentHandle *const) {}

TrackerFiltersList::TrackerFiltersList(QWidget *parent, TransferListWidget *transferList, const bool downloadFavicon)
    : BaseFilterWidget(parent, transferList)
    , m_totalTorrents(0)
    , m_downloadTrackerFavicon(downloadFavicon)
{
    QListWidgetItem *allTrackers = new QListWidgetItem(this);
    allTrackers->setData(Qt::DisplayRole, QVariant(tr("All (0)", "this is for the tracker filter")));
    allTrackers->setData(Qt::DecorationRole, GuiIconProvider::instance()->getIcon("network-server"));
    QListWidgetItem *noTracker = new QListWidgetItem(this);
    noTracker->setData(Qt::DisplayRole, QVariant(tr("Trackerless (0)")));
    noTracker->setData(Qt::DecorationRole, GuiIconProvider::instance()->getIcon("network-server"));
    QListWidgetItem *errorTracker = new QListWidgetItem(this);
    errorTracker->setData(Qt::DisplayRole, QVariant(tr("Error (0)")));
    errorTracker->setData(Qt::DecorationRole, style()->standardIcon(QStyle::SP_MessageBoxCritical));
    QListWidgetItem *warningTracker = new QListWidgetItem(this);
    warningTracker->setData(Qt::DisplayRole, QVariant(tr("Warning (0)")));
    warningTracker->setData(Qt::DecorationRole, style()->standardIcon(QStyle::SP_MessageBoxWarning));
    m_trackers.insert("", QStringList());

    setCurrentRow(0, QItemSelectionModel::SelectCurrent);
    toggleFilter(Preferences::instance()->getTrackerFilterState());
}

TrackerFiltersList::~TrackerFiltersList()
{
    for (const QString &iconPath : asConst(m_iconPaths))
        Utils::Fs::forceRemove(iconPath);
}

void TrackerFiltersList::addItem(const QString &tracker, const QString &hash)
{
    QStringList tmp;
    QListWidgetItem *trackerItem = nullptr;
    QString host = getHost(tracker);
    bool exists = m_trackers.contains(host);

    if (exists) {
        tmp = m_trackers.value(host);
        if (tmp.contains(hash))
            return;

        if (host != "") {
            trackerItem = item(rowFromTracker(host));
        }
        else {
            trackerItem = item(1);
        }
    }
    else {
        trackerItem = new QListWidgetItem();
        trackerItem->setData(Qt::DecorationRole, GuiIconProvider::instance()->getIcon("network-server"));

        const QString scheme = getScheme(tracker);
        downloadFavicon(QString("%1://%2/favicon.ico").arg((scheme.startsWith("http") ? scheme : "http"), host));
    }
    if (!trackerItem) return;

    tmp.append(hash);
    m_trackers.insert(host, tmp);
    if (host == "") {
        trackerItem->setText(tr("Trackerless (%1)").arg(tmp.size()));
        if (currentRow() == 1)
            applyFilter(1);
        return;
    }

    trackerItem->setText(QString("%1 (%2)").arg(host).arg(tmp.size()));
    if (exists) {
        if (currentRow() == rowFromTracker(host))
            applyFilter(currentRow());
        return;
    }

    Q_ASSERT(count() >= 4);
    int insPos = count();
    for (int i = 4; i < count(); ++i) {
        if (Utils::String::naturalLessThan<Qt::CaseSensitive>(host, item(i)->text())) {
            insPos = i;
            break;
        }
    }
    QListWidget::insertItem(insPos, trackerItem);
    updateGeometry();
}

void TrackerFiltersList::removeItem(const QString &tracker, const QString &hash)
{
    QString host = getHost(tracker);
    QListWidgetItem *trackerItem = nullptr;
    QStringList tmp = m_trackers.value(host);
    int row = 0;

    if (tmp.empty())
        return;
    tmp.removeAll(hash);

    if (!host.isEmpty()) {
        // Remove from 'Error' and 'Warning' view
        trackerSuccess(hash, tracker);
        row = rowFromTracker(host);
        trackerItem = item(row);
        if (tmp.empty()) {
            if (currentRow() == row)
                setCurrentRow(0, QItemSelectionModel::SelectCurrent);
            delete trackerItem;
            m_trackers.remove(host);
            updateGeometry();
            return;
        }
        if (trackerItem != nullptr)
            trackerItem->setText(QString("%1 (%2)").arg(host).arg(tmp.size()));
    }
    else {
        row = 1;
        trackerItem = item(1);
        trackerItem->setText(tr("Trackerless (%1)").arg(tmp.size()));
    }

    m_trackers.insert(host, tmp);
    if (currentRow() == row)
        applyFilter(row);
}

void TrackerFiltersList::changeTrackerless(bool trackerless, const QString &hash)
{
    if (trackerless)
        addItem("", hash);
    else
        removeItem("", hash);
}

void TrackerFiltersList::setDownloadTrackerFavicon(bool value)
{
    if (value == m_downloadTrackerFavicon) return;
    m_downloadTrackerFavicon = value;

    if (m_downloadTrackerFavicon) {
        for (auto i = m_trackers.cbegin(); i != m_trackers.cend(); ++i) {
            const QString &tracker = i.key();
            if (!tracker.isEmpty()) {
                const QString scheme = getScheme(tracker);
                downloadFavicon(QString("%1://%2/favicon.ico")
                                .arg((scheme.startsWith("http") ? scheme : "http"), getHost(tracker)));
             }
        }
    }
}

void TrackerFiltersList::trackerSuccess(const QString &hash, const QString &tracker)
{
    QStringList errored = m_errors.value(hash);
    QStringList warned = m_warnings.value(hash);

    if (errored.contains(tracker)) {
        errored.removeAll(tracker);
        if (errored.empty()) {
            m_errors.remove(hash);
            item(2)->setText(tr("Error (%1)").arg(m_errors.size()));
            if (currentRow() == 2)
                applyFilter(2);
        }
        else {
            m_errors.insert(hash, errored);
        }
    }

    if (warned.contains(tracker)) {
        warned.removeAll(tracker);
        if (warned.empty()) {
            m_warnings.remove(hash);
            item(3)->setText(tr("Warning (%1)").arg(m_warnings.size()));
            if (currentRow() == 3)
                applyFilter(3);
        }
        else {
            m_warnings.insert(hash, warned);
        }
    }
}

void TrackerFiltersList::trackerError(const QString &hash, const QString &tracker)
{
    QStringList trackers = m_errors.value(hash);

    if (trackers.contains(tracker))
        return;

    trackers.append(tracker);
    m_errors.insert(hash, trackers);
    item(2)->setText(tr("Error (%1)").arg(m_errors.size()));

    if (currentRow() == 2)
        applyFilter(2);
}

void TrackerFiltersList::trackerWarning(const QString &hash, const QString &tracker)
{
    QStringList trackers = m_warnings.value(hash);

    if (trackers.contains(tracker))
        return;

    trackers.append(tracker);
    m_warnings.insert(hash, trackers);
    item(3)->setText(tr("Warning (%1)").arg(m_warnings.size()));

    if (currentRow() == 3)
        applyFilter(3);
}

void TrackerFiltersList::downloadFavicon(const QString &url)
{
    if (!m_downloadTrackerFavicon) return;
    Net::DownloadHandler *h = Net::DownloadManager::instance()->download(
                Net::DownloadRequest(url).saveToFile(true));
    using Func = void (Net::DownloadHandler::*)(const QString &, const QString &);
    connect(h, static_cast<Func>(&Net::DownloadHandler::downloadFinished), this
            , &TrackerFiltersList::handleFavicoDownload);
    connect(h, static_cast<Func>(&Net::DownloadHandler::downloadFailed), this
            , &TrackerFiltersList::handleFavicoFailure);
}

void TrackerFiltersList::handleFavicoDownload(const QString &url, const QString &filePath)
{
    const QString host = getHost(url);

    if (!m_trackers.contains(host)) {
        Utils::Fs::forceRemove(filePath);
        return;
    }

    QListWidgetItem *trackerItem = item(rowFromTracker(host));
    if (!trackerItem) return;

    QIcon icon(filePath);
    //Detect a non-decodable icon
    QList<QSize> sizes = icon.availableSizes();
    bool invalid = (sizes.isEmpty() || icon.pixmap(sizes.first()).isNull());
    if (invalid) {
        if (url.endsWith(".ico", Qt::CaseInsensitive))
            downloadFavicon(url.left(url.size() - 4) + ".png");
        Utils::Fs::forceRemove(filePath);
    }
    else {
        trackerItem->setData(Qt::DecorationRole, QVariant(QIcon(filePath)));
        m_iconPaths.append(filePath);
    }
}

void TrackerFiltersList::handleFavicoFailure(const QString &url, const QString &error)
{
    Q_UNUSED(error)
    if (url.endsWith(".ico", Qt::CaseInsensitive))
        downloadFavicon(url.left(url.size() - 4) + ".png");
}

void TrackerFiltersList::showMenu(QPoint)
{
    QMenu menu(this);
    QAction *startAct = menu.addAction(GuiIconProvider::instance()->getIcon("media-playback-start"), tr("Resume torrents"));
    QAction *pauseAct = menu.addAction(GuiIconProvider::instance()->getIcon("media-playback-pause"), tr("Pause torrents"));
    QAction *deleteTorrentsAct = menu.addAction(GuiIconProvider::instance()->getIcon("edit-delete"), tr("Delete torrents"));
    QAction *act = nullptr;
    act = menu.exec(QCursor::pos());

    if (!act)
        return;

    if (act == startAct)
        transferList->startVisibleTorrents();
    else if (act == pauseAct)
        transferList->pauseVisibleTorrents();
    else if (act == deleteTorrentsAct)
        transferList->deleteVisibleTorrents();
}

void TrackerFiltersList::applyFilter(int row)
{
    if (row == 0)
        transferList->applyTrackerFilterAll();
    else if (isVisible())
        transferList->applyTrackerFilter(getHashes(row));
}

void TrackerFiltersList::handleNewTorrent(BitTorrent::TorrentHandle *const torrent)
{
    QString hash = torrent->hash();
    const QList<BitTorrent::TrackerEntry> trackers = torrent->trackers();
    for (const BitTorrent::TrackerEntry &tracker : trackers)
        addItem(tracker.url(), hash);

    //Check for trackerless torrent
    if (trackers.size() == 0)
        addItem("", hash);

    item(0)->setText(tr("All (%1)", "this is for the tracker filter").arg(++m_totalTorrents));
}

void TrackerFiltersList::torrentAboutToBeDeleted(BitTorrent::TorrentHandle *const torrent)
{
    QString hash = torrent->hash();
    const QList<BitTorrent::TrackerEntry> trackers = torrent->trackers();
    for (const BitTorrent::TrackerEntry &tracker : trackers)
        removeItem(tracker.url(), hash);

    //Check for trackerless torrent
    if (trackers.size() == 0)
        removeItem("", hash);

    item(0)->setText(tr("All (%1)", "this is for the tracker filter").arg(--m_totalTorrents));
}

QString TrackerFiltersList::trackerFromRow(int row) const
{
    Q_ASSERT(row > 1);
    const QString tracker = item(row)->text();
    QStringList parts = tracker.split(' ');
    Q_ASSERT(parts.size() >= 2);
    parts.removeLast(); // Remove trailing number
    return parts.join(' ');
}

int TrackerFiltersList::rowFromTracker(const QString &tracker) const
{
    Q_ASSERT(!tracker.isEmpty());
    for (int i = 4; i<count(); ++i)
        if (tracker == trackerFromRow(i)) return i;
    return -1;
}

QString TrackerFiltersList::getHost(const QString &tracker) const
{
    QUrl url(tracker);
    QString longHost = url.host();
    QString tld = url.topLevelDomain();
    // We get empty tld when it is invalid or an IPv4/IPv6 address,
    // so just return the full host
    if (tld.isEmpty())
        return longHost;
    // We want the domain + tld. Subdomains should be disregarded
    int index = longHost.lastIndexOf('.', -(tld.size() + 1));
    if (index == -1)
        return longHost;
    return longHost.mid(index + 1);
}

QStringList TrackerFiltersList::getHashes(int row)
{
    if (row == 1)
        return m_trackers.value("");
    else if (row == 2)
        return m_errors.keys();
    else if (row == 3)
        return m_warnings.keys();
    else
        return m_trackers.value(trackerFromRow(row));
}

TransferListFiltersWidget::TransferListFiltersWidget(QWidget *parent, TransferListWidget *transferList, const bool downloadFavicon)
    : QFrame(parent)
    , m_transferList(transferList)
{
    Preferences *const pref = Preferences::instance();

    // Construct lists
    QVBoxLayout *vLayout = new QVBoxLayout(this);
    QScrollArea *scroll = new QScrollArea(this);
    QFrame *frame = new QFrame(scroll);
    QVBoxLayout *frameLayout = new QVBoxLayout(frame);
    QFont font;
    font.setBold(true);
    font.setCapitalization(QFont::AllUppercase);

    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    setStyleSheet("QFrame {background: transparent;}");
    scroll->setStyleSheet("QFrame {border: none;}");
    vLayout->setContentsMargins(0, 0, 0, 0);
    frameLayout->setContentsMargins(0, 2, 0, 0);
    frameLayout->setSpacing(2);
    frameLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    frame->setLayout(frameLayout);
    scroll->setWidget(frame);
    vLayout->addWidget(scroll);
    setLayout(vLayout);

    QCheckBox *statusLabel = new QCheckBox(tr("Status"), this);
    statusLabel->setChecked(pref->getStatusFilterState());
    statusLabel->setFont(font);
    frameLayout->addWidget(statusLabel);

    StatusFilterWidget *statusFilters = new StatusFilterWidget(this, transferList);
    frameLayout->addWidget(statusFilters);

    QCheckBox *categoryLabel = new QCheckBox(tr("Categories"), this);
    categoryLabel->setChecked(pref->getCategoryFilterState());
    categoryLabel->setFont(font);
    connect(categoryLabel, &QCheckBox::toggled, this
            , &TransferListFiltersWidget::onCategoryFilterStateChanged);
    frameLayout->addWidget(categoryLabel);

    m_categoryFilterWidget = new CategoryFilterWidget(this);
    connect(m_categoryFilterWidget, &CategoryFilterWidget::actionDeleteTorrentsTriggered
            , transferList, &TransferListWidget::deleteVisibleTorrents);
    connect(m_categoryFilterWidget, &CategoryFilterWidget::actionPauseTorrentsTriggered
            , transferList, &TransferListWidget::pauseVisibleTorrents);
    connect(m_categoryFilterWidget, &CategoryFilterWidget::actionResumeTorrentsTriggered
            , transferList, &TransferListWidget::startVisibleTorrents);
    connect(m_categoryFilterWidget, &CategoryFilterWidget::categoryChanged
            , transferList, &TransferListWidget::applyCategoryFilter);
    toggleCategoryFilter(pref->getCategoryFilterState());
    frameLayout->addWidget(m_categoryFilterWidget);

    QCheckBox *tagsLabel = new QCheckBox(tr("Tags"), this);
    tagsLabel->setChecked(pref->getTagFilterState());
    tagsLabel->setFont(font);
    connect(tagsLabel, &QCheckBox::toggled, this, &TransferListFiltersWidget::onTagFilterStateChanged);
    frameLayout->addWidget(tagsLabel);

    m_tagFilterWidget = new TagFilterWidget(this);
    connect(m_tagFilterWidget, &TagFilterWidget::actionDeleteTorrentsTriggered
            , transferList, &TransferListWidget::deleteVisibleTorrents);
    connect(m_tagFilterWidget, &TagFilterWidget::actionPauseTorrentsTriggered
            , transferList, &TransferListWidget::pauseVisibleTorrents);
    connect(m_tagFilterWidget, &TagFilterWidget::actionResumeTorrentsTriggered
            , transferList, &TransferListWidget::startVisibleTorrents);
    connect(m_tagFilterWidget, &TagFilterWidget::tagChanged
            , transferList, &TransferListWidget::applyTagFilter);
    toggleTagFilter(pref->getTagFilterState());
    frameLayout->addWidget(m_tagFilterWidget);

    QCheckBox *trackerLabel = new QCheckBox(tr("Trackers"), this);
    trackerLabel->setChecked(pref->getTrackerFilterState());
    trackerLabel->setFont(font);
    frameLayout->addWidget(trackerLabel);

    m_trackerFilters = new TrackerFiltersList(this, transferList, downloadFavicon);
    frameLayout->addWidget(m_trackerFilters);

    connect(statusLabel, &QCheckBox::toggled, statusFilters, &StatusFilterWidget::toggleFilter);
    connect(statusLabel, &QCheckBox::toggled, pref, &Preferences::setStatusFilterState);
    connect(trackerLabel, &QCheckBox::toggled, m_trackerFilters, &TrackerFiltersList::toggleFilter);
    connect(trackerLabel, &QCheckBox::toggled, pref, &Preferences::setTrackerFilterState);

    using Func = void (TransferListFiltersWidget::*)(const QString&, const QString&);
    connect(this, static_cast<Func>(&TransferListFiltersWidget::trackerSuccess)
            , m_trackerFilters, &TrackerFiltersList::trackerSuccess);
    connect(this, static_cast<Func>(&TransferListFiltersWidget::trackerError)
            , m_trackerFilters, &TrackerFiltersList::trackerError);
    connect(this, static_cast<Func>(&TransferListFiltersWidget::trackerWarning)
            , m_trackerFilters, &TrackerFiltersList::trackerWarning);
}

void TransferListFiltersWidget::setDownloadTrackerFavicon(bool value)
{
    m_trackerFilters->setDownloadTrackerFavicon(value);
}

void TransferListFiltersWidget::addTrackers(BitTorrent::TorrentHandle *const torrent, const QList<BitTorrent::TrackerEntry> &trackers)
{
    for (const BitTorrent::TrackerEntry &tracker : trackers)
        m_trackerFilters->addItem(tracker.url(), torrent->hash());
}

void TransferListFiltersWidget::removeTrackers(BitTorrent::TorrentHandle *const torrent, const QList<BitTorrent::TrackerEntry> &trackers)
{
    for (const BitTorrent::TrackerEntry &tracker : trackers)
        m_trackerFilters->removeItem(tracker.url(), torrent->hash());
}

void TransferListFiltersWidget::changeTrackerless(BitTorrent::TorrentHandle *const torrent, bool trackerless)
{
    m_trackerFilters->changeTrackerless(trackerless, torrent->hash());
}

void TransferListFiltersWidget::trackerSuccess(BitTorrent::TorrentHandle *const torrent, const QString &tracker)
{
    emit trackerSuccess(torrent->hash(), tracker);
}

void TransferListFiltersWidget::trackerWarning(BitTorrent::TorrentHandle *const torrent, const QString &tracker)
{
    emit trackerWarning(torrent->hash(), tracker);
}

void TransferListFiltersWidget::trackerError(BitTorrent::TorrentHandle *const torrent, const QString &tracker)
{
    emit trackerError(torrent->hash(), tracker);
}

void TransferListFiltersWidget::onCategoryFilterStateChanged(bool enabled)
{
    toggleCategoryFilter(enabled);
    Preferences::instance()->setCategoryFilterState(enabled);
}

void TransferListFiltersWidget::toggleCategoryFilter(bool enabled)
{
    m_categoryFilterWidget->setVisible(enabled);
    m_transferList->applyCategoryFilter(enabled ? m_categoryFilterWidget->currentCategory() : QString());
}

void TransferListFiltersWidget::onTagFilterStateChanged(bool enabled)
{
    toggleTagFilter(enabled);
    Preferences::instance()->setTagFilterState(enabled);
}

void TransferListFiltersWidget::toggleTagFilter(bool enabled)
{
    m_tagFilterWidget->setVisible(enabled);
    m_transferList->applyTagFilter(enabled ? m_tagFilterWidget->currentTag() : QString());
}
