/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016-2024 qBittorrent project
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

#include <QtVersionChecks>
#include <QHeaderView>
#include <QHostAddress>
#include <QLabel>
#include <QNetworkInterface>

#include "base/bittorrent/session.h"
#include "base/global.h"
#include "base/preferences.h"
#include "base/unicodestrings.h"
#include "gui/addnewtorrentdialog.h"
#include "gui/desktopintegration.h"
#include "gui/mainwindow.h"
#include "interfaces/iguiapplication.h"

namespace
{
    QString makeLink(const QStringView url, const QStringView linkLabel)
    {
         return u"<a href=\"%1\">%2</a>"_s.arg(url, linkLabel);
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
        RESUME_DATA_STORAGE,
        TORRENT_CONTENT_REMOVE_OPTION,
#if defined(QBT_USES_LIBTORRENT2) && !defined(Q_OS_MACOS)
        MEMORY_WORKING_SET_LIMIT,
#endif
#if defined(Q_OS_WIN)
        OS_MEMORY_PRIORITY,
#endif
        // network interface
        NETWORK_IFACE,
        //Optional network address
        NETWORK_IFACE_ADDRESS,
        // behavior
        SAVE_RESUME_DATA_INTERVAL,
        SAVE_STATISTICS_INTERVAL,
        TORRENT_FILE_SIZE_LIMIT,
        CONFIRM_RECHECK_TORRENT,
        RECHECK_COMPLETED,
        // UI related
        APP_INSTANCE_NAME,
        LIST_REFRESH,
        RESOLVE_HOSTS,
        RESOLVE_COUNTRIES,
        PROGRAM_NOTIFICATIONS,
        TORRENT_ADDED_NOTIFICATIONS,
#ifdef QBT_USES_DBUS
        NOTIFICATION_TIMEOUT,
#endif
        CONFIRM_REMOVE_ALL_TAGS,
        CONFIRM_REMOVE_TRACKER_FROM_ALL_TORRENTS,
        REANNOUNCE_WHEN_ADDRESS_CHANGED,
        DOWNLOAD_TRACKER_FAVICON,
        SAVE_PATH_HISTORY_LENGTH,
        ENABLE_SPEED_WIDGET,
#ifndef Q_OS_MACOS
        ENABLE_ICONS_IN_MENUS,
#endif
        // embedded tracker
        TRACKER_STATUS,
        TRACKER_PORT,
        TRACKER_PORT_FORWARDING,
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
        ENABLE_MARK_OF_THE_WEB,
#endif // Q_OS_MACOS || Q_OS_WIN
        IGNORE_SSL_ERRORS,
        PYTHON_EXECUTABLE_PATH,
        START_SESSION_PAUSED,
        SESSION_SHUTDOWN_TIMEOUT,

        // libtorrent section
        LIBTORRENT_HEADER,
        BDECODE_DEPTH_LIMIT,
        BDECODE_TOKEN_LIMIT,
        ASYNC_IO_THREADS,
#ifdef QBT_USES_LIBTORRENT2
        HASHING_THREADS,
#endif
        FILE_POOL_SIZE,
        CHECKING_MEM_USAGE,
#ifndef QBT_USES_LIBTORRENT2
        // cache
        DISK_CACHE,
        DISK_CACHE_TTL,
#endif
        DISK_QUEUE_SIZE,
#ifdef QBT_USES_LIBTORRENT2
        DISK_IO_TYPE,
#endif
        DISK_IO_READ_MODE,
        DISK_IO_WRITE_MODE,
#ifndef QBT_USES_LIBTORRENT2
        COALESCE_RW,
#endif
        PIECE_EXTENT_AFFINITY,
        SUGGEST_MODE,
        SEND_BUF_WATERMARK,
        SEND_BUF_LOW_WATERMARK,
        SEND_BUF_WATERMARK_FACTOR,
        // networking & ports
        CONNECTION_SPEED,
        SOCKET_SEND_BUFFER_SIZE,
        SOCKET_RECEIVE_BUFFER_SIZE,
        SOCKET_BACKLOG_SIZE,
        OUTGOING_PORT_MIN,
        OUTGOING_PORT_MAX,
        UPNP_LEASE_DURATION,
        PEER_TOS,
        UTP_MIX_MODE,
        IDN_SUPPORT,
        MULTI_CONNECTIONS_PER_IP,
        VALIDATE_HTTPS_TRACKER_CERTIFICATE,
        SSRF_MITIGATION,
        BLOCK_PEERS_ON_PRIVILEGED_PORTS,
        // seeding
        CHOKING_ALGORITHM,
        SEED_CHOKING_ALGORITHM,
        // tracker
        ANNOUNCE_ALL_TRACKERS,
        ANNOUNCE_ALL_TIERS,
        ANNOUNCE_IP,
        ANNOUNCE_PORT,
        MAX_CONCURRENT_HTTP_ANNOUNCES,
        STOP_TRACKER_TIMEOUT,
        PEER_TURNOVER,
        PEER_TURNOVER_CUTOFF,
        PEER_TURNOVER_INTERVAL,
        REQUEST_QUEUE_SIZE,
        DHT_BOOTSTRAP_NODES,
#if defined(QBT_USES_LIBTORRENT2) && TORRENT_USE_I2P
        I2P_INBOUND_QUANTITY,
        I2P_OUTBOUND_QUANTITY,
        I2P_INBOUND_LENGTH,
        I2P_OUTBOUND_LENGTH,
#endif

        ROW_COUNT
    };
}

AdvancedSettings::AdvancedSettings(IGUIApplication *app, QWidget *parent)
    : GUIApplicationComponent(app, parent)
{
    // column
    setColumnCount(COL_COUNT);
    const QStringList header = {tr("Setting"), tr("Value", "Value set for this setting")};
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

void AdvancedSettings::saveAdvancedSettings() const
{
    Preferences *const pref = Preferences::instance();
    BitTorrent::Session *const session = BitTorrent::Session::instance();

    session->setResumeDataStorageType(m_comboBoxResumeDataStorage.currentData().value<BitTorrent::ResumeDataStorageType>());
#if defined(QBT_USES_LIBTORRENT2) && !defined(Q_OS_MACOS)
    // Physical memory (RAM) usage limit
    app()->setMemoryWorkingSetLimit(m_spinBoxMemoryWorkingSetLimit.value());
#endif
#if defined(Q_OS_WIN)
    app()->setProcessMemoryPriority(m_comboBoxOSMemoryPriority.currentData().value<MemoryPriority>());
#endif
    // Bdecode depth limit
    pref->setBdecodeDepthLimit(m_spinBoxBdecodeDepthLimit.value());
    // Bdecode token limit
    pref->setBdecodeTokenLimit(m_spinBoxBdecodeTokenLimit.value());
    // Async IO threads
    session->setAsyncIOThreads(m_spinBoxAsyncIOThreads.value());
#ifdef QBT_USES_LIBTORRENT2
    // Hashing threads
    session->setHashingThreads(m_spinBoxHashingThreads.value());
#endif
    // File pool size
    session->setFilePoolSize(m_spinBoxFilePoolSize.value());
    // Checking Memory Usage
    session->setCheckingMemUsage(m_spinBoxCheckingMemUsage.value());
#ifndef QBT_USES_LIBTORRENT2
    // Disk write cache
    session->setDiskCacheSize(m_spinBoxCache.value());
    session->setDiskCacheTTL(m_spinBoxCacheTTL.value());
#endif
    // Disk queue size
    session->setDiskQueueSize(m_spinBoxDiskQueueSize.value() * 1024);
#ifdef QBT_USES_LIBTORRENT2
    session->setDiskIOType(m_comboBoxDiskIOType.currentData().value<BitTorrent::DiskIOType>());
#endif
    // Disk IO read mode
    session->setDiskIOReadMode(m_comboBoxDiskIOReadMode.currentData().value<BitTorrent::DiskIOReadMode>());
    // Disk IO write mode
    session->setDiskIOWriteMode(m_comboBoxDiskIOWriteMode.currentData().value<BitTorrent::DiskIOWriteMode>());
#ifndef QBT_USES_LIBTORRENT2
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
    // Outgoing connections per second
    session->setConnectionSpeed(m_spinBoxConnectionSpeed.value());
    // Socket send buffer size
    session->setSocketSendBufferSize(m_spinBoxSocketSendBufferSize.value() * 1024);
    // Socket receive buffer size
    session->setSocketReceiveBufferSize(m_spinBoxSocketReceiveBufferSize.value() * 1024);
    // Socket listen backlog size
    session->setSocketBacklogSize(m_spinBoxSocketBacklogSize.value());
    // Save resume data interval
    session->setSaveResumeDataInterval(m_spinBoxSaveResumeDataInterval.value());
    // Save statistics interval
    session->setSaveStatisticsInterval(std::chrono::minutes(m_spinBoxSaveStatisticsInterval.value()));
    // .torrent file size limit
    pref->setTorrentFileSizeLimit(m_spinBoxTorrentFileSizeLimit.value() * 1024 * 1024);
    // Outgoing ports
    session->setOutgoingPortsMin(m_spinBoxOutgoingPortsMin.value());
    session->setOutgoingPortsMax(m_spinBoxOutgoingPortsMax.value());
    // UPnP lease duration
    session->setUPnPLeaseDuration(m_spinBoxUPnPLeaseDuration.value());
    // Type of service
    session->setPeerToS(m_spinBoxPeerToS.value());
    // uTP-TCP mixed mode
    session->setUtpMixedMode(m_comboBoxUtpMixedMode.currentData().value<BitTorrent::MixedModeAlgorithm>());
    // Support internationalized domain name (IDN)
    session->setIDNSupportEnabled(m_checkBoxIDNSupport.isChecked());
    // multiple connections per IP
    session->setMultiConnectionsPerIpEnabled(m_checkBoxMultiConnectionsPerIp.isChecked());
    // Validate HTTPS tracker certificate
    session->setValidateHTTPSTrackerCertificate(m_checkBoxValidateHTTPSTrackerCertificate.isChecked());
    // SSRF mitigation
    session->setSSRFMitigationEnabled(m_checkBoxSSRFMitigation.isChecked());
    // Disallow connection to peers on privileged ports
    session->setBlockPeersOnPrivilegedPorts(m_checkBoxBlockPeersOnPrivilegedPorts.isChecked());
    // Recheck torrents on completion
    pref->recheckTorrentsOnCompletion(m_checkBoxRecheckCompleted.isChecked());
    // Customize application instance name
    app()->setInstanceName(m_lineEditAppInstanceName.text());
    // Transfer list refresh interval
    session->setRefreshInterval(m_spinBoxListRefresh.value());
    // Peer resolution
    pref->resolvePeerCountries(m_checkBoxResolveCountries.isChecked());
    pref->resolvePeerHostNames(m_checkBoxResolveHosts.isChecked());
    // Network interface
    session->setNetworkInterface(m_comboBoxInterface.currentData().toString());
    session->setNetworkInterfaceName((m_comboBoxInterface.currentIndex() == 0)
        ? QString()
        : m_comboBoxInterface.currentText());
    // Interface address
    // Construct a QHostAddress to filter malformed strings
    const QHostAddress ifaceAddr {m_comboBoxInterfaceAddress.currentData().toString()};
    session->setNetworkInterfaceAddress(ifaceAddr.toString());
    // Announce IP
    // Construct a QHostAddress to filter malformed strings
    const QHostAddress addr(m_lineEditAnnounceIP.text().trimmed());
    session->setAnnounceIP(addr.toString());
    // Announce Port
    session->setAnnouncePort(m_spinBoxAnnouncePort.value());
    // Max concurrent HTTP announces
    session->setMaxConcurrentHTTPAnnounces(m_spinBoxMaxConcurrentHTTPAnnounces.value());
    // Stop tracker timeout
    session->setStopTrackerTimeout(m_spinBoxStopTrackerTimeout.value());
    // Program notification
    app()->desktopIntegration()->setNotificationsEnabled(m_checkBoxProgramNotifications.isChecked());
#ifdef QBT_USES_DBUS
    app()->desktopIntegration()->setNotificationTimeout(m_spinBoxNotificationTimeout.value());
#endif
    app()->setTorrentAddedNotificationsEnabled(m_checkBoxTorrentAddedNotifications.isChecked());
    // Reannounce to all trackers when ip/port changed
    session->setReannounceWhenAddressChangedEnabled(m_checkBoxReannounceWhenAddressChanged.isChecked());
    // Misc GUI properties
    app()->mainWindow()->setDownloadTrackerFavicon(m_checkBoxTrackerFavicon.isChecked());
    pref->setAddNewTorrentDialogSavePathHistoryLength(m_spinBoxSavePathHistoryLength.value());
    pref->setSpeedWidgetEnabled(m_checkBoxSpeedWidgetEnabled.isChecked());
#ifndef Q_OS_MACOS
    pref->setIconsInMenusEnabled(m_checkBoxIconsInMenusEnabled.isChecked());
#endif

    // Tracker
    pref->setTrackerPort(m_spinBoxTrackerPort.value());
    pref->setTrackerPortForwardingEnabled(m_checkBoxTrackerPortForwarding.isChecked());
    session->setTrackerEnabled(m_checkBoxTrackerStatus.isChecked());
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
    // Mark-of-the-Web
    pref->setMarkOfTheWebEnabled(m_checkBoxMarkOfTheWeb.isChecked());
#endif // Q_OS_MACOS || Q_OS_WIN
    // Ignore SSL errors
    pref->setIgnoreSSLErrors(m_checkBoxIgnoreSSLErrors.isChecked());
    // Python executable path
    pref->setPythonExecutablePath(Path(m_pythonExecutablePath.text().trimmed()));
    // Start session paused
    session->setStartPaused(m_checkBoxStartSessionPaused.isChecked());
    // Session shutdown timeout
    session->setShutdownTimeout(m_spinBoxSessionShutdownTimeout.value());
    // Choking algorithm
    session->setChokingAlgorithm(m_comboBoxChokingAlgorithm.currentData().value<BitTorrent::ChokingAlgorithm>());
    // Seed choking algorithm
    session->setSeedChokingAlgorithm(m_comboBoxSeedChokingAlgorithm.currentData().value<BitTorrent::SeedChokingAlgorithm>());

    pref->setConfirmTorrentRecheck(m_checkBoxConfirmTorrentRecheck.isChecked());

    pref->setConfirmRemoveAllTags(m_checkBoxConfirmRemoveAllTags.isChecked());
    pref->setConfirmRemoveTrackerFromAllTorrents(m_checkBoxConfirmRemoveTrackerFromAllTorrents.isChecked());

    session->setAnnounceToAllTrackers(m_checkBoxAnnounceAllTrackers.isChecked());
    session->setAnnounceToAllTiers(m_checkBoxAnnounceAllTiers.isChecked());

    session->setPeerTurnover(m_spinBoxPeerTurnover.value());
    session->setPeerTurnoverCutoff(m_spinBoxPeerTurnoverCutoff.value());
    session->setPeerTurnoverInterval(m_spinBoxPeerTurnoverInterval.value());
    // Maximum outstanding requests to a single peer
    session->setRequestQueueSize(m_spinBoxRequestQueueSize.value());
    // DHT bootstrap nodes
    session->setDHTBootstrapNodes(m_lineEditDHTBootstrapNodes.text());
#if defined(QBT_USES_LIBTORRENT2) && TORRENT_USE_I2P
    // I2P session options
    session->setI2PInboundQuantity(m_spinBoxI2PInboundQuantity.value());
    session->setI2POutboundQuantity(m_spinBoxI2POutboundQuantity.value());
    session->setI2PInboundLength(m_spinBoxI2PInboundLength.value());
    session->setI2POutboundLength(m_spinBoxI2POutboundLength.value());
#endif

    session->setTorrentContentRemoveOption(m_comboBoxTorrentContentRemoveOption.currentData().value<BitTorrent::TorrentContentRemoveOption>());
}

#ifndef QBT_USES_LIBTORRENT2
void AdvancedSettings::updateCacheSpinSuffix(const int value)
{
    if (value == 0)
        m_spinBoxCache.setSuffix(tr(" (disabled)"));
    else if (value < 0)
        m_spinBoxCache.setSuffix(tr(" (auto)"));
    else
        m_spinBoxCache.setSuffix(tr(" MiB"));
}
#endif

#ifdef QBT_USES_DBUS
void AdvancedSettings::updateNotificationTimeoutSuffix(const int value)
{
    if (value == 0)
        m_spinBoxNotificationTimeout.setSuffix(tr(" (infinite)"));
    else if (value < 0)
        m_spinBoxNotificationTimeout.setSuffix(tr(" (system default)"));
    else
        m_spinBoxNotificationTimeout.setSuffix(tr(" ms", " milliseconds"));
}
#endif

void AdvancedSettings::updateInterfaceAddressCombo()
{
    const auto toString = [](const QHostAddress &address) -> QString
    {
        switch (address.protocol()) {
        case QAbstractSocket::IPv4Protocol:
            return address.toString();
        case QAbstractSocket::IPv6Protocol:
            return Utils::Net::canonicalIPv6Addr(address).toString();
        default:
            Q_UNREACHABLE();
            break;
        }
        return {};
    };

    // Clear all items and reinsert them
    m_comboBoxInterfaceAddress.clear();
    m_comboBoxInterfaceAddress.addItem(tr("All addresses"), QString());
    m_comboBoxInterfaceAddress.addItem(tr("All IPv4 addresses"), QHostAddress(QHostAddress::AnyIPv4).toString());
    m_comboBoxInterfaceAddress.addItem(tr("All IPv6 addresses"), QHostAddress(QHostAddress::AnyIPv6).toString());

    const QString currentIface = m_comboBoxInterface.currentData().toString();
    if (currentIface.isEmpty())  // `any` interface
    {
        for (const QHostAddress &address : asConst(QNetworkInterface::allAddresses()))
        {
            const QString addressString = toString(address);
            m_comboBoxInterfaceAddress.addItem(addressString, addressString);
        }
    }
    else
    {
        const QList<QNetworkAddressEntry> addresses = QNetworkInterface::interfaceFromName(currentIface).addressEntries();
        for (const QNetworkAddressEntry &entry : addresses)
        {
            const QString addressString = toString(entry.ip());
            m_comboBoxInterfaceAddress.addItem(addressString, addressString);
        }
    }

    const QString currentAddress = BitTorrent::Session::instance()->networkInterfaceAddress();
    const int index = m_comboBoxInterfaceAddress.findData(currentAddress);
    if (index > -1)
    {
        m_comboBoxInterfaceAddress.setCurrentIndex(index);
    }
    else
    {
        // not found, for the sake of UI consistency, add such entry
        m_comboBoxInterfaceAddress.addItem(currentAddress, currentAddress);
        m_comboBoxInterfaceAddress.setCurrentIndex(m_comboBoxInterfaceAddress.count() - 1);
    }
}

void AdvancedSettings::loadAdvancedSettings()
{
    const Preferences *const pref = Preferences::instance();
    const BitTorrent::Session *const session = BitTorrent::Session::instance();

    // add section headers
    auto *labelQbtLink = new QLabel(
        makeLink(u"https://github.com/qbittorrent/qBittorrent/wiki/Explanation-of-Options-in-qBittorrent#Advanced"
                 , tr("Open documentation"))
        , this);
    labelQbtLink->setOpenExternalLinks(true);
    addRow(QBITTORRENT_HEADER, u"<b>%1</b>"_s.arg(tr("qBittorrent Section")), labelQbtLink);
    static_cast<QLabel *>(cellWidget(QBITTORRENT_HEADER, PROPERTY))->setAlignment(Qt::AlignCenter | Qt::AlignVCenter);

    auto *labelLibtorrentLink = new QLabel(
        makeLink(u"https://www.libtorrent.org/reference-Settings.html"
                 , tr("Open documentation"))
        , this);
    labelLibtorrentLink->setOpenExternalLinks(true);
    addRow(LIBTORRENT_HEADER, u"<b>%1</b>"_s.arg(tr("libtorrent Section")), labelLibtorrentLink);
    static_cast<QLabel *>(cellWidget(LIBTORRENT_HEADER, PROPERTY))->setAlignment(Qt::AlignCenter | Qt::AlignVCenter);

    m_comboBoxResumeDataStorage.addItem(tr("Fastresume files"), QVariant::fromValue(BitTorrent::ResumeDataStorageType::Legacy));
    m_comboBoxResumeDataStorage.addItem(tr("SQLite database (experimental)"), QVariant::fromValue(BitTorrent::ResumeDataStorageType::SQLite));
    m_comboBoxResumeDataStorage.setCurrentIndex(m_comboBoxResumeDataStorage.findData(QVariant::fromValue(session->resumeDataStorageType())));
    addRow(RESUME_DATA_STORAGE, tr("Resume data storage type (requires restart)"), &m_comboBoxResumeDataStorage);

    m_comboBoxTorrentContentRemoveOption.addItem(tr("Delete files permanently"), QVariant::fromValue(BitTorrent::TorrentContentRemoveOption::Delete));
    m_comboBoxTorrentContentRemoveOption.addItem(tr("Move files to trash (if possible)"), QVariant::fromValue(BitTorrent::TorrentContentRemoveOption::MoveToTrash));
    m_comboBoxTorrentContentRemoveOption.setCurrentIndex(m_comboBoxTorrentContentRemoveOption.findData(QVariant::fromValue(session->torrentContentRemoveOption())));
    addRow(TORRENT_CONTENT_REMOVE_OPTION, tr("Torrent content removing mode"), &m_comboBoxTorrentContentRemoveOption);

#if defined(QBT_USES_LIBTORRENT2) && !defined(Q_OS_MACOS)
    // Physical memory (RAM) usage limit
    m_spinBoxMemoryWorkingSetLimit.setMinimum(1);
    m_spinBoxMemoryWorkingSetLimit.setMaximum(std::numeric_limits<int>::max());
    m_spinBoxMemoryWorkingSetLimit.setSuffix(tr(" MiB"));
    m_spinBoxMemoryWorkingSetLimit.setToolTip(tr("This option is less effective on Linux"));
    m_spinBoxMemoryWorkingSetLimit.setValue(app()->memoryWorkingSetLimit());
    addRow(MEMORY_WORKING_SET_LIMIT, (tr("Physical memory (RAM) usage limit") + u' ' + makeLink(u"https://wikipedia.org/wiki/Working_set", u"(?)"))
        , &m_spinBoxMemoryWorkingSetLimit);
#endif
#if defined(Q_OS_WIN)
    m_comboBoxOSMemoryPriority.addItem(tr("Normal"), QVariant::fromValue(MemoryPriority::Normal));
    m_comboBoxOSMemoryPriority.addItem(tr("Below normal"), QVariant::fromValue(MemoryPriority::BelowNormal));
    m_comboBoxOSMemoryPriority.addItem(tr("Medium"), QVariant::fromValue(MemoryPriority::Medium));
    m_comboBoxOSMemoryPriority.addItem(tr("Low"), QVariant::fromValue(MemoryPriority::Low));
    m_comboBoxOSMemoryPriority.addItem(tr("Very low"), QVariant::fromValue(MemoryPriority::VeryLow));
    m_comboBoxOSMemoryPriority.setCurrentIndex(m_comboBoxOSMemoryPriority.findData(QVariant::fromValue(app()->processMemoryPriority())));
    addRow(OS_MEMORY_PRIORITY, (tr("Process memory priority")
        + u' ' + makeLink(u"https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/ns-processthreadsapi-memory_priority_information", u"(?)"))
        , &m_comboBoxOSMemoryPriority);
#endif
    // Bdecode depth limit
    m_spinBoxBdecodeDepthLimit.setMinimum(0);
    m_spinBoxBdecodeDepthLimit.setMaximum(std::numeric_limits<int>::max());
    m_spinBoxBdecodeDepthLimit.setValue(pref->getBdecodeDepthLimit());
    addRow(BDECODE_DEPTH_LIMIT, (tr("Bdecode depth limit") + u' ' + makeLink(u"https://www.libtorrent.org/reference-Bdecoding.html#bdecode()", u"(?)"))
            , &m_spinBoxBdecodeDepthLimit);
    // Bdecode token limit
    m_spinBoxBdecodeTokenLimit.setMinimum(0);
    m_spinBoxBdecodeTokenLimit.setMaximum(std::numeric_limits<int>::max());
    m_spinBoxBdecodeTokenLimit.setValue(pref->getBdecodeTokenLimit());
    addRow(BDECODE_TOKEN_LIMIT, (tr("Bdecode token limit") + u' ' + makeLink(u"https://www.libtorrent.org/reference-Bdecoding.html#bdecode()", u"(?)"))
            , &m_spinBoxBdecodeTokenLimit);
    // Async IO threads
    m_spinBoxAsyncIOThreads.setMinimum(1);
    m_spinBoxAsyncIOThreads.setMaximum(1024);
    m_spinBoxAsyncIOThreads.setValue(session->asyncIOThreads());
    addRow(ASYNC_IO_THREADS, (tr("Asynchronous I/O threads") + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#aio_threads", u"(?)"))
            , &m_spinBoxAsyncIOThreads);

#ifdef QBT_USES_LIBTORRENT2
    // Hashing threads
    m_spinBoxHashingThreads.setMinimum(1);
    m_spinBoxHashingThreads.setMaximum(1024);
    m_spinBoxHashingThreads.setValue(session->hashingThreads());
    addRow(HASHING_THREADS, (tr("Hashing threads") + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#hashing_threads", u"(?)"))
            , &m_spinBoxHashingThreads);
#endif

    // File pool size
    m_spinBoxFilePoolSize.setMinimum(1);
    m_spinBoxFilePoolSize.setMaximum(std::numeric_limits<int>::max());
    m_spinBoxFilePoolSize.setValue(session->filePoolSize());
    addRow(FILE_POOL_SIZE, (tr("File pool size") + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#file_pool_size", u"(?)"))
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
    addRow(CHECKING_MEM_USAGE, (tr("Outstanding memory when checking torrents") + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#checking_mem_usage", u"(?)"))
            , &m_spinBoxCheckingMemUsage);
#ifndef QBT_USES_LIBTORRENT2
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
    addRow(DISK_CACHE, (tr("Disk cache") + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#cache_size", u"(?)"))
            , &m_spinBoxCache);
    // Disk cache expiry
    m_spinBoxCacheTTL.setMinimum(1);
    m_spinBoxCacheTTL.setMaximum(std::numeric_limits<int>::max());
    m_spinBoxCacheTTL.setValue(session->diskCacheTTL());
    m_spinBoxCacheTTL.setSuffix(tr(" s", " seconds"));
    addRow(DISK_CACHE_TTL, (tr("Disk cache expiry interval") + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#cache_expiry", u"(?)"))
            , &m_spinBoxCacheTTL);
#endif
    // Disk queue size
    m_spinBoxDiskQueueSize.setMinimum(1);
    m_spinBoxDiskQueueSize.setMaximum(std::numeric_limits<int>::max() / 1024);
    m_spinBoxDiskQueueSize.setValue(session->diskQueueSize() / 1024);
    m_spinBoxDiskQueueSize.setSuffix(tr(" KiB"));
    addRow(DISK_QUEUE_SIZE, (tr("Disk queue size") + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#max_queued_disk_bytes", u"(?)"))
            , &m_spinBoxDiskQueueSize);
#ifdef QBT_USES_LIBTORRENT2
    // Disk IO type
    m_comboBoxDiskIOType.addItem(tr("Default"), QVariant::fromValue(BitTorrent::DiskIOType::Default));
    m_comboBoxDiskIOType.addItem(tr("Memory mapped files"), QVariant::fromValue(BitTorrent::DiskIOType::MMap));
    m_comboBoxDiskIOType.addItem(tr("POSIX-compliant"), QVariant::fromValue(BitTorrent::DiskIOType::Posix));
    m_comboBoxDiskIOType.addItem(tr("Simple pread/pwrite"), QVariant::fromValue(BitTorrent::DiskIOType::SimplePreadPwrite));
    m_comboBoxDiskIOType.setCurrentIndex(m_comboBoxDiskIOType.findData(QVariant::fromValue(session->diskIOType())));
    addRow(DISK_IO_TYPE, tr("Disk IO type (requires restart)") + u' ' + makeLink(u"https://www.libtorrent.org/single-page-ref.html#default-disk-io-constructor", u"(?)")
           , &m_comboBoxDiskIOType);
#endif
    // Disk IO read mode
    m_comboBoxDiskIOReadMode.addItem(tr("Disable OS cache"), QVariant::fromValue(BitTorrent::DiskIOReadMode::DisableOSCache));
    m_comboBoxDiskIOReadMode.addItem(tr("Enable OS cache"), QVariant::fromValue(BitTorrent::DiskIOReadMode::EnableOSCache));
    m_comboBoxDiskIOReadMode.setCurrentIndex(m_comboBoxDiskIOReadMode.findData(QVariant::fromValue(session->diskIOReadMode())));
    addRow(DISK_IO_READ_MODE, (tr("Disk IO read mode") + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#disk_io_read_mode", u"(?)"))
            , &m_comboBoxDiskIOReadMode);
    // Disk IO write mode
    m_comboBoxDiskIOWriteMode.addItem(tr("Disable OS cache"), QVariant::fromValue(BitTorrent::DiskIOWriteMode::DisableOSCache));
    m_comboBoxDiskIOWriteMode.addItem(tr("Enable OS cache"), QVariant::fromValue(BitTorrent::DiskIOWriteMode::EnableOSCache));
#ifdef QBT_USES_LIBTORRENT2
    m_comboBoxDiskIOWriteMode.addItem(tr("Write-through"), QVariant::fromValue(BitTorrent::DiskIOWriteMode::WriteThrough));
#endif
    m_comboBoxDiskIOWriteMode.setCurrentIndex(m_comboBoxDiskIOWriteMode.findData(QVariant::fromValue(session->diskIOWriteMode())));
    addRow(DISK_IO_WRITE_MODE, (tr("Disk IO write mode") + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#disk_io_write_mode", u"(?)"))
            , &m_comboBoxDiskIOWriteMode);
#ifndef QBT_USES_LIBTORRENT2
    // Coalesce reads & writes
    m_checkBoxCoalesceRW.setChecked(session->isCoalesceReadWriteEnabled());
    addRow(COALESCE_RW, (tr("Coalesce reads & writes") + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#coalesce_reads", u"(?)"))
            , &m_checkBoxCoalesceRW);
#endif
    // Piece extent affinity
    m_checkBoxPieceExtentAffinity.setChecked(session->usePieceExtentAffinity());
    addRow(PIECE_EXTENT_AFFINITY, (tr("Use piece extent affinity") + u' ' + makeLink(u"https://libtorrent.org/single-page-ref.html#piece_extent_affinity", u"(?)")), &m_checkBoxPieceExtentAffinity);
    // Suggest mode
    m_checkBoxSuggestMode.setChecked(session->isSuggestModeEnabled());
    addRow(SUGGEST_MODE, (tr("Send upload piece suggestions") + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#suggest_mode", u"(?)"))
            , &m_checkBoxSuggestMode);
    // Send buffer watermark
    m_spinBoxSendBufferWatermark.setMinimum(1);
    m_spinBoxSendBufferWatermark.setMaximum(std::numeric_limits<int>::max());
    m_spinBoxSendBufferWatermark.setSuffix(tr(" KiB"));
    m_spinBoxSendBufferWatermark.setValue(session->sendBufferWatermark());
    addRow(SEND_BUF_WATERMARK, (tr("Send buffer watermark") + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#send_buffer_watermark", u"(?)"))
            , &m_spinBoxSendBufferWatermark);
    m_spinBoxSendBufferLowWatermark.setMinimum(1);
    m_spinBoxSendBufferLowWatermark.setMaximum(std::numeric_limits<int>::max());
    m_spinBoxSendBufferLowWatermark.setSuffix(tr(" KiB"));
    m_spinBoxSendBufferLowWatermark.setValue(session->sendBufferLowWatermark());
    addRow(SEND_BUF_LOW_WATERMARK, (tr("Send buffer low watermark") + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#send_buffer_low_watermark", u"(?)"))
            , &m_spinBoxSendBufferLowWatermark);
    m_spinBoxSendBufferWatermarkFactor.setMinimum(1);
    m_spinBoxSendBufferWatermarkFactor.setMaximum(std::numeric_limits<int>::max());
    m_spinBoxSendBufferWatermarkFactor.setSuffix(u" %"_s);
    m_spinBoxSendBufferWatermarkFactor.setValue(session->sendBufferWatermarkFactor());
    addRow(SEND_BUF_WATERMARK_FACTOR, (tr("Send buffer watermark factor") + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#send_buffer_watermark_factor", u"(?)"))
            , &m_spinBoxSendBufferWatermarkFactor);
    // Outgoing connections per second
    m_spinBoxConnectionSpeed.setMinimum(0);
    m_spinBoxConnectionSpeed.setMaximum(std::numeric_limits<int>::max());
    m_spinBoxConnectionSpeed.setValue(session->connectionSpeed());
    addRow(CONNECTION_SPEED, (tr("Outgoing connections per second") + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#connection_speed", u"(?)"))
            , &m_spinBoxConnectionSpeed);
    // Socket send buffer size
    m_spinBoxSocketSendBufferSize.setMinimum(0);
    m_spinBoxSocketSendBufferSize.setMaximum(std::numeric_limits<int>::max() / 1024);
    m_spinBoxSocketSendBufferSize.setValue(session->socketSendBufferSize() / 1024);
    m_spinBoxSocketSendBufferSize.setSuffix(tr(" KiB"));
    m_spinBoxSocketSendBufferSize.setSpecialValueText(tr("0 (system default)"));
    addRow(SOCKET_SEND_BUFFER_SIZE, (tr("Socket send buffer size [0: system default]") + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#send_socket_buffer_size", u"(?)"))
            , &m_spinBoxSocketSendBufferSize);
    // Socket receive buffer size
    m_spinBoxSocketReceiveBufferSize.setMinimum(0);
    m_spinBoxSocketReceiveBufferSize.setMaximum(std::numeric_limits<int>::max() / 1024);
    m_spinBoxSocketReceiveBufferSize.setValue(session->socketReceiveBufferSize() / 1024);
    m_spinBoxSocketReceiveBufferSize.setSuffix(tr(" KiB"));
    m_spinBoxSocketReceiveBufferSize.setSpecialValueText(tr("0 (system default)"));
    addRow(SOCKET_RECEIVE_BUFFER_SIZE, (tr("Socket receive buffer size [0: system default]") + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#recv_socket_buffer_size", u"(?)"))
            , &m_spinBoxSocketReceiveBufferSize);
    // Socket listen backlog size
    m_spinBoxSocketBacklogSize.setMinimum(1);
    m_spinBoxSocketBacklogSize.setMaximum(std::numeric_limits<int>::max());
    m_spinBoxSocketBacklogSize.setValue(session->socketBacklogSize());
    addRow(SOCKET_BACKLOG_SIZE, (tr("Socket backlog size") + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#listen_queue_size", u"(?)"))
            , &m_spinBoxSocketBacklogSize);
    // Save resume data interval
    m_spinBoxSaveResumeDataInterval.setMinimum(0);
    m_spinBoxSaveResumeDataInterval.setMaximum(std::numeric_limits<int>::max());
    m_spinBoxSaveResumeDataInterval.setValue(session->saveResumeDataInterval());
    m_spinBoxSaveResumeDataInterval.setSuffix(tr(" min", " minutes"));
    m_spinBoxSaveResumeDataInterval.setSpecialValueText(tr("0 (disabled)"));
    addRow(SAVE_RESUME_DATA_INTERVAL, tr("Save resume data interval [0: disabled]", "How often the fastresume file is saved."), &m_spinBoxSaveResumeDataInterval);
    // Save statistics interval
    m_spinBoxSaveStatisticsInterval.setMinimum(0);
    m_spinBoxSaveStatisticsInterval.setMaximum(std::numeric_limits<int>::max());
    m_spinBoxSaveStatisticsInterval.setValue(session->saveStatisticsInterval().count());
    m_spinBoxSaveStatisticsInterval.setSuffix(tr(" min", " minutes"));
    m_spinBoxSaveStatisticsInterval.setSpecialValueText(tr("0 (disabled)"));
    addRow(SAVE_STATISTICS_INTERVAL, tr("Save statistics interval [0: disabled]", "How often the statistics file is saved."), &m_spinBoxSaveStatisticsInterval);
    // .torrent file size limit
    m_spinBoxTorrentFileSizeLimit.setMinimum(1);
    m_spinBoxTorrentFileSizeLimit.setMaximum(std::numeric_limits<int>::max() / 1024 / 1024);
    m_spinBoxTorrentFileSizeLimit.setValue(pref->getTorrentFileSizeLimit() / 1024 / 1024);
    m_spinBoxTorrentFileSizeLimit.setSuffix(tr(" MiB"));
    addRow(TORRENT_FILE_SIZE_LIMIT, tr(".torrent file size limit"), &m_spinBoxTorrentFileSizeLimit);
    // Outgoing port Min
    m_spinBoxOutgoingPortsMin.setMinimum(0);
    m_spinBoxOutgoingPortsMin.setMaximum(65535);
    m_spinBoxOutgoingPortsMin.setValue(session->outgoingPortsMin());
    m_spinBoxOutgoingPortsMin.setSpecialValueText(tr("0 (disabled)"));
    addRow(OUTGOING_PORT_MIN, (tr("Outgoing ports (Min) [0: disabled]")
        + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#outgoing_port", u"(?)"))
        , &m_spinBoxOutgoingPortsMin);
    // Outgoing port Max
    m_spinBoxOutgoingPortsMax.setMinimum(0);
    m_spinBoxOutgoingPortsMax.setMaximum(65535);
    m_spinBoxOutgoingPortsMax.setValue(session->outgoingPortsMax());
    m_spinBoxOutgoingPortsMax.setSpecialValueText(tr("0 (disabled)"));
    addRow(OUTGOING_PORT_MAX, (tr("Outgoing ports (Max) [0: disabled]")
        + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#outgoing_port", u"(?)"))
        , &m_spinBoxOutgoingPortsMax);
    // UPnP lease duration
    m_spinBoxUPnPLeaseDuration.setMinimum(0);
    m_spinBoxUPnPLeaseDuration.setMaximum(std::numeric_limits<int>::max());
    m_spinBoxUPnPLeaseDuration.setValue(session->UPnPLeaseDuration());
    m_spinBoxUPnPLeaseDuration.setSuffix(tr(" s", " seconds"));
    m_spinBoxUPnPLeaseDuration.setSpecialValueText(tr("0 (permanent lease)"));
    addRow(UPNP_LEASE_DURATION, (tr("UPnP lease duration [0: permanent lease]") + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#upnp_lease_duration", u"(?)"))
        , &m_spinBoxUPnPLeaseDuration);
    // Type of service
    m_spinBoxPeerToS.setMinimum(0);
    m_spinBoxPeerToS.setMaximum(255);
    m_spinBoxPeerToS.setValue(session->peerToS());
    addRow(PEER_TOS, (tr("Type of service (ToS) for connections to peers") + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#peer_tos", u"(?)"))
        , &m_spinBoxPeerToS);
    // uTP-TCP mixed mode
    m_comboBoxUtpMixedMode.addItem(tr("Prefer TCP"), QVariant::fromValue(BitTorrent::MixedModeAlgorithm::TCP));
    m_comboBoxUtpMixedMode.addItem(tr("Peer proportional (throttles TCP)"), QVariant::fromValue(BitTorrent::MixedModeAlgorithm::Proportional));
    m_comboBoxUtpMixedMode.setCurrentIndex(m_comboBoxUtpMixedMode.findData(QVariant::fromValue(session->utpMixedMode())));
    addRow(UTP_MIX_MODE, (tr("%1-TCP mixed mode algorithm", "uTP-TCP mixed mode algorithm").arg(C_UTP)
            + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#mixed_mode_algorithm", u"(?)"))
            , &m_comboBoxUtpMixedMode);
    // Support internationalized domain name (IDN)
    m_checkBoxIDNSupport.setChecked(session->isIDNSupportEnabled());
    addRow(IDN_SUPPORT, (tr("Support internationalized domain name (IDN)")
            + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#allow_idna", u"(?)"))
            , &m_checkBoxIDNSupport);
    // multiple connections per IP
    m_checkBoxMultiConnectionsPerIp.setChecked(session->multiConnectionsPerIpEnabled());
    addRow(MULTI_CONNECTIONS_PER_IP, (tr("Allow multiple connections from the same IP address")
            + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#allow_multiple_connections_per_ip", u"(?)"))
            , &m_checkBoxMultiConnectionsPerIp);
    // Validate HTTPS tracker certificate
    m_checkBoxValidateHTTPSTrackerCertificate.setChecked(session->validateHTTPSTrackerCertificate());
    addRow(VALIDATE_HTTPS_TRACKER_CERTIFICATE, (tr("Validate HTTPS tracker certificates")
            + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#validate_https_trackers", u"(?)"))
            , &m_checkBoxValidateHTTPSTrackerCertificate);
    // SSRF mitigation
    m_checkBoxSSRFMitigation.setChecked(session->isSSRFMitigationEnabled());
    addRow(SSRF_MITIGATION, (tr("Server-side request forgery (SSRF) mitigation")
        + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#ssrf_mitigation", u"(?)"))
        , &m_checkBoxSSRFMitigation);
    // Disallow connection to peers on privileged ports
    m_checkBoxBlockPeersOnPrivilegedPorts.setChecked(session->blockPeersOnPrivilegedPorts());
    addRow(BLOCK_PEERS_ON_PRIVILEGED_PORTS, (tr("Disallow connection to peers on privileged ports") + u' ' + makeLink(u"https://libtorrent.org/single-page-ref.html#no_connect_privileged_ports", u"(?)")), &m_checkBoxBlockPeersOnPrivilegedPorts);
    // Recheck completed torrents
    m_checkBoxRecheckCompleted.setChecked(pref->recheckTorrentsOnCompletion());
    addRow(RECHECK_COMPLETED, tr("Recheck torrents on completion"), &m_checkBoxRecheckCompleted);
    // Customize application instance name
    m_lineEditAppInstanceName.setText(app()->instanceName());
    m_lineEditAppInstanceName.setToolTip(tr("It appends the text to the window title to help distinguish qBittorent instances"));
    addRow(APP_INSTANCE_NAME, tr("Customize application instance name"), &m_lineEditAppInstanceName);
    // Refresh interval
    m_spinBoxListRefresh.setMinimum(30);
    m_spinBoxListRefresh.setMaximum(99999);
    m_spinBoxListRefresh.setValue(session->refreshInterval());
    m_spinBoxListRefresh.setSuffix(tr(" ms", " milliseconds"));
    m_spinBoxListRefresh.setToolTip(tr("It controls the internal state update interval which in turn will affect UI updates"));
    addRow(LIST_REFRESH, tr("Refresh interval"), &m_spinBoxListRefresh);
    // Resolve Peer countries
    m_checkBoxResolveCountries.setChecked(pref->resolvePeerCountries());
    addRow(RESOLVE_COUNTRIES, tr("Resolve peer countries"), &m_checkBoxResolveCountries);
    // Resolve peer hosts
    m_checkBoxResolveHosts.setChecked(pref->resolvePeerHostNames());
    addRow(RESOLVE_HOSTS, tr("Resolve peer host names"), &m_checkBoxResolveHosts);
    // Network interface
    m_comboBoxInterface.addItem(tr("Any interface", "i.e. Any network interface"), QString());
    for (const QNetworkInterface &iface : asConst(QNetworkInterface::allInterfaces()))
        m_comboBoxInterface.addItem(iface.humanReadableName(), iface.name());

    const QString currentInterface = session->networkInterface();
    const int ifaceIndex = m_comboBoxInterface.findData(currentInterface);
    if (ifaceIndex > -1)
    {
        m_comboBoxInterface.setCurrentIndex(ifaceIndex);
    }
    else
    {
        // Saved interface does not exist, show it
        m_comboBoxInterface.addItem(session->networkInterfaceName(), currentInterface);
        m_comboBoxInterface.setCurrentIndex(m_comboBoxInterface.count() - 1);
    }

    connect(&m_comboBoxInterface, qOverload<int>(&QComboBox::currentIndexChanged)
        , this, &AdvancedSettings::updateInterfaceAddressCombo);
    addRow(NETWORK_IFACE, tr("Network interface"), &m_comboBoxInterface);
    // Network interface address
    updateInterfaceAddressCombo();
    addRow(NETWORK_IFACE_ADDRESS, tr("Optional IP address to bind to"), &m_comboBoxInterfaceAddress);
    // Announce IP
    m_lineEditAnnounceIP.setText(session->announceIP());
    addRow(ANNOUNCE_IP, (tr("IP address reported to trackers (requires restart)")
        + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#announce_ip", u"(?)"))
        , &m_lineEditAnnounceIP);
    // Announce port
    m_spinBoxAnnouncePort.setMinimum(0);
    m_spinBoxAnnouncePort.setMaximum(65535);
    m_spinBoxAnnouncePort.setValue(session->announcePort());
    addRow(ANNOUNCE_PORT, (tr("Port reported to trackers (requires restart) [0: listening port]")
        + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#announce_port", u"(?)"))
        , &m_spinBoxAnnouncePort);
    // Max concurrent HTTP announces
    m_spinBoxMaxConcurrentHTTPAnnounces.setMaximum(std::numeric_limits<int>::max());
    m_spinBoxMaxConcurrentHTTPAnnounces.setValue(session->maxConcurrentHTTPAnnounces());
    addRow(MAX_CONCURRENT_HTTP_ANNOUNCES, (tr("Max concurrent HTTP announces") + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#max_concurrent_http_announces", u"(?)"))
           , &m_spinBoxMaxConcurrentHTTPAnnounces);
    // Stop tracker timeout
    m_spinBoxStopTrackerTimeout.setMaximum(std::numeric_limits<int>::max());
    m_spinBoxStopTrackerTimeout.setValue(session->stopTrackerTimeout());
    m_spinBoxStopTrackerTimeout.setSuffix(tr(" s", " seconds"));
    m_spinBoxStopTrackerTimeout.setSpecialValueText(tr("0 (disabled)"));
    addRow(STOP_TRACKER_TIMEOUT, (tr("Stop tracker timeout [0: disabled]") + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#stop_tracker_timeout", u"(?)"))
           , &m_spinBoxStopTrackerTimeout);
    // Program notifications
    m_checkBoxProgramNotifications.setChecked(app()->desktopIntegration()->isNotificationsEnabled());
    addRow(PROGRAM_NOTIFICATIONS, tr("Display notifications"), &m_checkBoxProgramNotifications);
    // Torrent added notifications
    m_checkBoxTorrentAddedNotifications.setChecked(app()->isTorrentAddedNotificationsEnabled());
    addRow(TORRENT_ADDED_NOTIFICATIONS, tr("Display notifications for added torrents"), &m_checkBoxTorrentAddedNotifications);
#ifdef QBT_USES_DBUS
    // Notification timeout
    m_spinBoxNotificationTimeout.setMinimum(-1);
    m_spinBoxNotificationTimeout.setMaximum(std::numeric_limits<int>::max());
    m_spinBoxNotificationTimeout.setValue(app()->desktopIntegration()->notificationTimeout());
    connect(&m_spinBoxNotificationTimeout, qOverload<int>(&QSpinBox::valueChanged)
        , this, &AdvancedSettings::updateNotificationTimeoutSuffix);
    updateNotificationTimeoutSuffix(m_spinBoxNotificationTimeout.value());
    addRow(NOTIFICATION_TIMEOUT, tr("Notification timeout [0: infinite, -1: system default]"), &m_spinBoxNotificationTimeout);
#endif
    // Reannounce to all trackers when ip/port changed
    m_checkBoxReannounceWhenAddressChanged.setChecked(session->isReannounceWhenAddressChangedEnabled());
    addRow(REANNOUNCE_WHEN_ADDRESS_CHANGED, tr("Reannounce to all trackers when IP or port changed"), &m_checkBoxReannounceWhenAddressChanged);
    // Download tracker's favicon
    m_checkBoxTrackerFavicon.setChecked(app()->mainWindow()->isDownloadTrackerFavicon());
    addRow(DOWNLOAD_TRACKER_FAVICON, tr("Download tracker's favicon"), &m_checkBoxTrackerFavicon);
    // Save path history length
    m_spinBoxSavePathHistoryLength.setRange(0, 99);
    m_spinBoxSavePathHistoryLength.setValue(pref->addNewTorrentDialogSavePathHistoryLength());
    addRow(SAVE_PATH_HISTORY_LENGTH, tr("Save path history length"), &m_spinBoxSavePathHistoryLength);
    // Enable speed graphs
    m_checkBoxSpeedWidgetEnabled.setChecked(pref->isSpeedWidgetEnabled());
    addRow(ENABLE_SPEED_WIDGET, tr("Enable speed graphs"), &m_checkBoxSpeedWidgetEnabled);
#ifndef Q_OS_MACOS
    // Enable icons in menus
    m_checkBoxIconsInMenusEnabled.setChecked(pref->iconsInMenusEnabled());
    addRow(ENABLE_ICONS_IN_MENUS, tr("Enable icons in menus"), &m_checkBoxIconsInMenusEnabled);
#endif
    // Tracker State
    m_checkBoxTrackerStatus.setChecked(session->isTrackerEnabled());
    addRow(TRACKER_STATUS, tr("Enable embedded tracker"), &m_checkBoxTrackerStatus);
    // Tracker port
    m_spinBoxTrackerPort.setMinimum(1);
    m_spinBoxTrackerPort.setMaximum(65535);
    m_spinBoxTrackerPort.setValue(pref->getTrackerPort());
    addRow(TRACKER_PORT, tr("Embedded tracker port"), &m_spinBoxTrackerPort);
    // Tracker port forwarding
    m_checkBoxTrackerPortForwarding.setChecked(pref->isTrackerPortForwardingEnabled());
    addRow(TRACKER_PORT_FORWARDING, tr("Enable port forwarding for embedded tracker"), &m_checkBoxTrackerPortForwarding);
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
    // Mark-of-the-Web
#ifdef Q_OS_MACOS
    const QString motwLabel = tr("Enable quarantine for downloaded files");
#elif defined(Q_OS_WIN)
    const QString motwLabel = tr("Enable Mark-of-the-Web (MOTW) for downloaded files");
#endif
    m_checkBoxMarkOfTheWeb.setChecked(pref->isMarkOfTheWebEnabled());
    addRow(ENABLE_MARK_OF_THE_WEB, motwLabel, &m_checkBoxMarkOfTheWeb);
#endif // Q_OS_MACOS || Q_OS_WIN
    // Ignore SSL errors
    m_checkBoxIgnoreSSLErrors.setChecked(pref->isIgnoreSSLErrors());
    m_checkBoxIgnoreSSLErrors.setToolTip(tr("Affects certificate validation and non-torrent protocol activities (e.g. RSS feeds, program updates, torrent files, geoip db, etc)"));
    addRow(IGNORE_SSL_ERRORS, tr("Ignore SSL errors"), &m_checkBoxIgnoreSSLErrors);
    // Python executable path
    m_pythonExecutablePath.setPlaceholderText(tr("(Auto detect if empty)"));
    m_pythonExecutablePath.setText(pref->getPythonExecutablePath().toString());
    addRow(PYTHON_EXECUTABLE_PATH, tr("Python executable path (may require restart)"), &m_pythonExecutablePath);
    // Start session paused
    m_checkBoxStartSessionPaused.setChecked(session->isStartPaused());
    addRow(START_SESSION_PAUSED, tr("Start BitTorrent session in paused state"), &m_checkBoxStartSessionPaused);
    // Session shutdown timeout
    m_spinBoxSessionShutdownTimeout.setMinimum(-1);
    m_spinBoxSessionShutdownTimeout.setMaximum(std::numeric_limits<int>::max());
    m_spinBoxSessionShutdownTimeout.setValue(session->shutdownTimeout());
    m_spinBoxSessionShutdownTimeout.setSuffix(tr(" sec", " seconds"));
    m_spinBoxSessionShutdownTimeout.setSpecialValueText(tr("-1 (unlimited)"));
    m_spinBoxSessionShutdownTimeout.setToolTip(u"Sets the timeout for the session to be shut down gracefully, at which point it will be forcibly terminated.<br>Note that this does not apply to the saving resume data time."_s);
    addRow(SESSION_SHUTDOWN_TIMEOUT, tr("BitTorrent session shutdown timeout [-1: unlimited]"), &m_spinBoxSessionShutdownTimeout);
    // Choking algorithm
    m_comboBoxChokingAlgorithm.addItem(tr("Fixed slots"), QVariant::fromValue(BitTorrent::ChokingAlgorithm::FixedSlots));
    m_comboBoxChokingAlgorithm.addItem(tr("Upload rate based"), QVariant::fromValue(BitTorrent::ChokingAlgorithm::RateBased));
    m_comboBoxChokingAlgorithm.setCurrentIndex(m_comboBoxChokingAlgorithm.findData(QVariant::fromValue(session->chokingAlgorithm())));
    addRow(CHOKING_ALGORITHM, (tr("Upload slots behavior") + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#choking_algorithm", u"(?)"))
            , &m_comboBoxChokingAlgorithm);
    // Seed choking algorithm
    m_comboBoxSeedChokingAlgorithm.addItem(tr("Round-robin"), QVariant::fromValue(BitTorrent::SeedChokingAlgorithm::RoundRobin));
    m_comboBoxSeedChokingAlgorithm.addItem(tr("Fastest upload"), QVariant::fromValue(BitTorrent::SeedChokingAlgorithm::FastestUpload));
    m_comboBoxSeedChokingAlgorithm.addItem(tr("Anti-leech"), QVariant::fromValue(BitTorrent::SeedChokingAlgorithm::AntiLeech));
    m_comboBoxSeedChokingAlgorithm.setCurrentIndex(m_comboBoxSeedChokingAlgorithm.findData(QVariant::fromValue(session->seedChokingAlgorithm())));
    addRow(SEED_CHOKING_ALGORITHM, (tr("Upload choking algorithm") + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#seed_choking_algorithm", u"(?)"))
            , &m_comboBoxSeedChokingAlgorithm);

    // Torrent recheck confirmation
    m_checkBoxConfirmTorrentRecheck.setChecked(pref->confirmTorrentRecheck());
    addRow(CONFIRM_RECHECK_TORRENT, tr("Confirm torrent recheck"), &m_checkBoxConfirmTorrentRecheck);

    // Remove all tags confirmation
    m_checkBoxConfirmRemoveAllTags.setChecked(pref->confirmRemoveAllTags());
    addRow(CONFIRM_REMOVE_ALL_TAGS, tr("Confirm removal of all tags"), &m_checkBoxConfirmRemoveAllTags);

    // Remove tracker from all torrents confirmation
    m_checkBoxConfirmRemoveTrackerFromAllTorrents.setChecked(pref->confirmRemoveTrackerFromAllTorrents());
    addRow(CONFIRM_REMOVE_TRACKER_FROM_ALL_TORRENTS, tr("Confirm removal of tracker from all torrents"), &m_checkBoxConfirmRemoveTrackerFromAllTorrents);

    // Announce to all trackers in a tier
    m_checkBoxAnnounceAllTrackers.setChecked(session->announceToAllTrackers());
    addRow(ANNOUNCE_ALL_TRACKERS, (tr("Always announce to all trackers in a tier")
        + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#announce_to_all_trackers", u"(?)"))
        , &m_checkBoxAnnounceAllTrackers);

    // Announce to all tiers
    m_checkBoxAnnounceAllTiers.setChecked(session->announceToAllTiers());
    addRow(ANNOUNCE_ALL_TIERS, (tr("Always announce to all tiers")
        + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#announce_to_all_tiers", u"(?)"))
        , &m_checkBoxAnnounceAllTiers);

    m_spinBoxPeerTurnover.setMinimum(0);
    m_spinBoxPeerTurnover.setMaximum(100);
    m_spinBoxPeerTurnover.setValue(session->peerTurnover());
    m_spinBoxPeerTurnover.setSuffix(u" %"_s);
    addRow(PEER_TURNOVER, (tr("Peer turnover disconnect percentage") + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#peer_turnover", u"(?)"))
            , &m_spinBoxPeerTurnover);
    m_spinBoxPeerTurnoverCutoff.setMinimum(0);
    m_spinBoxPeerTurnoverCutoff.setMaximum(100);
    m_spinBoxPeerTurnoverCutoff.setSuffix(u" %"_s);
    m_spinBoxPeerTurnoverCutoff.setValue(session->peerTurnoverCutoff());
    addRow(PEER_TURNOVER_CUTOFF, (tr("Peer turnover threshold percentage") + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#peer_turnover", u"(?)"))
            , &m_spinBoxPeerTurnoverCutoff);
    m_spinBoxPeerTurnoverInterval.setMinimum(30);
    m_spinBoxPeerTurnoverInterval.setMaximum(3600);
    m_spinBoxPeerTurnoverInterval.setSuffix(tr(" s", " seconds"));
    m_spinBoxPeerTurnoverInterval.setValue(session->peerTurnoverInterval());
    addRow(PEER_TURNOVER_INTERVAL, (tr("Peer turnover disconnect interval") + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#peer_turnover", u"(?)"))
            , &m_spinBoxPeerTurnoverInterval);
    // Maximum outstanding requests to a single peer
    m_spinBoxRequestQueueSize.setMinimum(1);
    m_spinBoxRequestQueueSize.setMaximum(std::numeric_limits<int>::max());
    m_spinBoxRequestQueueSize.setValue(session->requestQueueSize());
    addRow(REQUEST_QUEUE_SIZE, (tr("Maximum outstanding requests to a single peer") + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#max_out_request_queue", u"(?)"))
            , &m_spinBoxRequestQueueSize);
    // DHT bootstrap nodes
    m_lineEditDHTBootstrapNodes.setPlaceholderText(tr("Resets to default if empty"));
    m_lineEditDHTBootstrapNodes.setText(session->getDHTBootstrapNodes());
    addRow(DHT_BOOTSTRAP_NODES, (tr("DHT bootstrap nodes") + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#dht_bootstrap_nodes", u"(?)"))
        , &m_lineEditDHTBootstrapNodes);
#if defined(QBT_USES_LIBTORRENT2) && TORRENT_USE_I2P
    // I2P session options
    m_spinBoxI2PInboundQuantity.setMinimum(1);
    m_spinBoxI2PInboundQuantity.setMaximum(16);
    m_spinBoxI2PInboundQuantity.setValue(session->I2PInboundQuantity());
    addRow(I2P_INBOUND_QUANTITY, (tr("I2P inbound quantity") + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#i2p_inbound_quantity", u"(?)"))
        , &m_spinBoxI2PInboundQuantity);
    m_spinBoxI2POutboundQuantity.setMinimum(1);
    m_spinBoxI2POutboundQuantity.setMaximum(16);
    m_spinBoxI2POutboundQuantity.setValue(session->I2POutboundQuantity());
    addRow(I2P_OUTBOUND_QUANTITY, (tr("I2P outbound quantity") + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#i2p_outbound_quantity", u"(?)"))
        , &m_spinBoxI2POutboundQuantity);
    m_spinBoxI2PInboundLength.setMinimum(0);
    m_spinBoxI2PInboundLength.setMaximum(7);
    m_spinBoxI2PInboundLength.setValue(session->I2PInboundLength());
    addRow(I2P_INBOUND_LENGTH, (tr("I2P inbound length") + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#i2p_inbound_length", u"(?)"))
        , &m_spinBoxI2PInboundLength);
    m_spinBoxI2POutboundLength.setMinimum(0);
    m_spinBoxI2POutboundLength.setMaximum(7);
    m_spinBoxI2POutboundLength.setValue(session->I2POutboundLength());
    addRow(I2P_OUTBOUND_LENGTH, (tr("I2P outbound length") + u' ' + makeLink(u"https://www.libtorrent.org/reference-Settings.html#i2p_outbound_length", u"(?)"))
        , &m_spinBoxI2POutboundLength);
#endif
}

template <typename T>
void AdvancedSettings::addRow(const int row, const QString &text, T *widget)
{
    auto *label = new QLabel(text);
    label->setOpenExternalLinks(true);
    label->setToolTip(widget->toolTip());

    setCellWidget(row, PROPERTY, label);
    setCellWidget(row, VALUE, widget);

    if constexpr (std::is_same_v<T, QCheckBox>)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        connect(widget, &QCheckBox::checkStateChanged, this, &AdvancedSettings::settingsChanged);
#else
        connect(widget, &QCheckBox::stateChanged, this, &AdvancedSettings::settingsChanged);
#endif
    }
    else if constexpr (std::is_same_v<T, QSpinBox>)
        connect(widget, qOverload<int>(&QSpinBox::valueChanged), this, &AdvancedSettings::settingsChanged);
    else if constexpr (std::is_same_v<T, QComboBox>)
        connect(widget, qOverload<int>(&QComboBox::currentIndexChanged), this, &AdvancedSettings::settingsChanged);
    else if constexpr (std::is_same_v<T, QLineEdit>)
        connect(widget, &QLineEdit::textChanged, this, &AdvancedSettings::settingsChanged);
}
