/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016 qBittorrent project
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

#include "advancedsettings.h"

#include <limits>

#include <QHeaderView>
#include <QHostAddress>
#include <QLabel>
#include <QNetworkInterface>

#include "base/bittorrent/session.h"
#include "base/global.h"
#include "base/preferences.h"
#include "base/unicodestrings.h"
#include "app/application.h"
#include "gui/addnewtorrentdialog.h"
#include "gui/mainwindow.h"

namespace
{
    QString makeLink(const QString &url, const QString &linkLabel)
    {
         return QStringLiteral("<a href=\"%1\">%2</a>").arg(url, linkLabel);
    }

    enum AdvSettingsCols
    {
        PROPERTY,
        VALUE,
        COL_COUNT
    };

    enum AdvSettingsRows
    {
        // qBittorrent section
        QBITTORRENT_HEADER,
#if defined(Q_OS_WIN)
        OS_MEMORY_PRIORITY,
#endif
        // network interface
        NETWORK_IFACE,
        //Optional network address
        NETWORK_IFACE_ADDRESS,
        // behavior
        SAVE_RESUME_DATA_INTERVAL,
        CONFIRM_RECHECK_TORRENT,
        RECHECK_COMPLETED,
        // UI related
        LIST_REFRESH,
        RESOLVE_HOSTS,
        RESOLVE_COUNTRIES,
        PROGRAM_NOTIFICATIONS,
        TORRENT_ADDED_NOTIFICATIONS,
        CONFIRM_REMOVE_ALL_TAGS,
        DOWNLOAD_TRACKER_FAVICON,
        SAVE_PATH_HISTORY_LENGTH,
        ENABLE_SPEED_WIDGET,
        // embedded tracker
        TRACKER_STATUS,
        TRACKER_PORT,
        // libtorrent section
        LIBTORRENT_HEADER,
        ASYNC_IO_THREADS,
#if (LIBTORRENT_VERSION_NUM >= 20000)
        HASHING_THREADS,
#endif
        FILE_POOL_SIZE,
        CHECKING_MEM_USAGE,
#if (LIBTORRENT_VERSION_NUM < 20000)
        // cache
        DISK_CACHE,
        DISK_CACHE_TTL,
#endif
        OS_CACHE,
#if (LIBTORRENT_VERSION_NUM < 20000)
        COALESCE_RW,
#endif
        PIECE_EXTENT_AFFINITY,
        SUGGEST_MODE,
        SEND_BUF_WATERMARK,
        SEND_BUF_LOW_WATERMARK,
        SEND_BUF_WATERMARK_FACTOR,
        // networking & ports
        SOCKET_BACKLOG_SIZE,
        OUTGOING_PORT_MIN,
        OUTGOING_PORT_MAX,
        UPNP_LEASE_DURATION,
        UTP_MIX_MODE,
#ifdef HAS_IDN_SUPPORT
        IDN_SUPPORT,
#endif
        MULTI_CONNECTIONS_PER_IP,
#ifdef HAS_HTTPS_TRACKER_VALIDATION
        VALIDATE_HTTPS_TRACKER_CERTIFICATE,
#endif
        BLOCK_PEERS_ON_PRIVILEGED_PORTS,
        // seeding
        CHOKING_ALGORITHM,
        SEED_CHOKING_ALGORITHM,
        // tracker
        ANNOUNCE_ALL_TRACKERS,
        ANNOUNCE_ALL_TIERS,
        ANNOUNCE_IP,
        MAX_CONCURRENT_HTTP_ANNOUNCES,
        STOP_TRACKER_TIMEOUT,
        PEER_TURNOVER,
        PEER_TURNOVER_CUTOFF,
        PEER_TURNOVER_INTERVAL,

        ROW_COUNT
    };
}

AdvancedSettings::AdvancedSettings(QWidget *parent)
    : QTableWidget(parent)
{
    // column
    setColumnCount(COL_COUNT);
    QStringList header = {tr("Setting"), tr("Value", "Value set for this setting")};
    setHorizontalHeaderLabels(header);
    // row
    setRowCount(ROW_COUNT);
    verticalHeader()->setVisible(false);
    // etc.
    setAlternatingRowColors(true);
    setSelectionMode(QAbstractItemView::NoSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    // Load settings
    loadAdvancedSettings();
    resizeColumnToContents(0);
    horizontalHeader()->setStretchLastSection(true);
}

void AdvancedSettings::saveAdvancedSettings()
{
    Preferences *const pref = Preferences::instance();
    BitTorrent::Session *const session = BitTorrent::Session::instance();

#if defined(Q_OS_WIN)
    BitTorrent::OSMemoryPriority prio = BitTorrent::OSMemoryPriority::Normal;
    switch (m_comboBoxOSMemoryPriority.currentIndex())
    {
    case 0:
    default:
        prio = BitTorrent::OSMemoryPriority::Normal;
        break;
    case 1:
        prio = BitTorrent::OSMemoryPriority::BelowNormal;
        break;
    case 2:
        prio = BitTorrent::OSMemoryPriority::Medium;
        break;
    case 3:
        prio = BitTorrent::OSMemoryPriority::Low;
        break;
    case 4:
        prio = BitTorrent::OSMemoryPriority::VeryLow;
        break;
    }
    session->setOSMemoryPriority(prio);
#endif
    // Async IO threads
    session->setAsyncIOThreads(m_spinBoxAsyncIOThreads.value());
#if (LIBTORRENT_VERSION_NUM >= 20000)
    // Hashing threads
    session->setHashingThreads(m_spinBoxHashingThreads.value());
#endif
    // File pool size
    session->setFilePoolSize(m_spinBoxFilePoolSize.value());
    // Checking Memory Usage
    session->setCheckingMemUsage(m_spinBoxCheckingMemUsage.value());
#if (LIBTORRENT_VERSION_NUM < 20000)
    // Disk write cache
    session->setDiskCacheSize(m_spinBoxCache.value());
    session->setDiskCacheTTL(m_spinBoxCacheTTL.value());
#endif
    // Enable OS cache
    session->setUseOSCache(m_checkBoxOsCache.isChecked());
#if (LIBTORRENT_VERSION_NUM < 20000)
    // Coalesce reads & writes
    session->setCoalesceReadWriteEnabled(m_checkBoxCoalesceRW.isChecked());
#endif
    // Piece extent affinity
    session->setPieceExtentAffinity(m_checkBoxPieceExtentAffinity.isChecked());
    // Suggest mode
    session->setSuggestMode(m_checkBoxSuggestMode.isChecked());
    // Send buffer watermark
    session->setSendBufferWatermark(m_spinBoxSendBufferWatermark.value());
    session->setSendBufferLowWatermark(m_spinBoxSendBufferLowWatermark.value());
    session->setSendBufferWatermarkFactor(m_spinBoxSendBufferWatermarkFactor.value());
    // Socket listen backlog size
    session->setSocketBacklogSize(m_spinBoxSocketBacklogSize.value());
    // Save resume data interval
    session->setSaveResumeDataInterval(m_spinBoxSaveResumeDataInterval.value());
    // Outgoing ports
    session->setOutgoingPortsMin(m_spinBoxOutgoingPortsMin.value());
    session->setOutgoingPortsMax(m_spinBoxOutgoingPortsMax.value());
    // UPnP lease duration
    session->setUPnPLeaseDuration(m_spinBoxUPnPLeaseDuration.value());
    // uTP-TCP mixed mode
    session->setUtpMixedMode(static_cast<BitTorrent::MixedModeAlgorithm>(m_comboBoxUtpMixedMode.currentIndex()));
#ifdef HAS_IDN_SUPPORT
    // Support internationalized domain name (IDN)
    session->setIDNSupportEnabled(m_checkBoxIDNSupport.isChecked());
#endif
    // multiple connections per IP
    session->setMultiConnectionsPerIpEnabled(m_checkBoxMultiConnectionsPerIp.isChecked());
#ifdef HAS_HTTPS_TRACKER_VALIDATION
    // Validate HTTPS tracker certificate
    session->setValidateHTTPSTrackerCertificate(m_checkBoxValidateHTTPSTrackerCertificate.isChecked());
#endif
    // Disallow connection to peers on privileged ports
    session->setBlockPeersOnPrivilegedPorts(m_checkBoxBlockPeersOnPrivilegedPorts.isChecked());
    // Recheck torrents on completion
    pref->recheckTorrentsOnCompletion(m_checkBoxRecheckCompleted.isChecked());
    // Transfer list refresh interval
    session->setRefreshInterval(m_spinBoxListRefresh.value());
    // Peer resolution
    pref->resolvePeerCountries(m_checkBoxResolveCountries.isChecked());
    pref->resolvePeerHostNames(m_checkBoxResolveHosts.isChecked());
    // Network interface
    if (m_comboBoxInterface.currentIndex() == 0)
    {
        // All interfaces (default)
        session->setNetworkInterface(QString());
        session->setNetworkInterfaceName(QString());
    }
    else
    {
        session->setNetworkInterface(m_comboBoxInterface.itemData(m_comboBoxInterface.currentIndex()).toString());
        session->setNetworkInterfaceName(m_comboBoxInterface.currentText());
    }

    // Interface address
    // Construct a QHostAddress to filter malformed strings
    const QHostAddress ifaceAddr(m_comboBoxInterfaceAddress.currentData().toString().trimmed());
    session->setNetworkInterfaceAddress(ifaceAddr.toString());

    // Announce IP
    // Construct a QHostAddress to filter malformed strings
    const QHostAddress addr(m_lineEditAnnounceIP.text().trimmed());
    session->setAnnounceIP(addr.toString());
    // Max concurrent HTTP announces
    session->setMaxConcurrentHTTPAnnounces(m_spinBoxMaxConcurrentHTTPAnnounces.value());
    // Stop tracker timeout
    session->setStopTrackerTimeout(m_spinBoxStopTrackerTimeout.value());
    // Program notification
    MainWindow *const mainWindow = static_cast<Application*>(QCoreApplication::instance())->mainWindow();
    mainWindow->setNotificationsEnabled(m_checkBoxProgramNotifications.isChecked());
    mainWindow->setTorrentAddedNotificationsEnabled(m_checkBoxTorrentAddedNotifications.isChecked());
    // Misc GUI properties
    mainWindow->setDownloadTrackerFavicon(m_checkBoxTrackerFavicon.isChecked());
    AddNewTorrentDialog::setSavePathHistoryLength(m_spinBoxSavePathHistoryLength.value());
    pref->setSpeedWidgetEnabled(m_checkBoxSpeedWidgetEnabled.isChecked());

    // Tracker
    pref->setTrackerPort(m_spinBoxTrackerPort.value());
    session->setTrackerEnabled(m_checkBoxTrackerStatus.isChecked());
    // Choking algorithm
    session->setChokingAlgorithm(static_cast<BitTorrent::ChokingAlgorithm>(m_comboBoxChokingAlgorithm.currentIndex()));
    // Seed choking algorithm
    session->setSeedChokingAlgorithm(static_cast<BitTorrent::SeedChokingAlgorithm>(m_comboBoxSeedChokingAlgorithm.currentIndex()));

    pref->setConfirmTorrentRecheck(m_checkBoxConfirmTorrentRecheck.isChecked());

    pref->setConfirmRemoveAllTags(m_checkBoxConfirmRemoveAllTags.isChecked());

    session->setAnnounceToAllTrackers(m_checkBoxAnnounceAllTrackers.isChecked());
    session->setAnnounceToAllTiers(m_checkBoxAnnounceAllTiers.isChecked());

    session->setPeerTurnover(m_spinBoxPeerTurnover.value());
    session->setPeerTurnoverCutoff(m_spinBoxPeerTurnoverCutoff.value());
    session->setPeerTurnoverInterval(m_spinBoxPeerTurnoverInterval.value());
}

#if (LIBTORRENT_VERSION_NUM < 20000)
void AdvancedSettings::updateCacheSpinSuffix(int value)
{
    if (value == 0)
        m_spinBoxCache.setSuffix(tr(" (disabled)"));
    else if (value < 0)
        m_spinBoxCache.setSuffix(tr(" (auto)"));
    else
        m_spinBoxCache.setSuffix(tr(" MiB"));
}
#endif

void AdvancedSettings::updateSaveResumeDataIntervalSuffix(const int value)
{
    if (value > 0)
        m_spinBoxSaveResumeDataInterval.setSuffix(tr(" min", " minutes"));
    else
        m_spinBoxSaveResumeDataInterval.setSuffix(tr(" (disabled)"));
}

void AdvancedSettings::updateInterfaceAddressCombo()
{
    // Try to get the currently selected interface name
    const QString ifaceName = m_comboBoxInterface.itemData(m_comboBoxInterface.currentIndex()).toString(); // Empty string for the first element
    const QString currentAddress = BitTorrent::Session::instance()->networkInterfaceAddress();

    // Clear all items and reinsert them, default to all
    m_comboBoxInterfaceAddress.clear();
    m_comboBoxInterfaceAddress.addItem(tr("All addresses"), {});
    m_comboBoxInterfaceAddress.addItem(tr("All IPv4 addresses"), QLatin1String("0.0.0.0"));
    m_comboBoxInterfaceAddress.addItem(tr("All IPv6 addresses"), QLatin1String("::"));

    const auto populateCombo = [this](const QHostAddress &addr)
    {
        if (addr.protocol() == QAbstractSocket::IPv4Protocol)
        {
            const QString str = addr.toString();
            m_comboBoxInterfaceAddress.addItem(str, str);
        }
        else if (addr.protocol() == QAbstractSocket::IPv6Protocol)
        {
            const QString str = Utils::Net::canonicalIPv6Addr(addr).toString();
            m_comboBoxInterfaceAddress.addItem(str, str);
        }
    };

    if (ifaceName.isEmpty())
    {
        for (const QHostAddress &addr : asConst(QNetworkInterface::allAddresses()))
            populateCombo(addr);
    }
    else
    {
        const QNetworkInterface iface = QNetworkInterface::interfaceFromName(ifaceName);
        const QList<QNetworkAddressEntry> addresses = iface.addressEntries();
        for (const QNetworkAddressEntry &entry : addresses)
            populateCombo(entry.ip());
    }

    const int index = m_comboBoxInterfaceAddress.findData(currentAddress);
    m_comboBoxInterfaceAddress.setCurrentIndex(std::max(index, 0));
}

void AdvancedSettings::loadAdvancedSettings()
{
    const Preferences *const pref = Preferences::instance();
    const BitTorrent::Session *const session = BitTorrent::Session::instance();

    // add section headers
    auto *labelQbtLink = new QLabel(
        makeLink(QLatin1String("https://github.com/qbittorrent/qBittorrent/wiki/Explanation-of-Options-in-qBittorrent#Advanced")
                 , tr("Open documentation"))
        , this);
    labelQbtLink->setOpenExternalLinks(true);
    addRow(QBITTORRENT_HEADER, QString::fromLatin1("<b>%1</b>").arg(tr("qBittorrent Section")), labelQbtLink);
    static_cast<QLabel *>(cellWidget(QBITTORRENT_HEADER, PROPERTY))->setAlignment(Qt::AlignCenter | Qt::AlignVCenter);

    auto *labelLibtorrentLink = new QLabel(
        makeLink(QLatin1String("https://www.libtorrent.org/reference-Settings.html")
                 , tr("Open documentation"))
        , this);
    labelLibtorrentLink->setOpenExternalLinks(true);
    addRow(LIBTORRENT_HEADER, QString::fromLatin1("<b>%1</b>").arg(tr("libtorrent Section")), labelLibtorrentLink);
    static_cast<QLabel *>(cellWidget(LIBTORRENT_HEADER, PROPERTY))->setAlignment(Qt::AlignCenter | Qt::AlignVCenter);

#if defined(Q_OS_WIN)
    m_comboBoxOSMemoryPriority.addItems({tr("Normal"), tr("Below normal"), tr("Medium"), tr("Low"), tr("Very low")});
    int OSMemoryPriorityIndex = 0;
    switch (session->getOSMemoryPriority())
    {
    default:
    case BitTorrent::OSMemoryPriority::Normal:
        OSMemoryPriorityIndex = 0;
        break;
    case BitTorrent::OSMemoryPriority::BelowNormal:
        OSMemoryPriorityIndex = 1;
        break;
    case BitTorrent::OSMemoryPriority::Medium:
        OSMemoryPriorityIndex = 2;
        break;
    case BitTorrent::OSMemoryPriority::Low:
        OSMemoryPriorityIndex = 3;
        break;
    case BitTorrent::OSMemoryPriority::VeryLow:
        OSMemoryPriorityIndex = 4;
        break;
    }
    m_comboBoxOSMemoryPriority.setCurrentIndex(OSMemoryPriorityIndex);
    addRow(OS_MEMORY_PRIORITY, (tr("Process memory priority (Windows >= 8 only)")
        + ' ' + makeLink("https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/ns-processthreadsapi-memory_priority_information", "(?)"))
        , &m_comboBoxOSMemoryPriority);
#endif

    // Async IO threads
    m_spinBoxAsyncIOThreads.setMinimum(1);
    m_spinBoxAsyncIOThreads.setMaximum(1024);
    m_spinBoxAsyncIOThreads.setValue(session->asyncIOThreads());
    addRow(ASYNC_IO_THREADS, (tr("Asynchronous I/O threads") + ' ' + makeLink("https://www.libtorrent.org/reference-Settings.html#aio_threads", "(?)"))
            , &m_spinBoxAsyncIOThreads);

#if (LIBTORRENT_VERSION_NUM >= 20000)
    // Hashing threads
    m_spinBoxHashingThreads.setMinimum(1);
    m_spinBoxHashingThreads.setMaximum(1024);
    m_spinBoxHashingThreads.setValue(session->hashingThreads());
    addRow(HASHING_THREADS, (tr("Hashing threads") + ' ' + makeLink("https://www.libtorrent.org/reference-Settings.html#hashing_threads", "(?)"))
            , &m_spinBoxHashingThreads);
#endif

    // File pool size
    m_spinBoxFilePoolSize.setMinimum(1);
    m_spinBoxFilePoolSize.setMaximum(std::numeric_limits<int>::max());
    m_spinBoxFilePoolSize.setValue(session->filePoolSize());
    addRow(FILE_POOL_SIZE, (tr("File pool size") + ' ' + makeLink("https://www.libtorrent.org/reference-Settings.html#file_pool_size", "(?)"))
        , &m_spinBoxFilePoolSize);

    // Checking Memory Usage
    m_spinBoxCheckingMemUsage.setMinimum(1);
    // When build as 32bit binary, set the maximum value lower to prevent crashes.
#ifdef QBT_APP_64BIT
    m_spinBoxCheckingMemUsage.setMaximum(1024);
#else
    // Allocate at most 128MiB out of the remaining 512MiB (see the cache part below)
    m_spinBoxCheckingMemUsage.setMaximum(128);
#endif
    m_spinBoxCheckingMemUsage.setValue(session->checkingMemUsage());
    m_spinBoxCheckingMemUsage.setSuffix(tr(" MiB"));
    addRow(CHECKING_MEM_USAGE, (tr("Outstanding memory when checking torrents") + ' ' + makeLink("https://www.libtorrent.org/reference-Settings.html#checking_mem_usage", "(?)"))
            , &m_spinBoxCheckingMemUsage);
#if (LIBTORRENT_VERSION_NUM < 20000)
    // Disk write cache
    m_spinBoxCache.setMinimum(-1);
    // When build as 32bit binary, set the maximum at less than 2GB to prevent crashes.
#ifdef QBT_APP_64BIT
    m_spinBoxCache.setMaximum(33554431);  // 32768GiB
#else
    // allocate 1536MiB and leave 512MiB to the rest of program data in RAM
    m_spinBoxCache.setMaximum(1536);
#endif
    m_spinBoxCache.setValue(session->diskCacheSize());
    updateCacheSpinSuffix(m_spinBoxCache.value());
    connect(&m_spinBoxCache, qOverload<int>(&QSpinBox::valueChanged)
            , this, &AdvancedSettings::updateCacheSpinSuffix);
    addRow(DISK_CACHE, (tr("Disk cache") + ' ' + makeLink("https://www.libtorrent.org/reference-Settings.html#cache_size", "(?)"))
            , &m_spinBoxCache);
    // Disk cache expiry
    m_spinBoxCacheTTL.setMinimum(1);
    m_spinBoxCacheTTL.setMaximum(std::numeric_limits<int>::max());
    m_spinBoxCacheTTL.setValue(session->diskCacheTTL());
    m_spinBoxCacheTTL.setSuffix(tr(" s", " seconds"));
    addRow(DISK_CACHE_TTL, (tr("Disk cache expiry interval") + ' ' + makeLink("https://www.libtorrent.org/reference-Settings.html#cache_expiry", "(?)"))
            , &m_spinBoxCacheTTL);
#endif
    // Enable OS cache
    m_checkBoxOsCache.setChecked(session->useOSCache());
    addRow(OS_CACHE, (tr("Enable OS cache") + ' ' + makeLink("https://www.libtorrent.org/reference-Settings.html#disk_io_write_mode", "(?)"))
            , &m_checkBoxOsCache);
#if (LIBTORRENT_VERSION_NUM < 20000)
    // Coalesce reads & writes
    m_checkBoxCoalesceRW.setChecked(session->isCoalesceReadWriteEnabled());
    addRow(COALESCE_RW, (tr("Coalesce reads & writes") + ' ' + makeLink("https://www.libtorrent.org/reference-Settings.html#coalesce_reads", "(?)"))
            , &m_checkBoxCoalesceRW);
#endif
    // Piece extent affinity
    m_checkBoxPieceExtentAffinity.setChecked(session->usePieceExtentAffinity());
    addRow(PIECE_EXTENT_AFFINITY, (tr("Use piece extent affinity") + ' ' + makeLink("https://libtorrent.org/single-page-ref.html#piece_extent_affinity", "(?)")), &m_checkBoxPieceExtentAffinity);
    // Suggest mode
    m_checkBoxSuggestMode.setChecked(session->isSuggestModeEnabled());
    addRow(SUGGEST_MODE, (tr("Send upload piece suggestions") + ' ' + makeLink("https://www.libtorrent.org/reference-Settings.html#suggest_mode", "(?)"))
            , &m_checkBoxSuggestMode);
    // Send buffer watermark
    m_spinBoxSendBufferWatermark.setMinimum(1);
    m_spinBoxSendBufferWatermark.setMaximum(std::numeric_limits<int>::max());
    m_spinBoxSendBufferWatermark.setSuffix(tr(" KiB"));
    m_spinBoxSendBufferWatermark.setValue(session->sendBufferWatermark());
    addRow(SEND_BUF_WATERMARK, (tr("Send buffer watermark") + ' ' + makeLink("https://www.libtorrent.org/reference-Settings.html#send_buffer_watermark", "(?)"))
            , &m_spinBoxSendBufferWatermark);
    m_spinBoxSendBufferLowWatermark.setMinimum(1);
    m_spinBoxSendBufferLowWatermark.setMaximum(std::numeric_limits<int>::max());
    m_spinBoxSendBufferLowWatermark.setSuffix(tr(" KiB"));
    m_spinBoxSendBufferLowWatermark.setValue(session->sendBufferLowWatermark());
    addRow(SEND_BUF_LOW_WATERMARK, (tr("Send buffer low watermark") + ' ' + makeLink("https://www.libtorrent.org/reference-Settings.html#send_buffer_low_watermark", "(?)"))
            , &m_spinBoxSendBufferLowWatermark);
    m_spinBoxSendBufferWatermarkFactor.setMinimum(1);
    m_spinBoxSendBufferWatermarkFactor.setMaximum(std::numeric_limits<int>::max());
    m_spinBoxSendBufferWatermarkFactor.setSuffix(" %");
    m_spinBoxSendBufferWatermarkFactor.setValue(session->sendBufferWatermarkFactor());
    addRow(SEND_BUF_WATERMARK_FACTOR, (tr("Send buffer watermark factor") + ' ' + makeLink("https://www.libtorrent.org/reference-Settings.html#send_buffer_watermark_factor", "(?)"))
            , &m_spinBoxSendBufferWatermarkFactor);
    // Socket listen backlog size
    m_spinBoxSocketBacklogSize.setMinimum(1);
    m_spinBoxSocketBacklogSize.setMaximum(std::numeric_limits<int>::max());
    m_spinBoxSocketBacklogSize.setValue(session->socketBacklogSize());
    addRow(SOCKET_BACKLOG_SIZE, (tr("Socket backlog size") + ' ' + makeLink("https://www.libtorrent.org/reference-Settings.html#listen_queue_size", "(?)"))
            , &m_spinBoxSocketBacklogSize);
    // Save resume data interval
    m_spinBoxSaveResumeDataInterval.setMinimum(0);
    m_spinBoxSaveResumeDataInterval.setMaximum(std::numeric_limits<int>::max());
    m_spinBoxSaveResumeDataInterval.setValue(session->saveResumeDataInterval());
    connect(&m_spinBoxSaveResumeDataInterval, qOverload<int>(&QSpinBox::valueChanged)
        , this, &AdvancedSettings::updateSaveResumeDataIntervalSuffix);
    updateSaveResumeDataIntervalSuffix(m_spinBoxSaveResumeDataInterval.value());
    addRow(SAVE_RESUME_DATA_INTERVAL, tr("Save resume data interval", "How often the fastresume file is saved."), &m_spinBoxSaveResumeDataInterval);
    // Outgoing port Min
    m_spinBoxOutgoingPortsMin.setMinimum(0);
    m_spinBoxOutgoingPortsMin.setMaximum(65535);
    m_spinBoxOutgoingPortsMin.setValue(session->outgoingPortsMin());
    addRow(OUTGOING_PORT_MIN, (tr("Outgoing ports (Min) [0: Disabled]")
        + ' ' + makeLink("https://www.libtorrent.org/reference-Settings.html#outgoing_port", "(?)"))
        , &m_spinBoxOutgoingPortsMin);
    // Outgoing port Min
    m_spinBoxOutgoingPortsMax.setMinimum(0);
    m_spinBoxOutgoingPortsMax.setMaximum(65535);
    m_spinBoxOutgoingPortsMax.setValue(session->outgoingPortsMax());
    addRow(OUTGOING_PORT_MAX, (tr("Outgoing ports (Max) [0: Disabled]")
        + ' ' + makeLink("https://www.libtorrent.org/reference-Settings.html#outgoing_port", "(?)"))
        , &m_spinBoxOutgoingPortsMax);
    // UPnP lease duration
    m_spinBoxUPnPLeaseDuration.setMinimum(0);
    m_spinBoxUPnPLeaseDuration.setMaximum(std::numeric_limits<int>::max());
    m_spinBoxUPnPLeaseDuration.setValue(session->UPnPLeaseDuration());
    m_spinBoxUPnPLeaseDuration.setSuffix(tr(" s", " seconds"));
    addRow(UPNP_LEASE_DURATION, (tr("UPnP lease duration [0: Permanent lease]") + ' ' + makeLink("https://www.libtorrent.org/reference-Settings.html#upnp_lease_duration", "(?)"))
        , &m_spinBoxUPnPLeaseDuration);
    // uTP-TCP mixed mode
    m_comboBoxUtpMixedMode.addItems({tr("Prefer TCP"), tr("Peer proportional (throttles TCP)")});
    m_comboBoxUtpMixedMode.setCurrentIndex(static_cast<int>(session->utpMixedMode()));
    addRow(UTP_MIX_MODE, (tr("%1-TCP mixed mode algorithm", "uTP-TCP mixed mode algorithm").arg(C_UTP)
            + ' ' + makeLink("https://www.libtorrent.org/reference-Settings.html#mixed_mode_algorithm", "(?)"))
            , &m_comboBoxUtpMixedMode);
#ifdef HAS_IDN_SUPPORT
    // Support internationalized domain name (IDN)
    m_checkBoxIDNSupport.setChecked(session->isIDNSupportEnabled());
    addRow(IDN_SUPPORT, (tr("Support internationalized domain name (IDN)")
            + ' ' + makeLink("https://www.libtorrent.org/reference-Settings.html#allow_idna", "(?)"))
            , &m_checkBoxIDNSupport);
#endif
    // multiple connections per IP
    m_checkBoxMultiConnectionsPerIp.setChecked(session->multiConnectionsPerIpEnabled());
    addRow(MULTI_CONNECTIONS_PER_IP, (tr("Allow multiple connections from the same IP address")
            + ' ' + makeLink("https://www.libtorrent.org/reference-Settings.html#allow_multiple_connections_per_ip", "(?)"))
            , &m_checkBoxMultiConnectionsPerIp);
#ifdef HAS_HTTPS_TRACKER_VALIDATION
    // Validate HTTPS tracker certificate
    m_checkBoxValidateHTTPSTrackerCertificate.setChecked(session->validateHTTPSTrackerCertificate());
    addRow(VALIDATE_HTTPS_TRACKER_CERTIFICATE, (tr("Validate HTTPS tracker certificates")
            + ' ' + makeLink("https://www.libtorrent.org/reference-Settings.html#validate_https_trackers", "(?)"))
            , &m_checkBoxValidateHTTPSTrackerCertificate);
#endif
    // Disallow connection to peers on privileged ports
    m_checkBoxBlockPeersOnPrivilegedPorts.setChecked(session->blockPeersOnPrivilegedPorts());
    addRow(BLOCK_PEERS_ON_PRIVILEGED_PORTS, (tr("Disallow connection to peers on privileged ports") + ' ' + makeLink("https://libtorrent.org/single-page-ref.html#no_connect_privileged_ports", "(?)")), &m_checkBoxBlockPeersOnPrivilegedPorts);
    // Recheck completed torrents
    m_checkBoxRecheckCompleted.setChecked(pref->recheckTorrentsOnCompletion());
    addRow(RECHECK_COMPLETED, tr("Recheck torrents on completion"), &m_checkBoxRecheckCompleted);
    // Transfer list refresh interval
    m_spinBoxListRefresh.setMinimum(30);
    m_spinBoxListRefresh.setMaximum(99999);
    m_spinBoxListRefresh.setValue(session->refreshInterval());
    m_spinBoxListRefresh.setSuffix(tr(" ms", " milliseconds"));
    addRow(LIST_REFRESH, tr("Transfer list refresh interval"), &m_spinBoxListRefresh);
    // Resolve Peer countries
    m_checkBoxResolveCountries.setChecked(pref->resolvePeerCountries());
    addRow(RESOLVE_COUNTRIES, tr("Resolve peer countries"), &m_checkBoxResolveCountries);
    // Resolve peer hosts
    m_checkBoxResolveHosts.setChecked(pref->resolvePeerHostNames());
    addRow(RESOLVE_HOSTS, tr("Resolve peer host names"), &m_checkBoxResolveHosts);
    // Network interface
    m_comboBoxInterface.addItem(tr("Any interface", "i.e. Any network interface"));
    const QString currentInterface = session->networkInterface();
    bool interfaceExists = currentInterface.isEmpty();
    int i = 1;
    for (const QNetworkInterface &iface : asConst(QNetworkInterface::allInterfaces()))
    {
        m_comboBoxInterface.addItem(iface.humanReadableName(), iface.name());
        if (!currentInterface.isEmpty() && (iface.name() == currentInterface))
        {
            m_comboBoxInterface.setCurrentIndex(i);
            interfaceExists = true;
        }
        ++i;
    }
    // Saved interface does not exist, show it anyway
    if (!interfaceExists)
    {
        m_comboBoxInterface.addItem(session->networkInterfaceName(), currentInterface);
        m_comboBoxInterface.setCurrentIndex(i);
    }
    connect(&m_comboBoxInterface, qOverload<int>(&QComboBox::currentIndexChanged)
        , this, &AdvancedSettings::updateInterfaceAddressCombo);
    addRow(NETWORK_IFACE, tr("Network interface"), &m_comboBoxInterface);
    // Network interface address
    updateInterfaceAddressCombo();
    addRow(NETWORK_IFACE_ADDRESS, tr("Optional IP address to bind to"), &m_comboBoxInterfaceAddress);
    // Announce IP
    m_lineEditAnnounceIP.setText(session->announceIP());
    addRow(ANNOUNCE_IP, (tr("IP Address to report to trackers (requires restart)")
        + ' ' + makeLink("https://www.libtorrent.org/reference-Settings.html#announce_ip", "(?)"))
        , &m_lineEditAnnounceIP);
    // Max concurrent HTTP announces
    m_spinBoxMaxConcurrentHTTPAnnounces.setMaximum(std::numeric_limits<int>::max());
    m_spinBoxMaxConcurrentHTTPAnnounces.setValue(session->maxConcurrentHTTPAnnounces());
    addRow(MAX_CONCURRENT_HTTP_ANNOUNCES, (tr("Max concurrent HTTP announces") + ' ' + makeLink("https://www.libtorrent.org/reference-Settings.html#max_concurrent_http_announces", "(?)"))
           , &m_spinBoxMaxConcurrentHTTPAnnounces);
    // Stop tracker timeout
    m_spinBoxStopTrackerTimeout.setValue(session->stopTrackerTimeout());
    m_spinBoxStopTrackerTimeout.setSuffix(tr(" s", " seconds"));
    addRow(STOP_TRACKER_TIMEOUT, (tr("Stop tracker timeout") + ' ' + makeLink("https://www.libtorrent.org/reference-Settings.html#stop_tracker_timeout", "(?)"))
           , &m_spinBoxStopTrackerTimeout);

    // Program notifications
    const MainWindow *const mainWindow = static_cast<Application*>(QCoreApplication::instance())->mainWindow();
    m_checkBoxProgramNotifications.setChecked(mainWindow->isNotificationsEnabled());
    addRow(PROGRAM_NOTIFICATIONS, tr("Display notifications"), &m_checkBoxProgramNotifications);
    // Torrent added notifications
    m_checkBoxTorrentAddedNotifications.setChecked(mainWindow->isTorrentAddedNotificationsEnabled());
    addRow(TORRENT_ADDED_NOTIFICATIONS, tr("Display notifications for added torrents"), &m_checkBoxTorrentAddedNotifications);
    // Download tracker's favicon
    m_checkBoxTrackerFavicon.setChecked(mainWindow->isDownloadTrackerFavicon());
    addRow(DOWNLOAD_TRACKER_FAVICON, tr("Download tracker's favicon"), &m_checkBoxTrackerFavicon);
    // Save path history length
    m_spinBoxSavePathHistoryLength.setRange(AddNewTorrentDialog::minPathHistoryLength, AddNewTorrentDialog::maxPathHistoryLength);
    m_spinBoxSavePathHistoryLength.setValue(AddNewTorrentDialog::savePathHistoryLength());
    addRow(SAVE_PATH_HISTORY_LENGTH, tr("Save path history length"), &m_spinBoxSavePathHistoryLength);
    // Enable speed graphs
    m_checkBoxSpeedWidgetEnabled.setChecked(pref->isSpeedWidgetEnabled());
    addRow(ENABLE_SPEED_WIDGET, tr("Enable speed graphs"), &m_checkBoxSpeedWidgetEnabled);
    // Tracker State
    m_checkBoxTrackerStatus.setChecked(session->isTrackerEnabled());
    addRow(TRACKER_STATUS, tr("Enable embedded tracker"), &m_checkBoxTrackerStatus);
    // Tracker port
    m_spinBoxTrackerPort.setMinimum(1);
    m_spinBoxTrackerPort.setMaximum(65535);
    m_spinBoxTrackerPort.setValue(pref->getTrackerPort());
    addRow(TRACKER_PORT, tr("Embedded tracker port"), &m_spinBoxTrackerPort);
    // Choking algorithm
    m_comboBoxChokingAlgorithm.addItems({tr("Fixed slots"), tr("Upload rate based")});
    m_comboBoxChokingAlgorithm.setCurrentIndex(static_cast<int>(session->chokingAlgorithm()));
    addRow(CHOKING_ALGORITHM, (tr("Upload slots behavior") + ' ' + makeLink("https://www.libtorrent.org/reference-Settings.html#choking_algorithm", "(?)"))
            , &m_comboBoxChokingAlgorithm);
    // Seed choking algorithm
    m_comboBoxSeedChokingAlgorithm.addItems({tr("Round-robin"), tr("Fastest upload"), tr("Anti-leech")});
    m_comboBoxSeedChokingAlgorithm.setCurrentIndex(static_cast<int>(session->seedChokingAlgorithm()));
    addRow(SEED_CHOKING_ALGORITHM, (tr("Upload choking algorithm") + ' ' + makeLink("https://www.libtorrent.org/reference-Settings.html#seed_choking_algorithm", "(?)"))
            , &m_comboBoxSeedChokingAlgorithm);

    // Torrent recheck confirmation
    m_checkBoxConfirmTorrentRecheck.setChecked(pref->confirmTorrentRecheck());
    addRow(CONFIRM_RECHECK_TORRENT, tr("Confirm torrent recheck"), &m_checkBoxConfirmTorrentRecheck);

    // Remove all tags confirmation
    m_checkBoxConfirmRemoveAllTags.setChecked(pref->confirmRemoveAllTags());
    addRow(CONFIRM_REMOVE_ALL_TAGS, tr("Confirm removal of all tags"), &m_checkBoxConfirmRemoveAllTags);

    // Announce to all trackers in a tier
    m_checkBoxAnnounceAllTrackers.setChecked(session->announceToAllTrackers());
    addRow(ANNOUNCE_ALL_TRACKERS, (tr("Always announce to all trackers in a tier")
        + ' ' + makeLink("https://www.libtorrent.org/reference-Settings.html#announce_to_all_trackers", "(?)"))
        , &m_checkBoxAnnounceAllTrackers);

    // Announce to all tiers
    m_checkBoxAnnounceAllTiers.setChecked(session->announceToAllTiers());
    addRow(ANNOUNCE_ALL_TIERS, (tr("Always announce to all tiers")
        + ' ' + makeLink("https://www.libtorrent.org/reference-Settings.html#announce_to_all_tiers", "(?)"))
        , &m_checkBoxAnnounceAllTiers);

    m_spinBoxPeerTurnover.setMinimum(0);
    m_spinBoxPeerTurnover.setMaximum(100);
    m_spinBoxPeerTurnover.setValue(session->peerTurnover());
    m_spinBoxPeerTurnover.setSuffix(" %");
    addRow(PEER_TURNOVER, (tr("Peer turnover disconnect percentage") + ' ' + makeLink("https://www.libtorrent.org/reference-Settings.html#peer_turnover", "(?)"))
            , &m_spinBoxPeerTurnover);
    m_spinBoxPeerTurnoverCutoff.setMinimum(0);
    m_spinBoxPeerTurnoverCutoff.setMaximum(100);
    m_spinBoxPeerTurnoverCutoff.setSuffix(" %");
    m_spinBoxPeerTurnoverCutoff.setValue(session->peerTurnoverCutoff());
    addRow(PEER_TURNOVER_CUTOFF, (tr("Peer turnover threshold percentage") + ' ' + makeLink("https://www.libtorrent.org/reference-Settings.html#peer_turnover", "(?)"))
            , &m_spinBoxPeerTurnoverCutoff);
    m_spinBoxPeerTurnoverInterval.setMinimum(30);
    m_spinBoxPeerTurnoverInterval.setMaximum(3600);
    m_spinBoxPeerTurnoverInterval.setSuffix(tr(" s", " seconds"));
    m_spinBoxPeerTurnoverInterval.setValue(session->peerTurnoverInterval());
    addRow(PEER_TURNOVER_INTERVAL, (tr("Peer turnover disconnect interval") + ' ' + makeLink("https://www.libtorrent.org/reference-Settings.html#peer_turnover", "(?)"))
            , &m_spinBoxPeerTurnoverInterval);
}

template <typename T>
void AdvancedSettings::addRow(const int row, const QString &text, T *widget)
{
    auto label = new QLabel(text);
    label->setOpenExternalLinks(true);

    setCellWidget(row, PROPERTY, label);
    setCellWidget(row, VALUE, widget);

    if constexpr (std::is_same_v<T, QCheckBox>)
        connect(widget, &QCheckBox::stateChanged, this, &AdvancedSettings::settingsChanged);
    else if constexpr (std::is_same_v<T, QSpinBox>)
        connect(widget, qOverload<int>(&QSpinBox::valueChanged), this, &AdvancedSettings::settingsChanged);
    else if constexpr (std::is_same_v<T, QComboBox>)
        connect(widget, qOverload<int>(&QComboBox::currentIndexChanged), this, &AdvancedSettings::settingsChanged);
    else if constexpr (std::is_same_v<T, QLineEdit>)
        connect(widget, &QLineEdit::textChanged, this, &AdvancedSettings::settingsChanged);
}
