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
 * Contact : chris@qbittorrent.org
 */

#ifndef OPTIONS_IMP_H
#define OPTIONS_IMP_H

#include "ui_options.h"
#include <libtorrent/ip_filter.hpp>

#define HTTP 1
#define SOCKS5 2
#define HTTP_PW 3
#define SOCKS5_PW 4

// actions on double-click on torrents
#define TOGGLE_PAUSE 0
#define OPEN_DEST 1
#define SHOW_PROPERTIES 2

using namespace libtorrent;

class QCloseEvent;

class options_imp : public QDialog, private Ui::Dialog {
  Q_OBJECT

  private:
    QButtonGroup choiceLanguage;
    QStringList locales;
    QAbstractButton *applyButton;

  public:
    // Contructor / Destructor
    options_imp(QWidget *parent=0);
    ~options_imp();

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
    bool OSDEnabled() const;
    bool isToolbarDisplayed() const;
    // Downloads
    QString getSavePath() const;
    bool preAllocateAllFiles() const;
    bool useAdditionDialog() const;
    bool addTorrentsInPause() const;
    bool isDirScanEnabled() const;
    QString getScanDir() const;
    int getActionOnDblClOnTorrentDl() const;
    int getActionOnDblClOnTorrentFn() const;
    // Connection options
    std::pair<unsigned short, unsigned short> getPorts() const;
    bool isUPnPEnabled() const;
    bool isNATPMPEnabled() const;
    QPair<int,int> getGlobalBandwidthLimits() const;
    // Bittorrent options
    int getMaxConnecs() const;
    int getMaxConnecsPerTorrent() const;
    int getMaxUploadsPerTorrent() const;
    bool isDHTEnabled() const;
    bool isPeXEnabled() const;
    bool isLSDEnabled() const;
    bool isRSSEnabled() const;
    bool shouldSpoofAzureus() const;
    int getEncryptionSetting() const;
    float getDesiredRatio() const;
    float getDeleteRatio() const;
    // Proxy options
    bool isHTTPProxyEnabled() const;
    bool isHTTPProxyAuthEnabled() const;
    QString getHTTPProxyIp() const;
    unsigned short getHTTPProxyPort() const;
    QString getHTTPProxyUsername() const;
    QString getHTTPProxyPassword() const;
    int getHTTPProxyType() const;
    bool isProxyEnabled() const;
    bool isProxyAuthEnabled() const;
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
    int getMaxActiveTorrents() const;
    bool isWebUiEnabled() const;
    quint16 webUiPort() const;
    QString webUiUsername() const;
    QString webUiPassword() const;

  protected slots:
    void enableUploadLimit(int checkBoxValue);
    void enableDownloadLimit(int checkBoxValue);
    void enableDirScan(int checkBoxValue);
    void enableProxy(int comboIndex);
    void enableProxyAuth(int checkBoxValue);
    void enableProxyHTTP(int comboIndex);
    void enableProxyAuthHTTP(int checkBoxValue);
    void enableMaxConnecsLimit(int);
    void enableMaxConnecsLimitPerTorrent(int checkBoxValue);
    void enableMaxUploadsLimitPerTorrent(int checkBoxValue);
    void enableShareRatio(int checkBoxValue);
    void enableDeleteRatio(int checkBoxValue);
    void enableFilter(int checkBoxValue);
    void enableRSS(int checkBoxValue);
    void enableQueueingSystem(int checkBoxValue);
    void setStyle(int style);
    void on_buttonBox_accepted();
    void closeEvent(QCloseEvent *e);
    void on_buttonBox_rejected();
    void applySettings(QAbstractButton* button);
    void on_browseScanDirButton_clicked();
    void on_browseFilterButton_clicked();
    void on_browseSaveDirButton_clicked();
    void enableApplyButton();
    void checkPortsLogic();
    void enableSystrayOptions();
    void disableSystrayOptions();
    void setSystrayOptionsState(int checkBoxValue);
    void enableWebUi(bool checkBoxValue);

  public slots:
    void setLocale(QString locale);
    void useStyle();

  signals:
    void status_changed(bool) const;
    void exitWithCancel();
};

#endif
