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
#include "base/preferences.h"

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
    TRACKER_EXCHANGE,
    ANNOUNCE_ALL_TRACKERS,
    NETWORK_ADDRESS,

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
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    // Signals
    connect(&spin_cache, SIGNAL(valueChanged(int)), SLOT(updateCacheSpinSuffix(int)));
    // Load settings
    loadAdvancedSettings();
    resizeColumnToContents(0);
    horizontalHeader()->setStretchLastSection(true);
}

void AdvancedSettings::saveAdvancedSettings()
{
    Preferences* const pref = Preferences::instance();
    // Disk write cache
    pref->setDiskCacheSize(spin_cache.value());
    pref->setDiskCacheTTL(spin_cache_ttl.value());
    // Enable OS cache
    pref->setOsCache(cb_os_cache.isChecked());
    // Save resume data interval
    pref->setSaveResumeDataInterval(spin_save_resume_data_interval.value());
    // Outgoing ports
    pref->setOutgoingPortsMin(outgoing_ports_min.value());
    pref->setOutgoingPortsMax(outgoing_ports_max.value());
    // Recheck torrents on completion
    pref->recheckTorrentsOnCompletion(cb_recheck_completed.isChecked());
    // Transfer list refresh interval
    pref->setRefreshInterval(spin_list_refresh.value());
    // Peer resolution
    pref->resolvePeerCountries(cb_resolve_countries.isChecked());
    pref->resolvePeerHostNames(cb_resolve_hosts.isChecked());
    // Max Half-Open connections
    pref->setMaxHalfOpenConnections(spin_maxhalfopen.value());
    // Super seeding
    pref->enableSuperSeeding(cb_super_seeding.isChecked());
    // Network interface
    if (combo_iface.currentIndex() == 0) {
        // All interfaces (default)
        pref->setNetworkInterface(QString::null);
        pref->setNetworkInterfaceName(QString::null);
    }
    else {
        pref->setNetworkInterface(combo_iface.itemData(combo_iface.currentIndex()).toString());
        pref->setNetworkInterfaceName(combo_iface.currentText());
    }
    // Listen on IPv6 address
    pref->setListenIPv6(cb_listen_ipv6.isChecked());
    // Network address
    QHostAddress addr(txt_network_address.text().trimmed());
    if (addr.isNull())
        pref->setNetworkAddress("");
    else
        pref->setNetworkAddress(addr.toString());
    // Program notification
    pref->useProgramNotification(cb_program_notifications.isChecked());
    // Tracker
    pref->setTrackerEnabled(cb_tracker_status.isChecked());
    pref->setTrackerPort(spin_tracker_port.value());
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    pref->setUpdateCheckEnabled(cb_update_check.isChecked());
#endif
    // Icon theme
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC))
    pref->useSystemIconTheme(cb_use_icon_theme.isChecked());
#endif
    pref->setConfirmTorrentRecheck(cb_confirm_torrent_recheck.isChecked());
    // Tracker exchange
    pref->setTrackerExchangeEnabled(cb_enable_tracker_ext.isChecked());
    pref->setAnnounceToAllTrackers(cb_announce_all_trackers.isChecked());
}

void AdvancedSettings::updateCacheSpinSuffix(int value)
{
    if (value <= 0)
        spin_cache.setSuffix(tr(" (auto)"));
    else
        spin_cache.setSuffix(tr(" MiB"));
}

void AdvancedSettings::loadAdvancedSettings()
{
    const Preferences* const pref = Preferences::instance();
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
    spin_cache.setValue(pref->diskCacheSize());
    updateCacheSpinSuffix(spin_cache.value());
    addRow(DISK_CACHE, tr("Disk write cache size"), &spin_cache);
    // Disk cache expiry
    spin_cache_ttl.setMinimum(15);
    spin_cache_ttl.setMaximum(600);
    spin_cache_ttl.setValue(pref->diskCacheTTL());
    spin_cache_ttl.setSuffix(tr(" s", " seconds"));
    addRow(DISK_CACHE_TTL, tr("Disk cache expiry interval"), &spin_cache_ttl);
    // Enable OS cache
    cb_os_cache.setChecked(pref->osCache());
    addRow(OS_CACHE, tr("Enable OS cache"), &cb_os_cache);
    // Save resume data interval
    spin_save_resume_data_interval.setMinimum(1);
    spin_save_resume_data_interval.setMaximum(1440);
    spin_save_resume_data_interval.setValue(pref->saveResumeDataInterval());
    spin_save_resume_data_interval.setSuffix(tr(" m", " minutes"));
    addRow(SAVE_RESUME_DATA_INTERVAL, tr("Save resume data interval", "How often the fastresume file is saved."), &spin_save_resume_data_interval);
    // Outgoing port Min
    outgoing_ports_min.setMinimum(0);
    outgoing_ports_min.setMaximum(65535);
    outgoing_ports_min.setValue(pref->outgoingPortsMin());
    addRow(OUTGOING_PORT_MIN, tr("Outgoing ports (Min) [0: Disabled]"), &outgoing_ports_min);
    // Outgoing port Min
    outgoing_ports_max.setMinimum(0);
    outgoing_ports_max.setMaximum(65535);
    outgoing_ports_max.setValue(pref->outgoingPortsMax());
    addRow(OUTGOING_PORT_MAX, tr("Outgoing ports (Max) [0: Disabled]"), &outgoing_ports_max);
    // Recheck completed torrents
    cb_recheck_completed.setChecked(pref->recheckTorrentsOnCompletion());
    addRow(RECHECK_COMPLETED, tr("Recheck torrents on completion"), &cb_recheck_completed);
    // Transfer list refresh interval
    spin_list_refresh.setMinimum(30);
    spin_list_refresh.setMaximum(99999);
    spin_list_refresh.setValue(pref->getRefreshInterval());
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
    spin_maxhalfopen.setValue(pref->getMaxHalfOpenConnections());
    addRow(MAX_HALF_OPEN, tr("Maximum number of half-open connections [0: Unlimited]"), &spin_maxhalfopen);
    // Super seeding
    cb_super_seeding.setChecked(pref->isSuperSeedingEnabled());
    addRow(SUPER_SEEDING, tr("Strict super seeding"), &cb_super_seeding);
    // Network interface
    combo_iface.addItem(tr("Any interface", "i.e. Any network interface"));
    const QString current_iface = pref->getNetworkInterface();
    bool interface_exists = current_iface.isEmpty();
    int i = 1;
    foreach (const QNetworkInterface& iface, QNetworkInterface::allInterfaces()) {
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
        combo_iface.addItem(pref->getNetworkInterfaceName(), current_iface);
        combo_iface.setCurrentIndex(i);
    }
    addRow(NETWORK_IFACE, tr("Network Interface (requires restart)"), &combo_iface);
    // Listen on IPv6 address
    cb_listen_ipv6.setChecked(pref->getListenIPv6());
    addRow(NETWORK_LISTEN_IPV6, tr("Listen on IPv6 address (requires restart)"), &cb_listen_ipv6);
    // Network address
    txt_network_address.setText(pref->getNetworkAddress());
    addRow(NETWORK_ADDRESS, tr("IP Address to report to trackers (requires restart)"), &txt_network_address);
    // Program notifications
    cb_program_notifications.setChecked(pref->useProgramNotification());
    addRow(PROGRAM_NOTIFICATIONS, tr("Display program on-screen notifications"), &cb_program_notifications);
    // Tracker State
    cb_tracker_status.setChecked(pref->isTrackerEnabled());
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
    // Tracker exchange
    cb_enable_tracker_ext.setChecked(pref->trackerExchangeEnabled());
    addRow(TRACKER_EXCHANGE, tr("Exchange trackers with other peers"), &cb_enable_tracker_ext);
    // Announce to all trackers
    cb_announce_all_trackers.setChecked(pref->announceToAllTrackers());
    addRow(ANNOUNCE_ALL_TRACKERS, tr("Always announce to all trackers"), &cb_announce_all_trackers);
}

template <typename T>
void AdvancedSettings::addRow(int row, const QString &rowText, T* widget)
{
    setItem(row, PROPERTY, new QTableWidgetItem(rowText));
    setCellWidget(row, VALUE, widget);

    bool ok;
    if (std::is_same<T, QCheckBox>::value)
        ok = connect(widget, SIGNAL(stateChanged(int)), SIGNAL(settingsChanged()));
    else if (std::is_same<T, QSpinBox>::value)
        ok = connect(widget, SIGNAL(valueChanged(int)), SIGNAL(settingsChanged()));
    else if (std::is_same<T, QComboBox>::value)
        ok = connect(widget, SIGNAL(currentIndexChanged(int)), SIGNAL(settingsChanged()));
    else if (std::is_same<T, QLineEdit>::value)
        ok = connect(widget, SIGNAL(textChanged(QString)), SIGNAL(settingsChanged()));
    Q_ASSERT(ok);
}
