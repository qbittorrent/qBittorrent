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

#include <QFont>
#include <QHeaderView>
#include <QHostAddress>
#include <QNetworkInterface>

#include "base/bittorrent/session.h"
#include "base/global.h"
#include "base/preferences.h"
#include "base/unicodestrings.h"
#include "app/application.h"
#include "gui/addnewtorrentdialog.h"
#include "gui/mainwindow.h"

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
    // network interface
    NETWORK_IFACE,
    //Optional network address
    NETWORK_IFACE_ADDRESS,
    NETWORK_LISTEN_IPV6,
    // behavior
    SAVE_RESUME_DATA_INTERVAL,
    CONFIRM_RECHECK_TORRENT,
    RECHECK_COMPLETED,
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    UPDATE_CHECK,
#endif
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
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC))
    USE_ICON_THEME,
#endif

    // libtorrent section
    LIBTORRENT_HEADER,
#if LIBTORRENT_VERSION_NUM >= 10100
    ASYNC_IO_THREADS,
#endif
    CHECKING_MEM_USAGE,
    // cache
    DISK_CACHE,
    DISK_CACHE_TTL,
    OS_CACHE,
    GUIDED_READ_CACHE,
#if LIBTORRENT_VERSION_NUM >= 10107
    COALESCE_RW,
#endif
    SUGGEST_MODE,
    SEND_BUF_WATERMARK,
    SEND_BUF_LOW_WATERMARK,
    SEND_BUF_WATERMARK_FACTOR,
    // ports
    MAX_HALF_OPEN,
    OUTGOING_PORT_MIN,
    OUTGOING_PORT_MAX,
    UTP_MIX_MODE,
    MULTI_CONNECTIONS_PER_IP,
    // embedded tracker
    TRACKER_STATUS,
    TRACKER_PORT,
    // seeding
    CHOKING_ALGORITHM,
    SEED_CHOKING_ALGORITHM,
    SUPER_SEEDING,
    // tracker
    ANNOUNCE_ALL_TRACKERS,
    ANNOUNCE_ALL_TIERS,
    ANNOUNCE_IP,

    ROW_COUNT
};

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
    // Signals
    connect(&spinBoxCache, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged)
            , this, &AdvancedSettings::updateCacheSpinSuffix);
    connect(&comboBoxInterface, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged)
            , this, &AdvancedSettings::updateInterfaceAddressCombo);
    connect(&spinBoxSaveResumeDataInterval, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged)
            , this, &AdvancedSettings::updateSaveResumeDataIntervalSuffix);
    // Load settings
    loadAdvancedSettings();
    resizeColumnToContents(0);
    horizontalHeader()->setStretchLastSection(true);
}

void AdvancedSettings::saveAdvancedSettings()
{
    Preferences *const pref = Preferences::instance();
    BitTorrent::Session *const session = BitTorrent::Session::instance();

#if LIBTORRENT_VERSION_NUM >= 10100
    // Async IO threads
    session->setAsyncIOThreads(spinBoxAsyncIOThreads.value());
#endif
    // Checking Memory Usage
    session->setCheckingMemUsage(spinBoxCheckingMemUsage.value());
    // Disk write cache
    session->setDiskCacheSize(spinBoxCache.value());
    session->setDiskCacheTTL(spinBoxCacheTTL.value());
    // Enable OS cache
    session->setUseOSCache(checkBoxOsCache.isChecked());
    // Guided read cache
    session->setGuidedReadCacheEnabled(checkBoxGuidedReadCache.isChecked());
    // Coalesce reads & writes
    session->setCoalesceReadWriteEnabled(checkBoxCoalesceRW.isChecked());
    // Suggest mode
    session->setSuggestMode(checkBoxSuggestMode.isChecked());
    // Send buffer watermark
    session->setSendBufferWatermark(spinBoxSendBufferWatermark.value());
    session->setSendBufferLowWatermark(spinBoxSendBufferLowWatermark.value());
    session->setSendBufferWatermarkFactor(spinBoxSendBufferWatermarkFactor.value());
    // Save resume data interval
    session->setSaveResumeDataInterval(spinBoxSaveResumeDataInterval.value());
    // Outgoing ports
    session->setOutgoingPortsMin(spinBoxOutgoingPortsMin.value());
    session->setOutgoingPortsMax(spinBoxOutgoingPortsMax.value());
    // uTP-TCP mixed mode
    session->setUtpMixedMode(static_cast<BitTorrent::MixedModeAlgorithm>(comboBoxUtpMixedMode.currentIndex()));
    // multiple connections per IP
    session->setMultiConnectionsPerIpEnabled(checkBoxMultiConnectionsPerIp.isChecked());
    // Recheck torrents on completion
    pref->recheckTorrentsOnCompletion(checkBoxRecheckCompleted.isChecked());
    // Transfer list refresh interval
    session->setRefreshInterval(spinBoxListRefresh.value());
    // Peer resolution
    pref->resolvePeerCountries(checkBoxResolveCountries.isChecked());
    pref->resolvePeerHostNames(checkBoxResolveHosts.isChecked());
    // Max Half-Open connections
    session->setMaxHalfOpenConnections(spinBoxMaxHalfOpen.value());
    // Super seeding
    session->setSuperSeedingEnabled(checkBoxSuperSeeding.isChecked());
    // Network interface
    if (comboBoxInterface.currentIndex() == 0) {
        // All interfaces (default)
        session->setNetworkInterface(QString());
        session->setNetworkInterfaceName(QString());
    }
    else {
        session->setNetworkInterface(comboBoxInterface.itemData(comboBoxInterface.currentIndex()).toString());
        session->setNetworkInterfaceName(comboBoxInterface.currentText());
    }

    // Interface address
    if (comboBoxInterfaceAddress.currentIndex() == 0) {
        // All addresses (default)
        session->setNetworkInterfaceAddress(QString::null);
    }
    else {
        QHostAddress ifaceAddr(comboBoxInterfaceAddress.currentText().trimmed());
        ifaceAddr.isNull() ? session->setNetworkInterfaceAddress(QString::null) : session->setNetworkInterfaceAddress(ifaceAddr.toString());
    }
    session->setIPv6Enabled(checkBoxListenIPv6.isChecked());
    // Announce IP
    QHostAddress addr(lineEditAnnounceIP.text().trimmed());
    session->setAnnounceIP(addr.isNull() ? "" : addr.toString());

    // Program notification
    MainWindow *const mainWindow = static_cast<Application*>(QCoreApplication::instance())->mainWindow();
    mainWindow->setNotificationsEnabled(checkBoxProgramNotifications.isChecked());
    mainWindow->setTorrentAddedNotificationsEnabled(checkBoxTorrentAddedNotifications.isChecked());
    // Misc GUI properties
    mainWindow->setDownloadTrackerFavicon(checkBoxTrackerFavicon.isChecked());
    AddNewTorrentDialog::setSavePathHistoryLength(spinBoxSavePathHistoryLength.value());
    pref->setSpeedWidgetEnabled(checkBoxSpeedWidgetEnabled.isChecked());

    // Tracker
    session->setTrackerEnabled(checkBoxTrackerStatus.isChecked());
    pref->setTrackerPort(spinBoxTrackerPort.value());
    // Choking algorithm
    session->setChokingAlgorithm(static_cast<BitTorrent::ChokingAlgorithm>(comboBoxChokingAlgorithm.currentIndex()));
    // Seed choking algorithm
    session->setSeedChokingAlgorithm(static_cast<BitTorrent::SeedChokingAlgorithm>(comboBoxSeedChokingAlgorithm.currentIndex()));

#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    pref->setUpdateCheckEnabled(checkBoxUpdateCheck.isChecked());
#endif
    // Icon theme
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC))
    pref->useSystemIconTheme(checkBoxUseIconTheme.isChecked());
#endif
    pref->setConfirmTorrentRecheck(checkBoxConfirmTorrentRecheck.isChecked());

    pref->setConfirmRemoveAllTags(checkBoxConfirmRemoveAllTags.isChecked());

    session->setAnnounceToAllTrackers(checkBoxAnnounceAllTrackers.isChecked());
    session->setAnnounceToAllTiers(checkBoxAnnounceAllTiers.isChecked());
}

void AdvancedSettings::updateCacheSpinSuffix(int value)
{
    if (value == 0)
        spinBoxCache.setSuffix(tr(" (disabled)"));
    else if (value < 0)
        spinBoxCache.setSuffix(tr(" (auto)"));
    else
        spinBoxCache.setSuffix(tr(" MiB"));
}

void AdvancedSettings::updateSaveResumeDataIntervalSuffix(const int value)
{
    if (value > 0)
        spinBoxSaveResumeDataInterval.setSuffix(tr(" min", " minutes"));
    else
        spinBoxSaveResumeDataInterval.setSuffix(tr(" (disabled)"));
}

void AdvancedSettings::updateInterfaceAddressCombo()
{
    // Try to get the currently selected interface name
    const QString ifaceName = comboBoxInterface.itemData(comboBoxInterface.currentIndex()).toString(); // Empty string for the first element
    const QString currentAddress = BitTorrent::Session::instance()->networkInterfaceAddress();

    // Clear all items and reinsert them, default to all
    comboBoxInterfaceAddress.clear();
    comboBoxInterfaceAddress.addItem(tr("All addresses"));
    comboBoxInterfaceAddress.setCurrentIndex(0);

    auto populateCombo = [this, &currentAddress](const QString &ip, const QAbstractSocket::NetworkLayerProtocol &protocol)
    {
        Q_ASSERT((protocol == QAbstractSocket::IPv4Protocol) || (protocol == QAbstractSocket::IPv6Protocol));
        // Only take ipv4 for now?
        if ((protocol != QAbstractSocket::IPv4Protocol) && (protocol != QAbstractSocket::IPv6Protocol))
            return;
        comboBoxInterfaceAddress.addItem(ip);
        //Try to select the last added one
        if (ip == currentAddress)
            comboBoxInterfaceAddress.setCurrentIndex(comboBoxInterfaceAddress.count() - 1);
    };

    if (ifaceName.isEmpty()) {
        for (const QHostAddress &ip : asConst(QNetworkInterface::allAddresses()))
            populateCombo(ip.toString(), ip.protocol());
    }
    else {
        const QNetworkInterface iface = QNetworkInterface::interfaceFromName(ifaceName);
        const QList<QNetworkAddressEntry> addresses = iface.addressEntries();
        for (const QNetworkAddressEntry &entry : addresses) {
            const QHostAddress ip = entry.ip();
            populateCombo(ip.toString(), ip.protocol());
        }
    }
}

void AdvancedSettings::loadAdvancedSettings()
{
    const Preferences *const pref = Preferences::instance();
    const BitTorrent::Session *const session = BitTorrent::Session::instance();

    // add section headers
    QFont boldFont;
    boldFont.setBold(true);
    addRow(QBITTORRENT_HEADER, tr("qBittorrent Section"), &labelQbtLink);
    item(QBITTORRENT_HEADER, PROPERTY)->setFont(boldFont);
    labelQbtLink.setText(QString("<a href=\"%1\">%2</a>")
        .arg("https://github.com/qbittorrent/qBittorrent/wiki/Explanation-of-Options-in-qBittorrent#Advanced", tr("Open documentation")));
    labelQbtLink.setOpenExternalLinks(true);

    addRow(LIBTORRENT_HEADER, tr("libtorrent Section"), &labelLibtorrentLink);
    item(LIBTORRENT_HEADER, PROPERTY)->setFont(boldFont);
    labelLibtorrentLink.setText(QString("<a href=\"%1\">%2</a>").arg("https://www.libtorrent.org/reference.html", tr("Open documentation")));
    labelLibtorrentLink.setOpenExternalLinks(true);

#if LIBTORRENT_VERSION_NUM >= 10100
    // Async IO threads
    spinBoxAsyncIOThreads.setMinimum(1);
    spinBoxAsyncIOThreads.setMaximum(1024);
    spinBoxAsyncIOThreads.setValue(session->asyncIOThreads());
    addRow(ASYNC_IO_THREADS, tr("Asynchronous I/O threads"), &spinBoxAsyncIOThreads);
#endif

    // Checking Memory Usage
    spinBoxCheckingMemUsage.setMinimum(1);
    spinBoxCheckingMemUsage.setValue(session->checkingMemUsage());
    spinBoxCheckingMemUsage.setSuffix(tr(" MiB"));
    addRow(CHECKING_MEM_USAGE, tr("Outstanding memory when checking torrents"), &spinBoxCheckingMemUsage);

    // Disk write cache
    spinBoxCache.setMinimum(-1);
    // When build as 32bit binary, set the maximum at less than 2GB to prevent crashes.
    // These macros may not be available on compilers other than MSVC and GCC
#if defined(__x86_64__) || defined(_M_X64)
    spinBoxCache.setMaximum(4096);
#else
    // allocate 1536MiB and leave 512MiB to the rest of program data in RAM
    spinBoxCache.setMaximum(1536);
#endif
    spinBoxCache.setValue(session->diskCacheSize());
    updateCacheSpinSuffix(spinBoxCache.value());
    addRow(DISK_CACHE, tr("Disk cache"), &spinBoxCache);
    // Disk cache expiry
    spinBoxCacheTTL.setMinimum(15);
    spinBoxCacheTTL.setMaximum(600);
    spinBoxCacheTTL.setValue(session->diskCacheTTL());
    spinBoxCacheTTL.setSuffix(tr(" s", " seconds"));
    addRow(DISK_CACHE_TTL, tr("Disk cache expiry interval"), &spinBoxCacheTTL);
    // Enable OS cache
    checkBoxOsCache.setChecked(session->useOSCache());
    addRow(OS_CACHE, tr("Enable OS cache"), &checkBoxOsCache);
    // Guided read cache
    checkBoxGuidedReadCache.setChecked(session->isGuidedReadCacheEnabled());
    addRow(GUIDED_READ_CACHE, tr("Guided read cache"), &checkBoxGuidedReadCache);
    // Coalesce reads & writes
    checkBoxCoalesceRW.setChecked(session->isCoalesceReadWriteEnabled());
#if LIBTORRENT_VERSION_NUM >= 10107
    addRow(COALESCE_RW, tr("Coalesce reads & writes"), &checkBoxCoalesceRW);
#endif
    // Suggest mode
    checkBoxSuggestMode.setChecked(session->isSuggestModeEnabled());
    addRow(SUGGEST_MODE, tr("Send upload piece suggestions"), &checkBoxSuggestMode);
    // Send buffer watermark
    spinBoxSendBufferWatermark.setMinimum(1);
    spinBoxSendBufferWatermark.setMaximum(INT_MAX);
    spinBoxSendBufferWatermark.setSuffix(tr(" KiB"));
    spinBoxSendBufferWatermark.setValue(session->sendBufferWatermark());
    addRow(SEND_BUF_WATERMARK, tr("Send buffer watermark"), &spinBoxSendBufferWatermark);
    spinBoxSendBufferLowWatermark.setMinimum(1);
    spinBoxSendBufferLowWatermark.setMaximum(INT_MAX);
    spinBoxSendBufferLowWatermark.setSuffix(tr(" KiB"));
    spinBoxSendBufferLowWatermark.setValue(session->sendBufferLowWatermark());
    addRow(SEND_BUF_LOW_WATERMARK, tr("Send buffer low watermark"), &spinBoxSendBufferLowWatermark);
    spinBoxSendBufferWatermarkFactor.setMinimum(1);
    spinBoxSendBufferWatermarkFactor.setMaximum(INT_MAX);
    spinBoxSendBufferWatermarkFactor.setSuffix(" %");
    spinBoxSendBufferWatermarkFactor.setValue(session->sendBufferWatermarkFactor());
    addRow(SEND_BUF_WATERMARK_FACTOR, tr("Send buffer watermark factor"), &spinBoxSendBufferWatermarkFactor);
    // Save resume data interval
    spinBoxSaveResumeDataInterval.setMinimum(0);
    spinBoxSaveResumeDataInterval.setMaximum(std::numeric_limits<int>::max());
    spinBoxSaveResumeDataInterval.setValue(session->saveResumeDataInterval());
    updateSaveResumeDataIntervalSuffix(spinBoxSaveResumeDataInterval.value());
    addRow(SAVE_RESUME_DATA_INTERVAL, tr("Save resume data interval", "How often the fastresume file is saved."), &spinBoxSaveResumeDataInterval);
    // Outgoing port Min
    spinBoxOutgoingPortsMin.setMinimum(0);
    spinBoxOutgoingPortsMin.setMaximum(65535);
    spinBoxOutgoingPortsMin.setValue(session->outgoingPortsMin());
    addRow(OUTGOING_PORT_MIN, tr("Outgoing ports (Min) [0: Disabled]"), &spinBoxOutgoingPortsMin);
    // Outgoing port Min
    spinBoxOutgoingPortsMax.setMinimum(0);
    spinBoxOutgoingPortsMax.setMaximum(65535);
    spinBoxOutgoingPortsMax.setValue(session->outgoingPortsMax());
    addRow(OUTGOING_PORT_MAX, tr("Outgoing ports (Max) [0: Disabled]"), &spinBoxOutgoingPortsMax);
    // uTP-TCP mixed mode
    comboBoxUtpMixedMode.addItems({tr("Prefer TCP"), tr("Peer proportional (throttles TCP)")});
    comboBoxUtpMixedMode.setCurrentIndex(static_cast<int>(session->utpMixedMode()));
    addRow(UTP_MIX_MODE, tr("%1-TCP mixed mode algorithm", "uTP-TCP mixed mode algorithm").arg(C_UTP), &comboBoxUtpMixedMode);
    // multiple connections per IP
    checkBoxMultiConnectionsPerIp.setChecked(session->multiConnectionsPerIpEnabled());
    addRow(MULTI_CONNECTIONS_PER_IP, tr("Allow multiple connections from the same IP address"), &checkBoxMultiConnectionsPerIp);
    // Recheck completed torrents
    checkBoxRecheckCompleted.setChecked(pref->recheckTorrentsOnCompletion());
    addRow(RECHECK_COMPLETED, tr("Recheck torrents on completion"), &checkBoxRecheckCompleted);
    // Transfer list refresh interval
    spinBoxListRefresh.setMinimum(30);
    spinBoxListRefresh.setMaximum(99999);
    spinBoxListRefresh.setValue(session->refreshInterval());
    spinBoxListRefresh.setSuffix(tr(" ms", " milliseconds"));
    addRow(LIST_REFRESH, tr("Transfer list refresh interval"), &spinBoxListRefresh);
    // Resolve Peer countries
    checkBoxResolveCountries.setChecked(pref->resolvePeerCountries());
    addRow(RESOLVE_COUNTRIES, tr("Resolve peer countries (GeoIP)"), &checkBoxResolveCountries);
    // Resolve peer hosts
    checkBoxResolveHosts.setChecked(pref->resolvePeerHostNames());
    addRow(RESOLVE_HOSTS, tr("Resolve peer host names"), &checkBoxResolveHosts);
    // Max Half Open connections
    spinBoxMaxHalfOpen.setMinimum(0);
    spinBoxMaxHalfOpen.setMaximum(99999);
    spinBoxMaxHalfOpen.setValue(session->maxHalfOpenConnections());
    addRow(MAX_HALF_OPEN, tr("Maximum number of half-open connections [0: Unlimited]"), &spinBoxMaxHalfOpen);
    // Super seeding
    checkBoxSuperSeeding.setChecked(session->isSuperSeedingEnabled());
    addRow(SUPER_SEEDING, tr("Strict super seeding"), &checkBoxSuperSeeding);
    // Network interface
    comboBoxInterface.addItem(tr("Any interface", "i.e. Any network interface"));
    const QString currentInterface = session->networkInterface();
    bool interfaceExists = currentInterface.isEmpty();
    int i = 1;
    for (const QNetworkInterface &iface : asConst(QNetworkInterface::allInterfaces())) {
        // This line fixes a Qt bug => https://bugreports.qt.io/browse/QTBUG-52633
        // Tested in Qt 5.6.0. For more info see:
        // https://github.com/qbittorrent/qBittorrent/issues/5131
        // https://github.com/qbittorrent/qBittorrent/pull/5135
        if (iface.addressEntries().isEmpty()) continue;

        comboBoxInterface.addItem(iface.humanReadableName(), iface.name());
        if (!currentInterface.isEmpty() && (iface.name() == currentInterface)) {
            comboBoxInterface.setCurrentIndex(i);
            interfaceExists = true;
        }
        ++i;
    }
    // Saved interface does not exist, show it anyway
    if (!interfaceExists) {
        comboBoxInterface.addItem(session->networkInterfaceName(), currentInterface);
        comboBoxInterface.setCurrentIndex(i);
    }
    addRow(NETWORK_IFACE, tr("Network Interface (requires restart)"), &comboBoxInterface);
    // Network interface address
    updateInterfaceAddressCombo();
    addRow(NETWORK_IFACE_ADDRESS, tr("Optional IP Address to bind to (requires restart)"), &comboBoxInterfaceAddress);
    // Listen on IPv6 address
    checkBoxListenIPv6.setChecked(session->isIPv6Enabled());
    addRow(NETWORK_LISTEN_IPV6, tr("Listen on IPv6 address (requires restart)"), &checkBoxListenIPv6);
    // Announce IP
    lineEditAnnounceIP.setText(session->announceIP());
    addRow(ANNOUNCE_IP, tr("IP Address to report to trackers (requires restart)"), &lineEditAnnounceIP);

    // Program notifications
    const MainWindow *const mainWindow = static_cast<Application*>(QCoreApplication::instance())->mainWindow();
    checkBoxProgramNotifications.setChecked(mainWindow->isNotificationsEnabled());
    addRow(PROGRAM_NOTIFICATIONS, tr("Display notifications"), &checkBoxProgramNotifications);
    // Torrent added notifications
    checkBoxTorrentAddedNotifications.setChecked(mainWindow->isTorrentAddedNotificationsEnabled());
    addRow(TORRENT_ADDED_NOTIFICATIONS, tr("Display notifications for added torrents"), &checkBoxTorrentAddedNotifications);
    // Download tracker's favicon
    checkBoxTrackerFavicon.setChecked(mainWindow->isDownloadTrackerFavicon());
    addRow(DOWNLOAD_TRACKER_FAVICON, tr("Download tracker's favicon"), &checkBoxTrackerFavicon);
    // Save path history length
    spinBoxSavePathHistoryLength.setRange(AddNewTorrentDialog::minPathHistoryLength, AddNewTorrentDialog::maxPathHistoryLength);
    spinBoxSavePathHistoryLength.setValue(AddNewTorrentDialog::savePathHistoryLength());
    addRow(SAVE_PATH_HISTORY_LENGTH, tr("Save path history length"), &spinBoxSavePathHistoryLength);
    // Enable speed graphs
    checkBoxSpeedWidgetEnabled.setChecked(pref->isSpeedWidgetEnabled());
    addRow(ENABLE_SPEED_WIDGET, tr("Enable speed graphs"), &checkBoxSpeedWidgetEnabled);
    // Tracker State
    checkBoxTrackerStatus.setChecked(session->isTrackerEnabled());
    addRow(TRACKER_STATUS, tr("Enable embedded tracker"), &checkBoxTrackerStatus);
    // Tracker port
    spinBoxTrackerPort.setMinimum(1);
    spinBoxTrackerPort.setMaximum(65535);
    spinBoxTrackerPort.setValue(pref->getTrackerPort());
    addRow(TRACKER_PORT, tr("Embedded tracker port"), &spinBoxTrackerPort);
    // Choking algorithm
    comboBoxChokingAlgorithm.addItems({tr("Fixed slots"), tr("Upload rate based")});
    comboBoxChokingAlgorithm.setCurrentIndex(static_cast<int>(session->chokingAlgorithm()));
    addRow(CHOKING_ALGORITHM, tr("Upload slots behavior"), &comboBoxChokingAlgorithm);
    // Seed choking algorithm
    comboBoxSeedChokingAlgorithm.addItems({tr("Round-robin"), tr("Fastest upload"), tr("Anti-leech")});
    comboBoxSeedChokingAlgorithm.setCurrentIndex(static_cast<int>(session->seedChokingAlgorithm()));
    addRow(SEED_CHOKING_ALGORITHM, tr("Upload choking algorithm"), &comboBoxSeedChokingAlgorithm);

#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    checkBoxUpdateCheck.setChecked(pref->isUpdateCheckEnabled());
    addRow(UPDATE_CHECK, tr("Check for software updates"), &checkBoxUpdateCheck);
#endif
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC))
    checkBoxUseIconTheme.setChecked(pref->useSystemIconTheme());
    addRow(USE_ICON_THEME, tr("Use system icon theme"), &checkBoxUseIconTheme);
#endif
    // Torrent recheck confirmation
    checkBoxConfirmTorrentRecheck.setChecked(pref->confirmTorrentRecheck());
    addRow(CONFIRM_RECHECK_TORRENT, tr("Confirm torrent recheck"), &checkBoxConfirmTorrentRecheck);

    // Remove all tags confirmation
    checkBoxConfirmRemoveAllTags.setChecked(pref->confirmRemoveAllTags());
    addRow(CONFIRM_REMOVE_ALL_TAGS, tr("Confirm removal of all tags"), &checkBoxConfirmRemoveAllTags);

    // Announce to all trackers in a tier
    checkBoxAnnounceAllTrackers.setChecked(session->announceToAllTrackers());
    addRow(ANNOUNCE_ALL_TRACKERS, tr("Always announce to all trackers in a tier"), &checkBoxAnnounceAllTrackers);

    // Announce to all tiers
    checkBoxAnnounceAllTiers.setChecked(session->announceToAllTiers());
    addRow(ANNOUNCE_ALL_TIERS, tr("Always announce to all tiers"), &checkBoxAnnounceAllTiers);
}

template <typename T>
void AdvancedSettings::addRow(int row, const QString &rowText, T *widget)
{
    setItem(row, PROPERTY, new QTableWidgetItem(rowText));
    setCellWidget(row, VALUE, widget);

    if (std::is_same<T, QCheckBox>::value)
        connect(widget, SIGNAL(stateChanged(int)), SIGNAL(settingsChanged()));
    else if (std::is_same<T, QSpinBox>::value)
        connect(widget, SIGNAL(valueChanged(int)), SIGNAL(settingsChanged()));
    else if (std::is_same<T, QComboBox>::value)
        connect(widget, SIGNAL(currentIndexChanged(int)), SIGNAL(settingsChanged()));
    else if (std::is_same<T, QLineEdit>::value)
        connect(widget, SIGNAL(textChanged(QString)), SIGNAL(settingsChanged()));
}
