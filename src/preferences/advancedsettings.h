#ifndef ADVANCEDSETTINGS_H
#define ADVANCEDSETTINGS_H

#include <QTableWidget>
#include <QHeaderView>
#include <QSpinBox>
#include <QHostAddress>
#include <QCheckBox>
#include <QLineEdit>
#include <QComboBox>
#include <QNetworkInterface>
#include <libtorrent/version.hpp>
#include "preferences.h"

enum AdvSettingsCols {PROPERTY, VALUE};
enum AdvSettingsRows {DISK_CACHE, OUTGOING_PORT_MIN, OUTGOING_PORT_MAX, IGNORE_LIMIT_LAN, RECHECK_COMPLETED, LIST_REFRESH, RESOLVE_COUNTRIES, RESOLVE_HOSTS, MAX_HALF_OPEN, SUPER_SEEDING, NETWORK_IFACE, NETWORK_ADDRESS, PROGRAM_NOTIFICATIONS, TRACKER_STATUS, TRACKER_PORT,
                    #if defined(Q_WS_WIN) || defined(Q_WS_MAC)
                      UPDATE_CHECK,
                    #endif
                    #if defined(Q_WS_X11)
                      USE_ICON_THEME,
                    #endif
                      CONFIRM_DELETE_TORRENT, TRACKER_EXCHANGE,
                      ANNOUNCE_ALL_TRACKERS,
                      ROW_COUNT};

class AdvancedSettings: public QTableWidget {
  Q_OBJECT

private:
  QSpinBox spin_cache, outgoing_ports_min, outgoing_ports_max, spin_list_refresh, spin_maxhalfopen, spin_tracker_port;
  QCheckBox cb_ignore_limits_lan, cb_recheck_completed, cb_resolve_countries, cb_resolve_hosts,
  cb_super_seeding, cb_program_notifications, cb_tracker_status, cb_confirm_torrent_deletion,
  cb_enable_tracker_ext;
  QComboBox combo_iface;
#if defined(Q_WS_WIN) || defined(Q_WS_MAC)
  QCheckBox cb_update_check;
#endif
#if defined(Q_WS_X11)
  QCheckBox cb_use_icon_theme;
#endif
  QCheckBox cb_announce_all_trackers;
  QLineEdit txt_network_address;

public:
  AdvancedSettings(QWidget *parent=0): QTableWidget(parent) {
    // Set visual appearance
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setAlternatingRowColors(true);
    setColumnCount(2);
    QStringList header;
    header << tr("Setting") << tr("Value", "Value set for this setting");
    setHorizontalHeaderLabels(header);
    setColumnWidth(0, width()/2);
    horizontalHeader()->setStretchLastSection(true);
    verticalHeader()->setVisible(false);
    setRowCount(ROW_COUNT);
    // Signals
    connect(&spin_cache, SIGNAL(valueChanged(int)), SLOT(updateCacheSpinSuffix(int)));
    // Load settings
    loadAdvancedSettings();
  }

  ~AdvancedSettings() {
  }

public slots:
  void saveAdvancedSettings() {
    Preferences pref;
    // Disk write cache
    pref.setDiskCacheSize(spin_cache.value());
    // Outgoing ports
    pref.setOutgoingPortsMin(outgoing_ports_min.value());
    pref.setOutgoingPortsMax(outgoing_ports_max.value());
    // Ignore limits on LAN
    pref.ignoreLimitsOnLAN(cb_ignore_limits_lan.isChecked());
    // Recheck torrents on completion
    pref.recheckTorrentsOnCompletion(cb_recheck_completed.isChecked());
    // Transfer list refresh interval
    pref.setRefreshInterval(spin_list_refresh.value());
    // Peer resolution
    pref.resolvePeerCountries(cb_resolve_countries.isChecked());
    pref.resolvePeerHostNames(cb_resolve_hosts.isChecked());
    // Max Half-Open connections
    pref.setMaxHalfOpenConnections(spin_maxhalfopen.value());
    // Super seeding
    pref.enableSuperSeeding(cb_super_seeding.isChecked());
    // Network interface
    if (combo_iface.currentIndex() == 0) {
      // All interfaces (default)
      pref.setNetworkInterface(QString::null);
    } else {
      pref.setNetworkInterface(combo_iface.currentText());
    }
    // Network address
    QHostAddress addr(txt_network_address.text().trimmed());
    if (addr.isNull())
      pref.setNetworkAddress("");
    else
      pref.setNetworkAddress(addr.toString());
    // Program notification
    pref.useProgramNotification(cb_program_notifications.isChecked());
    // Tracker
    pref.setTrackerEnabled(cb_tracker_status.isChecked());
    pref.setTrackerPort(spin_tracker_port.value());
#if defined(Q_WS_WIN) || defined(Q_WS_MAC)
    pref.setUpdateCheckEnabled(cb_update_check.isChecked());
#endif
    // Icon theme
#if defined(Q_WS_X11)
    pref.useSystemIconTheme(cb_use_icon_theme.isChecked());
#endif
    pref.setConfirmTorrentDeletion(cb_confirm_torrent_deletion.isChecked());
    // Tracker exchange
    pref.setTrackerExchangeEnabled(cb_enable_tracker_ext.isChecked());
    pref.setAnnounceToAllTrackers(cb_announce_all_trackers.isChecked());
  }

signals:
  void settingsChanged();

private:
  void setRow(int row, const QString &property, QSpinBox* editor) {
    setItem(row, PROPERTY, new QTableWidgetItem(property));
    bool ok; Q_UNUSED(ok);
    ok = connect(editor, SIGNAL(valueChanged(int)), SIGNAL(settingsChanged()));
    Q_ASSERT(ok);
    setCellWidget(row, VALUE, editor);
  }

  void setRow(int row, const QString &property, QComboBox* editor) {
    setItem(row, PROPERTY, new QTableWidgetItem(property));
    bool ok; Q_UNUSED(ok);
    ok = connect(editor, SIGNAL(currentIndexChanged(int)), SIGNAL(settingsChanged()));
    Q_ASSERT(ok);
    setCellWidget(row, VALUE, editor);
  }

  void setRow(int row, const QString &property, QCheckBox* editor) {
    setItem(row, PROPERTY, new QTableWidgetItem(property));
    bool ok; Q_UNUSED(ok);
    ok = connect(editor, SIGNAL(stateChanged(int)), SIGNAL(settingsChanged()));
    Q_ASSERT(ok);
    setCellWidget(row, VALUE, editor);
  }

  void setRow(int row, const QString &property, QLineEdit* editor) {
    setItem(row, PROPERTY, new QTableWidgetItem(property));
    bool ok; Q_UNUSED(ok);
    ok = connect(editor, SIGNAL(textChanged(QString)), SIGNAL(settingsChanged()));
    Q_ASSERT(ok);
    setCellWidget(row, VALUE, editor);
  }

private slots:
  void updateCacheSpinSuffix(int value)
  {
    if (value <= 0)
      spin_cache.setSuffix(tr(" (auto)"));
    else
      spin_cache.setSuffix(tr(" MiB"));
  }

  void loadAdvancedSettings()
  {
    const Preferences pref;
    // Disk write cache
    spin_cache.setMinimum(0);
    spin_cache.setMaximum(2048);
    spin_cache.setValue(pref.diskCacheSize());
    updateCacheSpinSuffix(spin_cache.value());
    setRow(DISK_CACHE, tr("Disk write cache size"), &spin_cache);
    // Outgoing port Min
    outgoing_ports_min.setMinimum(0);
    outgoing_ports_min.setMaximum(65535);
    outgoing_ports_min.setValue(pref.outgoingPortsMin());
    setRow(OUTGOING_PORT_MIN, tr("Outgoing ports (Min) [0: Disabled]"), &outgoing_ports_min);
    // Outgoing port Min
    outgoing_ports_max.setMinimum(0);
    outgoing_ports_max.setMaximum(65535);
    outgoing_ports_max.setValue(pref.outgoingPortsMax());
    setRow(OUTGOING_PORT_MAX, tr("Outgoing ports (Max) [0: Disabled]"), &outgoing_ports_max);
    // Ignore transfer limits on local network
    cb_ignore_limits_lan.setChecked(pref.ignoreLimitsOnLAN());
    setRow(IGNORE_LIMIT_LAN, tr("Ignore transfer limits on local network"), &cb_ignore_limits_lan);
    // Recheck completed torrents
    cb_recheck_completed.setChecked(pref.recheckTorrentsOnCompletion());
    setRow(RECHECK_COMPLETED, tr("Recheck torrents on completion"), &cb_recheck_completed);
    // Transfer list refresh interval
    spin_list_refresh.setMinimum(30);
    spin_list_refresh.setMaximum(99999);
    spin_list_refresh.setValue(pref.getRefreshInterval());
    spin_list_refresh.setSuffix(tr(" ms", " milliseconds"));
    setRow(LIST_REFRESH, tr("Transfer list refresh interval"), &spin_list_refresh);
    // Resolve Peer countries
    cb_resolve_countries.setChecked(pref.resolvePeerCountries());
    setRow(RESOLVE_COUNTRIES, tr("Resolve peer countries (GeoIP)"), &cb_resolve_countries);
    // Resolve peer hosts
    cb_resolve_hosts.setChecked(pref.resolvePeerHostNames());
    setRow(RESOLVE_HOSTS, tr("Resolve peer host names"), &cb_resolve_hosts);
    // Max Half Open connections
    spin_maxhalfopen.setMinimum(0);
    spin_maxhalfopen.setMaximum(99999);
    spin_maxhalfopen.setValue(pref.getMaxHalfOpenConnections());
    setRow(MAX_HALF_OPEN, tr("Maximum number of half-open connections [0: Disabled]"), &spin_maxhalfopen);
    // Super seeding
    cb_super_seeding.setChecked(pref.isSuperSeedingEnabled());
    setRow(SUPER_SEEDING, tr("Strict super seeding"), &cb_super_seeding);
    // Network interface
    combo_iface.addItem(tr("Any interface", "i.e. Any network interface"));
    const QString current_iface = pref.getNetworkInterface();
    bool interface_exists = current_iface.isEmpty();
    int i = 1;
    foreach (const QNetworkInterface& iface, QNetworkInterface::allInterfaces()) {
      if (iface.flags() & QNetworkInterface::IsLoopBack) continue;
      combo_iface.addItem(iface.name());
      if (!current_iface.isEmpty() && iface.name() == current_iface) {
        combo_iface.setCurrentIndex(i);
        interface_exists = true;
      }
      ++i;
    }
    // Saved interface does not exist, show it anyway
    if (!interface_exists) {
      combo_iface.addItem(current_iface);
      combo_iface.setCurrentIndex(i);
    }
    setRow(NETWORK_IFACE, tr("Network Interface (requires restart)"), &combo_iface);
    // Network address
    txt_network_address.setText(pref.getNetworkAddress());
    setRow(NETWORK_ADDRESS, tr("IP Address to report to trackers (requires restart)"), &txt_network_address);
    // Program notifications
    cb_program_notifications.setChecked(pref.useProgramNotification());
    setRow(PROGRAM_NOTIFICATIONS, tr("Display program on-screen notifications"), &cb_program_notifications);
    // Tracker State
    cb_tracker_status.setChecked(pref.isTrackerEnabled());
    setRow(TRACKER_STATUS, tr("Enable embedded tracker"), &cb_tracker_status);
    // Tracker port
    spin_tracker_port.setMinimum(1);
    spin_tracker_port.setMaximum(65535);
    spin_tracker_port.setValue(pref.getTrackerPort());
    setRow(TRACKER_PORT, tr("Embedded tracker port"), &spin_tracker_port);
#if defined(Q_WS_WIN) || defined(Q_WS_MAC)
    cb_update_check.setChecked(pref.isUpdateCheckEnabled());
    setRow(UPDATE_CHECK, tr("Check for software updates"), &cb_update_check);
#endif
#if defined(Q_WS_X11)
    cb_use_icon_theme.setChecked(pref.useSystemIconTheme());
    setRow(USE_ICON_THEME, tr("Use system icon theme"), &cb_use_icon_theme);
#endif
    // Torrent deletion confirmation
    cb_confirm_torrent_deletion.setChecked(pref.confirmTorrentDeletion());
    setRow(CONFIRM_DELETE_TORRENT, tr("Confirm torrent deletion"), &cb_confirm_torrent_deletion);
    // Tracker exchange
    cb_enable_tracker_ext.setChecked(pref.trackerExchangeEnabled());
    setRow(TRACKER_EXCHANGE, tr("Exchange trackers with other peers"), &cb_enable_tracker_ext);
    // Announce to all trackers
    cb_announce_all_trackers.setChecked(pref.announceToAllTrackers());
    setRow(ANNOUNCE_ALL_TRACKERS, tr("Always announce to all trackers"), &cb_announce_all_trackers);
  }

};

#endif // ADVANCEDSETTINGS_H
