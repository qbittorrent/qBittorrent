/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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

#ifndef PLUGINSELECTDLG_H
#define PLUGINSELECTDLG_H

#include "ui_pluginselectdlg.h"

class QDropEvent;
class SearchEngine;

class PluginSelectDlg: public QDialog, private Ui::PluginSelectDlg
{
    Q_OBJECT

public:
    explicit PluginSelectDlg(SearchEngine *pluginManager, QWidget *parent = 0);
    ~PluginSelectDlg();

    QList<QTreeWidgetItem*> findItemsWithUrl(QString url);
    QTreeWidgetItem* findItemWithID(QString id);

signals:
    void pluginsChanged();

protected:
    void dropEvent(QDropEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);

private slots:
    void on_actionUninstall_triggered();
    void on_updateButton_clicked();
    void on_installButton_clicked();
    void on_closeButton_clicked();
    void togglePluginState(QTreeWidgetItem*, int);
    void setRowColor(int row, QString color);
    void displayContextMenu(const QPoint& pos);
    void enableSelection(bool enable);
    void askForLocalPlugin();
    void askForPluginUrl();
    void iconDownloaded(const QString &url, QString filePath);
    void iconDownloadFailed(const QString &url, const QString &reason);

    void checkForUpdatesFinished(const QHash<QString, qreal> &updateInfo);
    void checkForUpdatesFailed(const QString &reason);
    void pluginInstalled(const QString &name);
    void pluginInstallationFailed(const QString &name, const QString &reason);
    void pluginUpdated(const QString &name);
    void pluginUpdateFailed(const QString &name, const QString &reason);

private:
    void loadSupportedSearchPlugins();
    void addNewPlugin(QString pluginName);
    void startAsyncOp();
    void finishAsyncOp();

    SearchEngine *m_pluginManager;
    int m_asyncOps;
};

#endif // PLUGINSELECTDLG_H
