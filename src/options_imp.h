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

#define HTTP 0
#define SOCKS5 1
#define HTTP_PW 2
#define SOCKS5_PW 3

using namespace libtorrent;

class QCloseEvent;

class options_imp : public QDialog, private Ui::Dialog{
  Q_OBJECT

  private:
    QButtonGroup choiceLanguage;
    ip_filter filter;
    QStringList locales;
    QAbstractButton *applyButton;

  public:
    // Contructor / Destructor
    options_imp(QWidget *parent=0);
    ~options_imp();

    // Methods
    void saveOptions();
    void loadOptions();
    // Main options
    std::pair<unsigned short, unsigned short> getPorts() const;
    QPair<int,int> getLimits() const;
    float getRatio() const;
    int getMaxConnec() const;
    QString getScanDir() const;
    bool isDHTEnabled() const;
    int getDHTPort() const;
    int getEncryptionSetting() const;
    bool isPeXDisabled() const;
    // Filter Settings
    bool isFilteringEnabled() const;
    ip_filter getFilter() const;
    // Proxy settings
    bool isProxyEnabled() const;
    bool isProxyAuthEnabled() const;
    QString getProxyIp() const;
    unsigned short getProxyPort() const;
    QString getProxyUsername() const;
    QString getProxyPassword() const;
    unsigned short getProxyType() const;
    bool useProxyForTrackers() const;
    bool useProxyForPeers() const;
    bool useProxyForWebseeds() const;
    bool useProxyForDHT() const;
    // Language Settings
    QString getLocale() const;
    // Misc Settings
    bool useAdditionDialog() const;
    QString getSavePath() const;
    bool getGoToSysTrayOnMinimizingWindow() const;
    bool getGoToSysTrayOnExitingWindow() const;
    bool getConfirmOnExit() const;
    QString getPreviewProgram() const;
    bool getUseOSDAlways() const;
    bool getUseOSDWhenHiddenOnly() const;
    QString getStyle() const;
    bool useSystrayIntegration() const;

  protected slots:
    void on_buttonBox_accepted();
    void closeEvent(QCloseEvent *e);
    void on_buttonBox_rejected();
    void applySettings(QAbstractButton* button);
    void on_addFilterRange_clicked();
    void on_delFilterRange_clicked();
    void on_browse_button_scan_clicked();
    void on_browsePreview_clicked();
    void on_filterBrowse_clicked();
    void disableDownload(int checkBoxValue);
    void disableDHTGroup(int checkBoxValue);
    void disableMaxConnecLimit(int);
    void enableFilter(int checkBoxValue);
    void disableUpload(int checkBoxValue);
    void disableShareRatio(int checkBoxValue);
    void enableProxy(int checkBoxValue);
    void enableProxyAuth(int checkBoxValue);
    void enableDirScan(int checkBoxValue);
    void on_browse_button_clicked();
    void processFilterFile(QString filePath=QString());
    void enableApplyButton();
    void checkPortsLogic();
    void enableSavePath(int checkBoxValue);
    void setStyle(QString style);
    void systrayDisabled(int val);

  public slots:
    void setLocale(QString locale);
    void useStyle();

  signals:
    void status_changed(QString, bool) const;
    void exitWithCancel();
};

#endif
