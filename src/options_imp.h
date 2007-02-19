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

#include <QTranslator>
#include "ui_options.h"
#include "libtorrent/session.hpp"
#include "libtorrent/ip_filter.hpp"

using namespace libtorrent;

class options_imp : public QDialog, private Ui::Dialog{
  Q_OBJECT

  private:
    QButtonGroup choiceLanguage;
    ip_filter filter;
    QStringList locales;

  public:
    // Contructor
    options_imp(QWidget *parent=0);

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
    // Language Settings
    QString getLocale() const;
    QTranslator translator;
    // Misc Settings
    bool useAdditionDialog() const;
    QString getSavePath() const;
    bool getClearFinishedOnExit() const;
    bool getGoToSysTrayOnMinimizingWindow() const;
    bool getConfirmOnExit() const;
    QString getPreviewProgram() const;
    bool getUseOSDAlways() const;
    bool getUseOSDWhenHiddenOnly() const;


  protected slots:
    void on_okButton_clicked();
    void on_cancelButton_clicked();
    void on_applyButton_clicked();
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
    void processFilterFile(const QString& filePath=QString());
    void enableApplyButton();
    void checkPortsLogic();
    void enableSavePath(int checkBoxValue);

  public slots:
    void setLocale(QString locale);

  signals:
    void status_changed(const QString&) const;
};

#endif
