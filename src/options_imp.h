/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez
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
 *
 * Contact : chris@qbittorrent.org
 */

#ifndef OPTIONS_IMP_H
#define OPTIONS_IMP_H

#include "ui_options.h"
#include <libtorrent/ip_filter.hpp>

enum ProxyType {HTTP=1, SOCKS5=2, HTTP_PW=3, SOCKS5_PW=4};

// actions on double-click on torrents
enum DoubleClickAction {TOGGLE_PAUSE, OPEN_DEST};

using namespace libtorrent;

class QCloseEvent;

class options_imp : public QDialog, private Ui_Preferences {
  Q_OBJECT

private:
  QButtonGroup choiceLanguage;
  QStringList locales;
  QAbstractButton *applyButton;

public:
  // Contructor / Destructor
  options_imp(QWidget *parent=0);
  ~options_imp();
  QSize sizeFittingScreen();

protected:
  // Methods
  void saveOptions();
  void loadOptions();
  // General options
  QString getLocale() const;
  int getStyle() const;
  bool confirmOnExit() const;
  bool speedInTitleBar() const;
  unsigned int getRefreshInterval() const;
  bool systrayIntegration() const;
  bool minimizeToTray() const;
  bool closeToTray() const;
  bool startMinimized() const;
  bool isSlashScreenDisabled() const;
  bool OSDEnabled() const;
  bool isToolbarDisplayed() const;
  // Downloads
  QString getSavePath() const;
  bool isTempPathEnabled() const;
  QString getTempPath() const;
  bool preAllocateAllFiles() const;
  bool useAdditionDialog() const;
  bool addTorrentsInPause() const;
  bool isDirScanEnabled() const;
  QString getScanDir() const;
  int getActionOnDblClOnTorrentDl() const;
  int getActionOnDblClOnTorrentFn() const;
  // Connection options
  int getPort() const;
  bool isUPnPEnabled() const;
  bool isNATPMPEnabled() const;
  QPair<int,int> getGlobalBandwidthLimits() const;
  // Bittorrent options
  int getMaxConnecs() const;
  int getMaxConnecsPerTorrent() const;
  int getMaxUploadsPerTorrent() const;
  bool isDHTEnabled() const;
  bool isDHTPortSameAsBT() const;
  int getDHTPort() const;
  bool isLSDEnabled() const;
  bool isRSSEnabled() const;
  bool isUtorrentSpoofingEnabled() const;
  int getEncryptionSetting() const;
  float getDesiredRatio() const;
  float getDeleteRatio() const;
  // Proxy options
  QString getHTTPProxyIp() const;
  unsigned short getHTTPProxyPort() const;
  QString getHTTPProxyUsername() const;
  QString getHTTPProxyPassword() const;
  int getHTTPProxyType() const;
  bool isProxyEnabled() const;
  bool isHTTPProxyEnabled() const;
  bool isProxyAuthEnabled() const;
  bool isHTTPProxyAuthEnabled() const;
  QString getProxyIp() const;
  unsigned short getProxyPort() const;
  QString getProxyUsername() const;
  QString getProxyPassword() const;
  int getProxyType() const;
  bool useProxyForTrackers() const;
  bool useProxyForPeers() const;
  bool useProxyForWebseeds() const;
  bool useProxyForDHT() const;
  // IP Filter
  bool isFilteringEnabled() const;
  QString getFilter() const;
  // Queueing system
  bool isQueueingSystemEnabled() const;
  int getMaxActiveDownloads() const;
  int getMaxActiveUploads() const;
  int getMaxActiveTorrents() const;
  bool isWebUiEnabled() const;
  quint16 webUiPort() const;
  QString webUiUsername() const;
  QString webUiPassword() const;

protected slots:
  void enableUploadLimit(bool checked);
  void enableDownloadLimit(bool checked);
  void enableTempPathInput(bool checked);
  void enableDirScan(bool checked);
  void enableProxy(int comboIndex);
  void enableProxyAuth(bool checked);
  void enableProxyHTTP(int comboIndex);
  void enableProxyAuthHTTP(bool checked);
  void enableMaxConnecsLimit(bool checked);
  void enableMaxConnecsLimitPerTorrent(bool checked);
  void enableMaxUploadsLimitPerTorrent(bool checked);
  void enableShareRatio(bool checked);
  void enableDeleteRatio(bool checked);
  void enableFilter(bool checked);
  void enableRSS(bool checked);
  void enableDHTSettings(bool checked);
  void enableDHTPortSettings(bool checked);
  void enableQueueingSystem(bool checked);
  void setStyle(int style);
  void on_buttonBox_accepted();
  void closeEvent(QCloseEvent *e);
  void on_buttonBox_rejected();
  void applySettings(QAbstractButton* button);
  void on_browseScanDirButton_clicked();
  void on_browseFilterButton_clicked();
  void on_browseSaveDirButton_clicked();
  void on_browseTempDirButton_clicked();
  void enableApplyButton();
  void enableSystrayOptions();
  void disableSystrayOptions();
  void setSystrayOptionsState(bool checked);
  void enableWebUi(bool checkBoxValue);
  void changePage(QListWidgetItem*, QListWidgetItem*);
  void loadWindowState();
  void saveWindowState() const;
  void on_randomButton_clicked();

public slots:
  void setLocale(QString locale);
  void useStyle();

signals:
  void status_changed() const;
  void exitWithCancel();
};

#endif
