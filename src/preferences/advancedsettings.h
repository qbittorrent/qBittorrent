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
enum AdvSettingsRows {DISK_CACHE, OUTGOING_PORT_MIN, OUTGOING_PORT_MAX, IGNORE_LIMIT_LAN, COUNT_OVERHEAD, RECHECK_COMPLETED, LIST_REFRESH, RESOLVE_COUNTRIES, RESOLVE_HOSTS, MAX_HALF_OPEN, SUPER_SEEDING, NETWORK_IFACE, NETWORK_ADDRESS, PROGRAM_NOTIFICATIONS, TRACKER_STATUS, TRACKER_PORT,
                    #if defined(Q_WS_WIN) || defined(Q_WS_MAC)
                      UPDATE_CHECK,
                    #endif
                    #if defined(Q_WS_X11) && (QT_VERSION >= QT_VERSION_CHECK(4,6,0))
                      USE_ICON_THEME,
                    #endif
                      CONFIRM_DELETE_TORRENT,
                      ROW_COUNT};

class AdvancedSettings: public QTableWidget {
  Q_OBJECT

private:
  QSpinBox *spin_cache, *outgoing_ports_min, *outgoing_ports_max, *spin_list_refresh, *spin_maxhalfopen, *spin_tracker_port;
  QCheckBox *cb_ignore_limits_lan, *cb_count_overhead, *cb_recheck_completed, *cb_resolve_countries, *cb_resolve_hosts, *cb_super_seeding, *cb_program_notifications, *cb_tracker_status, *cb_confirm_torrent_deletion;
  QComboBox *combo_iface;
#if defined(Q_WS_WIN) || defined(Q_WS_MAC)
  QCheckBox *cb_update_check;
#endif
#if defined(Q_WS_X11) && (QT_VERSION >= QT_VERSION_CHECK(4,6,0))
  QCheckBox *cb_use_icon_theme;
#endif
  QLineEdit *txt_network_address;

public:
  AdvancedSettings(QWidget *parent=0): QTableWidget(parent) {
    // Set visual appearance
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setAlternatingRowColors(true);
    setColumnCount(2);
    QStringList header;
    header << tr("Property") << tr("Value");
    setHorizontalHeaderLabels(header);
    setColumnWidth(0, width()/2);
    horizontalHeader()->setStretchLastSection(true);
    verticalHeader()->setVisible(false);
    setRowCount(ROW_COUNT);
    // Load settings
    loadAdvancedSettings();
  }

  ~AdvancedSettings() {
    delete spin_cache;
    delete outgoing_ports_min;
    delete outgoing_ports_max;
    delete cb_ignore_limits_lan;
    delete cb_count_overhead;
    delete cb_recheck_completed;
    delete spin_list_refresh;
    delete cb_resolve_countries;
    delete cb_resolve_hosts;
    delete spin_maxhalfopen;
    delete cb_super_seeding;
    delete combo_iface;
    delete cb_program_notifications;
    delete spin_tracker_port;
    delete cb_tracker_status;
    delete cb_confirm_torrent_deletion;
#if defined(Q_WS_WIN) || defined(Q_WS_MAC)
    delete cb_update_check;
#endif
#if defined(Q_WS_X11) && (QT_VERSION >= QT_VERSION_CHECK(4,6,0))
    delete cb_use_icon_theme;
#endif
    delete txt_network_address;
  }

public slots:
  void saveAdvancedSettings() {
    Preferences pref;
    // Disk write cache
    pref.setDiskCacheSize(spin_cache->value());
    // Outgoing ports
    pref.setOutgoingPortsMin(outgoing_ports_min->value());
    pref.setOutgoingPortsMax(outgoing_ports_max->value());
    // Ignore limits on LAN
    pref.ignoreLimitsOnLAN(cb_ignore_limits_lan->isChecked());
    // Include protocol overhead in transfer limits
    pref.includeOverheadInLimits(cb_count_overhead->isChecked());
    // Recheck torrents on completion
    pref.recheckTorrentsOnCompletion(cb_recheck_completed->isChecked());
    // Transfer list refresh interval
    pref.setRefreshInterval(spin_list_refresh->value());
    // Peer resolution
    pref.resolvePeerCountries(cb_resolve_countries->isChecked());
    pref.resolvePeerHostNames(cb_resolve_hosts->isChecked());
    // Max Half-Open connections
    pref.setMaxHalfOpenConnections(spin_maxhalfopen->value());
#if LIBTORRENT_VERSION_MINOR > 14
    // Super seeding
    pref.enableSuperSeeding(cb_super_seeding->isChecked());
#endif
    // Network interface
    if(combo_iface->currentIndex() == 0) {
      // All interfaces (default)
      pref.setNetworkInterface(QString::null);
    } else {
      pref.setNetworkInterface(combo_iface->currentText());
    }
    // Network address
    QHostAddress addr(txt_network_address->text().trimmed());
    if(addr.isNull())
      pref.setNetworkAddress("");
    else
      pref.setNetworkAddress(addr.toString());
    // Program notification
    pref.useProgramNotification(cb_program_notifications->isChecked());
    // Tracker
    pref.setTrackerEnabled(cb_tracker_status->isChecked());
    pref.setTrackerPort(spin_tracker_port->value());
#if defined(Q_WS_WIN) || defined(Q_WS_MAC)
    pref.setUpdateCheckEnabled(cb_update_check->isChecked());
#endif
    // Icon theme
#if defined(Q_WS_X11) && (QT_VERSION >= QT_VERSION_CHECK(4,6,0))
    pref.useSystemIconTheme(cb_use_icon_theme->isChecked());
#endif
    pref.setConfirmTorrentDeletion(cb_confirm_torrent_deletion->isChecked());
  }

protected slots:
  void loadAdvancedSettings() {
    const Preferences pref;
    // Disk write cache
    setItem(DISK_CACHE, PROPERTY, new QTableWidgetItem(tr("Disk write cache size")));
    spin_cache = new QSpinBox();
    connect(spin_cache, SIGNAL(valueChanged(int)), this, SLOT(emitSettingsChanged()));
    spin_cache->setMinimum(1);
    spin_cache->setMaximum(200);
    spin_cache->setValue(pref.diskCacheSize());
    spin_cache->setSuffix(tr(" MiB"));
    setCellWidget(DISK_CACHE, VALUE, spin_cache);
    // Outgoing port Min
    setItem(OUTGOING_PORT_MIN, PROPERTY, new QTableWidgetItem(tr("Outgoing ports (Min) [0: Disabled]")));
    outgoing_ports_min = new QSpinBox();
    connect(outgoing_ports_min, SIGNAL(valueChanged(int)), this, SLOT(emitSettingsChanged()));
    outgoing_ports_min->setMinimum(0);
    outgoing_ports_min->setMaximum(65535);
    outgoing_ports_min->setValue(pref.outgoingPortsMin());
    setCellWidget(OUTGOING_PORT_MIN, VALUE, outgoing_ports_min);
    // Outgoing port Min
    setItem(OUTGOING_PORT_MAX, PROPERTY, new QTableWidgetItem(tr("Outgoing ports (Max) [0: Disabled]")));
    outgoing_ports_max = new QSpinBox();
    connect(outgoing_ports_max, SIGNAL(valueChanged(int)), this, SLOT(emitSettingsChanged()));
    outgoing_ports_max->setMinimum(0);
    outgoing_ports_max->setMaximum(65535);
    outgoing_ports_max->setValue(pref.outgoingPortsMax());
    setCellWidget(OUTGOING_PORT_MAX, VALUE, outgoing_ports_max);
    // Ignore transfer limits on local network
    setItem(IGNORE_LIMIT_LAN, PROPERTY, new QTableWidgetItem(tr("Ignore transfer limits on local network")));
    cb_ignore_limits_lan = new QCheckBox();
    connect(cb_ignore_limits_lan, SIGNAL(toggled(bool)), this, SLOT(emitSettingsChanged()));
    cb_ignore_limits_lan->setChecked(pref.ignoreLimitsOnLAN());
    setCellWidget(IGNORE_LIMIT_LAN, VALUE, cb_ignore_limits_lan);
    // Consider protocol overhead in transfer limits
    setItem(COUNT_OVERHEAD, PROPERTY, new QTableWidgetItem(tr("Include TCP/IP overhead in transfer limits")));
    cb_count_overhead = new QCheckBox();
    connect(cb_count_overhead, SIGNAL(toggled(bool)), this, SLOT(emitSettingsChanged()));
    cb_count_overhead->setChecked(pref.includeOverheadInLimits());
    setCellWidget(COUNT_OVERHEAD, VALUE, cb_count_overhead);
    // Recheck completed torrents
    setItem(RECHECK_COMPLETED, PROPERTY, new QTableWidgetItem(tr("Recheck torrents on completion")));
    cb_recheck_completed = new QCheckBox();
    connect(cb_recheck_completed, SIGNAL(toggled(bool)), this, SLOT(emitSettingsChanged()));
    cb_recheck_completed->setChecked(pref.recheckTorrentsOnCompletion());
    setCellWidget(RECHECK_COMPLETED, VALUE, cb_recheck_completed);
    // Transfer list refresh interval
    setItem(LIST_REFRESH, PROPERTY, new QTableWidgetItem(tr("Transfer list refresh interval")));
    spin_list_refresh = new QSpinBox();
    connect(spin_list_refresh, SIGNAL(valueChanged(int)), this, SLOT(emitSettingsChanged()));
    spin_list_refresh->setMinimum(30);
    spin_list_refresh->setMaximum(99999);
    spin_list_refresh->setValue(pref.getRefreshInterval());
    spin_list_refresh->setSuffix(tr(" ms", " milliseconds"));
    setCellWidget(LIST_REFRESH, VALUE, spin_list_refresh);
    // Resolve Peer countries
    setItem(RESOLVE_COUNTRIES, PROPERTY, new QTableWidgetItem(tr("Resolve peer countries (GeoIP)")));
    cb_resolve_countries = new QCheckBox();
    connect(cb_resolve_countries, SIGNAL(toggled(bool)), this, SLOT(emitSettingsChanged()));
    cb_resolve_countries->setChecked(pref.resolvePeerCountries());
    setCellWidget(RESOLVE_COUNTRIES, VALUE, cb_resolve_countries);
    // Resolve peer hosts
    setItem(RESOLVE_HOSTS, PROPERTY, new QTableWidgetItem(tr("Resolve peer host names")));
    cb_resolve_hosts = new QCheckBox();
    connect(cb_resolve_hosts, SIGNAL(toggled(bool)), this, SLOT(emitSettingsChanged()));
    cb_resolve_hosts->setChecked(pref.resolvePeerHostNames());
    setCellWidget(RESOLVE_HOSTS, VALUE, cb_resolve_hosts);
    // Max Half Open connections
    setItem(MAX_HALF_OPEN, PROPERTY, new QTableWidgetItem(tr("Maximum number of half-open connections [0: Disabled]")));
    spin_maxhalfopen = new QSpinBox();
    connect(spin_maxhalfopen, SIGNAL(valueChanged(int)), this, SLOT(emitSettingsChanged()));
    spin_maxhalfopen->setMinimum(0);
    spin_maxhalfopen->setMaximum(99999);
    spin_maxhalfopen->setValue(pref.getMaxHalfOpenConnections());
    setCellWidget(MAX_HALF_OPEN, VALUE, spin_maxhalfopen);
    // Super seeding
    setItem(SUPER_SEEDING, PROPERTY, new QTableWidgetItem(tr("Strict super seeding")));
    cb_super_seeding = new QCheckBox();
    connect(cb_super_seeding, SIGNAL(toggled(bool)), this, SLOT(emitSettingsChanged()));
#if LIBTORRENT_VERSION_MINOR > 14
    cb_super_seeding->setChecked(pref.isSuperSeedingEnabled());
#else
    cb_super_seeding->setEnabled(false);
#endif
    setCellWidget(SUPER_SEEDING, VALUE, cb_super_seeding);
    // Network interface
    setItem(NETWORK_IFACE, PROPERTY, new QTableWidgetItem(tr("Network Interface (requires restart)")));
    combo_iface = new QComboBox;
    combo_iface->addItem(tr("Any interface", "i.e. Any network interface"));
    const QString current_iface = pref.getNetworkInterface();
    int i = 1;
    foreach(const QNetworkInterface& iface, QNetworkInterface::allInterfaces()) {
      if(iface.name() == "lo") continue;
      combo_iface->addItem(iface.name());
      if(!current_iface.isEmpty() && iface.name() == current_iface)
        combo_iface->setCurrentIndex(i);
      ++i;
    }
    connect(combo_iface, SIGNAL(currentIndexChanged(int)), this, SLOT(emitSettingsChanged()));
    setCellWidget(NETWORK_IFACE, VALUE, combo_iface);
    // Network address
    setItem(NETWORK_ADDRESS, PROPERTY, new QTableWidgetItem(tr("IP Address to report to trackers (requires restart)")));
    txt_network_address = new QLineEdit;
    txt_network_address->setText(pref.getNetworkAddress());
    connect(txt_network_address, SIGNAL(textChanged(QString)), this, SLOT(emitSettingsChanged()));
    setCellWidget(NETWORK_ADDRESS, VALUE, txt_network_address);
    // Program notifications
    setItem(PROGRAM_NOTIFICATIONS, PROPERTY, new QTableWidgetItem(tr("Display program on-screen notifications")));
    cb_program_notifications = new QCheckBox();
    connect(cb_program_notifications, SIGNAL(toggled(bool)), this, SLOT(emitSettingsChanged()));
    cb_program_notifications->setChecked(pref.useProgramNotification());
    setCellWidget(PROGRAM_NOTIFICATIONS, VALUE, cb_program_notifications);
    // Tracker State
    setItem(TRACKER_STATUS, PROPERTY, new QTableWidgetItem(tr("Enable embedded tracker")));
    cb_tracker_status = new QCheckBox();
    connect(cb_tracker_status, SIGNAL(toggled(bool)), this, SLOT(emitSettingsChanged()));
    cb_tracker_status->setChecked(pref.isTrackerEnabled());
    setCellWidget(TRACKER_STATUS, VALUE, cb_tracker_status);
    // Tracker port
    setItem(TRACKER_PORT, PROPERTY, new QTableWidgetItem(tr("Embedded tracker port")));
    spin_tracker_port = new QSpinBox();
    connect(spin_tracker_port, SIGNAL(valueChanged(int)), this, SLOT(emitSettingsChanged()));
    spin_tracker_port->setMinimum(1);
    spin_tracker_port->setMaximum(65535);
    spin_tracker_port->setValue(pref.getTrackerPort());
    setCellWidget(TRACKER_PORT, VALUE, spin_tracker_port);
#if defined(Q_WS_WIN) || defined(Q_WS_MAC)
    setItem(UPDATE_CHECK, PROPERTY, new QTableWidgetItem(tr("Check for software updates")));
    cb_update_check = new QCheckBox();
    connect(cb_update_check, SIGNAL(toggled(bool)), this, SLOT(emitSettingsChanged()));
    cb_update_check->setChecked(pref.isUpdateCheckEnabled());
    setCellWidget(UPDATE_CHECK, VALUE, cb_update_check);
#endif
#if defined(Q_WS_X11) && (QT_VERSION >= QT_VERSION_CHECK(4,6,0))
    setItem(USE_ICON_THEME, PROPERTY, new QTableWidgetItem(tr("Use system icon theme")));
    cb_use_icon_theme = new QCheckBox();
    connect(cb_use_icon_theme, SIGNAL(toggled(bool)), this, SLOT(emitSettingsChanged()));
    cb_use_icon_theme->setChecked(pref.useSystemIconTheme());
    setCellWidget(USE_ICON_THEME, VALUE, cb_use_icon_theme);
#endif
    // Torrent deletion confirmation
    setItem(CONFIRM_DELETE_TORRENT, PROPERTY, new QTableWidgetItem(tr("Confirm torrent deletion")));
    cb_confirm_torrent_deletion = new QCheckBox();
    connect(cb_confirm_torrent_deletion, SIGNAL(toggled(bool)), this, SLOT(emitSettingsChanged()));
    cb_confirm_torrent_deletion->setChecked(pref.confirmTorrentDeletion());
    setCellWidget(CONFIRM_DELETE_TORRENT, VALUE, cb_confirm_torrent_deletion);
  }

  void emitSettingsChanged() {
    emit settingsChanged();
  }

signals:
  void settingsChanged();
};

#endif // ADVANCEDSETTINGS_H
