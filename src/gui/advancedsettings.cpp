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

#include <QFont>
#include <QHeaderView>
#include <QHostAddress>
#include <QNetworkInterface>

#include "app/application.h"
#include "base/bittorrent/session.h"
#include "base/preferences.h"
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
    DOWNLOAD_TRACKER_FAVICON,
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC))
    USE_ICON_THEME,
#endif

    // libtorrent section
    LIBTORRENT_HEADER,
    // cache
    DISK_CACHE,
    DISK_CACHE_TTL,
    OS_CACHE,
    // ports
    MAX_HALF_OPEN,
    OUTGOING_PORT_MIN,
    OUTGOING_PORT_MAX,
    // embedded tracker
    TRACKER_STATUS,
    TRACKER_PORT,
    // seeding
    SUPER_SEEDING,
    // tracker
    ANNOUNCE_ALL_TRACKERS,
    ANNOUNCE_IP,

    ROW_COUNT
};

AdvancedSettings::AdvancedSettings(QWidget *parent)
    : QTableWidget(parent)
{
    // column
    setColumnCount(COL_COUNT);
    QStringList header = { tr("Setting"), tr("Value", "Value set for this setting") };
    setHorizontalHeaderLabels(header);
    // row
    setRowCount(ROW_COUNT);
    verticalHeader()->setVisible(false);
    // etc.
    setAlternatingRowColors(true);
    setSelectionMode(QAbstractItemView::NoSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    // Signals
    connect(&spin_cache, SIGNAL(valueChanged(int)), SLOT(updateCacheSpinSuffix(int)));
    connect(&combo_iface, SIGNAL(currentIndexChanged(int)), SLOT(updateInterfaceAddressCombo()));
    // Load settings
    loadAdvancedSettings();
    resizeColumnToContents(0);
    horizontalHeader()->setStretchLastSection(true);
}

void AdvancedSettings::saveAdvancedSettings()
{
    Preferences* const pref = Preferences::instance();
    BitTorrent::Session *const session = BitTorrent::Session::instance();

    // Disk write cache
    session->setDiskCacheSize(spin_cache.value());
    session->setDiskCacheTTL(spin_cache_ttl.value());
    // Enable OS cache
    session->setUseOSCache(cb_os_cache.isChecked());
    // Save resume data interval
    session->setSaveResumeDataInterval(spin_save_resume_data_interval.value());
    // Outgoing ports
    session->setOutgoingPortsMin(outgoing_ports_min.value());
    session->setOutgoingPortsMax(outgoing_ports_max.value());
    // Recheck torrents on completion
    pref->recheckTorrentsOnCompletion(cb_recheck_completed.isChecked());
    // Transfer list refresh interval
    session->setRefreshInterval(spin_list_refresh.value());
    // Peer resolution
    pref->resolvePeerCountries(cb_resolve_countries.isChecked());
    pref->resolvePeerHostNames(cb_resolve_hosts.isChecked());
    // Max Half-Open connections
    session->setMaxHalfOpenConnections(spin_maxhalfopen.value());
    // Super seeding
    session->setSuperSeedingEnabled(cb_super_seeding.isChecked());
    // Network interface
    if (combo_iface.currentIndex() == 0) {
        // All interfaces (default)
        session->setNetworkInterface(QString());
        session->setNetworkInterfaceName(QString());
    }
    else {
        session->setNetworkInterface(combo_iface.itemData(combo_iface.currentIndex()).toString());
        session->setNetworkInterfaceName(combo_iface.currentText());
    }

    // Interface address
    if (combo_iface_address.currentIndex() == 0) {
        // All addresses (default)
        session->setNetworkInterfaceAddress(QString::null);
    }
    else {
        QHostAddress ifaceAddr(combo_iface_address.currentText().trimmed());
        ifaceAddr.isNull() ? session->setNetworkInterfaceAddress(QString::null) : session->setNetworkInterfaceAddress(ifaceAddr.toString());
    }
    session->setIPv6Enabled(cb_listen_ipv6.isChecked());
    // Announce IP
    QHostAddress addr(txtAnnounceIP.text().trimmed());
    session->setAnnounceIP(addr.isNull() ? "" : addr.toString());

    // Program notification
    MainWindow * const mainWindow = static_cast<Application*>(QCoreApplication::instance())->mainWindow();
    mainWindow->setNotificationsEnabled(cb_program_notifications.isChecked());
    mainWindow->setTorrentAddedNotificationsEnabled(cb_torrent_added_notifications.isChecked());
    // Misc GUI properties
    mainWindow->setDownloadTrackerFavicon(cb_tracker_favicon.isChecked());

    // Tracker
    session->setTrackerEnabled(cb_tracker_status.isChecked());
    pref->setTrackerPort(spin_tracker_port.value());
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    pref->setUpdateCheckEnabled(cb_update_check.isChecked());
#endif
    // Icon theme
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC))
    pref->useSystemIconTheme(cb_use_icon_theme.isChecked());
#endif
    pref->setConfirmTorrentRecheck(cb_confirm_torrent_recheck.isChecked());
    session->setAnnounceToAllTrackers(cb_announce_all_trackers.isChecked());
}

void AdvancedSettings::updateCacheSpinSuffix(int value)
{
    if (value <= 0)
        spin_cache.setSuffix(tr(" (auto)"));
    else
        spin_cache.setSuffix(tr(" MiB"));
}

void AdvancedSettings::updateInterfaceAddressCombo()
{
    // Try to get the currently selected interface name
    const QString ifaceName = combo_iface.itemData(combo_iface.currentIndex()).toString(); // Empty string for the first element
    const QString currentAddress = BitTorrent::Session::instance()->networkInterfaceAddress();

    //Clear all items and reinsert them, default to all
    combo_iface_address.clear();
    combo_iface_address.addItem(tr("All addresses"));
    combo_iface_address.setCurrentIndex(0);

    auto populateCombo = [this, &currentAddress](const QString &ip, const QAbstractSocket::NetworkLayerProtocol &protocol)
    {
        Q_ASSERT(protocol == QAbstractSocket::IPv4Protocol || protocol == QAbstractSocket::IPv6Protocol);
        //Only take ipv4 for now?
        if (protocol != QAbstractSocket::IPv4Protocol && protocol != QAbstractSocket::IPv6Protocol)
            return;
        combo_iface_address.addItem(ip);
        //Try to select the last added one
        if (ip == currentAddress)
            combo_iface_address.setCurrentIndex(combo_iface_address.count() - 1);
    };

    if (ifaceName.isEmpty()) {
        foreach (const QHostAddress &ip, QNetworkInterface::allAddresses())
            populateCombo(ip.toString(), ip.protocol());
    }
    else {
        const QNetworkInterface iface = QNetworkInterface::interfaceFromName(ifaceName);
        const QList<QNetworkAddressEntry> addresses = iface.addressEntries();
        foreach (const QNetworkAddressEntry &entry, addresses) {
            const QHostAddress ip = entry.ip();
            populateCombo(ip.toString(), ip.protocol());
        }
    }
}

void AdvancedSettings::loadAdvancedSettings()
{
    const Preferences* const pref = Preferences::instance();
    const BitTorrent::Session *const session = BitTorrent::Session::instance();

    // add section headers
    QFont boldFont;
    boldFont.setBold(true);
    addRow(QBITTORRENT_HEADER, tr("qBittorrent Section"), &labelQbtLink);
    item(QBITTORRENT_HEADER, PROPERTY)->setFont(boldFont);
    labelQbtLink.setText(QString("<a href=\"%1\">%2</a>").arg("https://github.com/qbittorrent/qBittorrent/wiki/Explanation-of-Options-in-qBittorrent#Advanced").arg(tr("Open documentation")));
    labelQbtLink.setOpenExternalLinks(true);

    addRow(LIBTORRENT_HEADER, tr("libtorrent Section"), &labelLibtorrentLink);
    item(LIBTORRENT_HEADER, PROPERTY)->setFont(boldFont);
    labelLibtorrentLink.setText(QString("<a href=\"%1\">%2</a>").arg("http://www.libtorrent.org/reference.html").arg(tr("Open documentation")));
    labelLibtorrentLink.setOpenExternalLinks(true);
    // Disk write cache
    spin_cache.setMinimum(0);
    // When build as 32bit binary, set the maximum at less than 2GB to prevent crashes.
    // These macros may not be available on compilers other than MSVC and GCC
#if defined(__x86_64__) || defined(_M_X64)
    spin_cache.setMaximum(4096);
#else
    // allocate 1536MiB and leave 512MiB to the rest of program data in RAM
    spin_cache.setMaximum(1536);
#endif
    spin_cache.setValue(session->diskCacheSize());
    updateCacheSpinSuffix(spin_cache.value());
    addRow(DISK_CACHE, tr("Disk write cache size"), &spin_cache);
    // Disk cache expiry
    spin_cache_ttl.setMinimum(15);
    spin_cache_ttl.setMaximum(600);
    spin_cache_ttl.setValue(session->diskCacheTTL());
    spin_cache_ttl.setSuffix(tr(" s", " seconds"));
    addRow(DISK_CACHE_TTL, tr("Disk cache expiry interval"), &spin_cache_ttl);
    // Enable OS cache
    cb_os_cache.setChecked(session->useOSCache());
    addRow(OS_CACHE, tr("Enable OS cache"), &cb_os_cache);
    // Save resume data interval
    spin_save_resume_data_interval.setMinimum(1);
    spin_save_resume_data_interval.setMaximum(1440);
    spin_save_resume_data_interval.setValue(session->saveResumeDataInterval());
    spin_save_resume_data_interval.setSuffix(tr(" m", " minutes"));
    addRow(SAVE_RESUME_DATA_INTERVAL, tr("Save resume data interval", "How often the fastresume file is saved."), &spin_save_resume_data_interval);
    // Outgoing port Min
    outgoing_ports_min.setMinimum(0);
    outgoing_ports_min.setMaximum(65535);
    outgoing_ports_min.setValue(session->outgoingPortsMin());
    addRow(OUTGOING_PORT_MIN, tr("Outgoing ports (Min) [0: Disabled]"), &outgoing_ports_min);
    // Outgoing port Min
    outgoing_ports_max.setMinimum(0);
    outgoing_ports_max.setMaximum(65535);
    outgoing_ports_max.setValue(session->outgoingPortsMax());
    addRow(OUTGOING_PORT_MAX, tr("Outgoing ports (Max) [0: Disabled]"), &outgoing_ports_max);
    // Recheck completed torrents
    cb_recheck_completed.setChecked(pref->recheckTorrentsOnCompletion());
    addRow(RECHECK_COMPLETED, tr("Recheck torrents on completion"), &cb_recheck_completed);
    // Transfer list refresh interval
    spin_list_refresh.setMinimum(30);
    spin_list_refresh.setMaximum(99999);
    spin_list_refresh.setValue(session->refreshInterval());
    spin_list_refresh.setSuffix(tr(" ms", " milliseconds"));
    addRow(LIST_REFRESH, tr("Transfer list refresh interval"), &spin_list_refresh);
    // Resolve Peer countries
    cb_resolve_countries.setChecked(pref->resolvePeerCountries());
    addRow(RESOLVE_COUNTRIES, tr("Resolve peer countries (GeoIP)"), &cb_resolve_countries);
    // Resolve peer hosts
    cb_resolve_hosts.setChecked(pref->resolvePeerHostNames());
    addRow(RESOLVE_HOSTS, tr("Resolve peer host names"), &cb_resolve_hosts);
    // Max Half Open connections
    spin_maxhalfopen.setMinimum(0);
    spin_maxhalfopen.setMaximum(99999);
    spin_maxhalfopen.setValue(session->maxHalfOpenConnections());
    addRow(MAX_HALF_OPEN, tr("Maximum number of half-open connections [0: Unlimited]"), &spin_maxhalfopen);
    // Super seeding
    cb_super_seeding.setChecked(session->isSuperSeedingEnabled());
    addRow(SUPER_SEEDING, tr("Strict super seeding"), &cb_super_seeding);
    // Network interface
    combo_iface.addItem(tr("Any interface", "i.e. Any network interface"));
    const QString current_iface = session->networkInterface();
    bool interface_exists = current_iface.isEmpty();
    int i = 1;
    foreach (const QNetworkInterface& iface, QNetworkInterface::allInterfaces()) {
        // This line fixes a Qt bug => https://bugreports.qt.io/browse/QTBUG-52633
        // Tested in Qt 5.6.0. For more info see:
        // https://github.com/qbittorrent/qBittorrent/issues/5131
        // https://github.com/qbittorrent/qBittorrent/pull/5135
        if (iface.addressEntries().isEmpty()) continue;

        if (iface.flags() & QNetworkInterface::IsLoopBack) continue;
        combo_iface.addItem(iface.humanReadableName(), iface.name());
        if (!current_iface.isEmpty() && (iface.name() == current_iface)) {
            combo_iface.setCurrentIndex(i);
            interface_exists = true;
        }
        ++i;
    }
    // Saved interface does not exist, show it anyway
    if (!interface_exists) {
        combo_iface.addItem(session->networkInterfaceName(), current_iface);
        combo_iface.setCurrentIndex(i);
    }
    addRow(NETWORK_IFACE, tr("Network Interface (requires restart)"), &combo_iface);
    // Network interface address
    updateInterfaceAddressCombo();
    addRow(NETWORK_IFACE_ADDRESS, tr("Optional IP Address to bind to (requires restart)"), &combo_iface_address);
    // Listen on IPv6 address
    cb_listen_ipv6.setChecked(session->isIPv6Enabled());
    addRow(NETWORK_LISTEN_IPV6, tr("Listen on IPv6 address (requires restart)"), &cb_listen_ipv6);
    // Announce IP
    txtAnnounceIP.setText(session->announceIP());
    addRow(ANNOUNCE_IP, tr("IP Address to report to trackers (requires restart)"), &txtAnnounceIP);

    // Program notifications
    const MainWindow * const mainWindow = static_cast<Application*>(QCoreApplication::instance())->mainWindow();
    cb_program_notifications.setChecked(mainWindow->isNotificationsEnabled());
    addRow(PROGRAM_NOTIFICATIONS, tr("Display notifications"), &cb_program_notifications);
    // Torrent added notifications
    cb_torrent_added_notifications.setChecked(mainWindow->isTorrentAddedNotificationsEnabled());
    addRow(TORRENT_ADDED_NOTIFICATIONS, tr("Display notifications for added torrents"), &cb_torrent_added_notifications);
    // Download tracker's favicon
    cb_tracker_favicon.setChecked(mainWindow->isDownloadTrackerFavicon());
    addRow(DOWNLOAD_TRACKER_FAVICON, tr("Download tracker's favicon"), &cb_tracker_favicon);

    // Tracker State
    cb_tracker_status.setChecked(session->isTrackerEnabled());
    addRow(TRACKER_STATUS, tr("Enable embedded tracker"), &cb_tracker_status);
    // Tracker port
    spin_tracker_port.setMinimum(1);
    spin_tracker_port.setMaximum(65535);
    spin_tracker_port.setValue(pref->getTrackerPort());
    addRow(TRACKER_PORT, tr("Embedded tracker port"), &spin_tracker_port);
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    cb_update_check.setChecked(pref->isUpdateCheckEnabled());
    addRow(UPDATE_CHECK, tr("Check for software updates"), &cb_update_check);
#endif
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC))
    cb_use_icon_theme.setChecked(pref->useSystemIconTheme());
    addRow(USE_ICON_THEME, tr("Use system icon theme"), &cb_use_icon_theme);
#endif
    // Torrent recheck confirmation
    cb_confirm_torrent_recheck.setChecked(pref->confirmTorrentRecheck());
    addRow(CONFIRM_RECHECK_TORRENT, tr("Confirm torrent recheck"), &cb_confirm_torrent_recheck);
    // Announce to all trackers
    cb_announce_all_trackers.setChecked(session->announceToAllTrackers());
    addRow(ANNOUNCE_ALL_TRACKERS, tr("Always announce to all trackers"), &cb_announce_all_trackers);
}

template <typename T>
void AdvancedSettings::addRow(int row, const QString &rowText, T* widget)
{
    // ignore mouse wheel event
    static WheelEventEater filter;
    widget->installEventFilter(&filter);

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
