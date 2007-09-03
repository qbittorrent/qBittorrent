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

#ifndef ENGINE_SELECT_DLG_H
#define ENGINE_SELECT_DLG_H

#include "ui_engineSelect.h"

class downloadThread;
class QDropEvent;

class engineSelectDlg : public QDialog, public Ui::engineSelect{
  Q_OBJECT

  private:
    // Search related
    QHash<QString, bool> installed_engines;
    downloadThread *downloader;

  public:
    engineSelectDlg(QWidget *parent);
    ~engineSelectDlg();
    QList<QTreeWidgetItem*> findItemsWithUrl(QString url);

  protected:
    bool parseVersionsFile(QString versions_file, QString updateServer);
    bool isUpdateNeeded(QString plugin_name, float new_version) const;
    bool checkInstalled(QString plugin_name) const;

  signals:
    void enginesChanged();

  protected slots:
    void saveSettings();
    void on_closeButton_clicked();
    void loadSupportedSearchEngines(bool first=false);
    void toggleEngineState(QTreeWidgetItem*, int);
    void setRowColor(int row, QString color);
    void processDownloadedFile(QString url, QString filePath);
    void handleDownloadFailure(QString url, QString reason);
    void displayContextMenu(const QPoint& pos);
    void enableSelection();
    void disableSelection();
    void on_actionUninstall_triggered();
    void on_updateButton_clicked();
    void on_installButton_clicked();
    void dropEvent(QDropEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);
    void installPlugin(QString plugin_path);
};

#endif
