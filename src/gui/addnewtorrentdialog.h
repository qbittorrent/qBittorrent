/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2012  Christophe Dumez
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

#ifndef ADDNEWTORRENTDIALOG_H
#define ADDNEWTORRENTDIALOG_H

#include <QShortcut>
#include <QDialog>
#include <QUrl>

#include "core/bittorrent/infohash.h"
#include "core/bittorrent/torrentinfo.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class AddNewTorrentDialog;
}
QT_END_NAMESPACE

class TorrentContentFilterModel;
class PropListDelegate;

class AddNewTorrentDialog: public QDialog
{
    Q_OBJECT

public:
    ~AddNewTorrentDialog();

    static void show(QString source, QWidget *parent = 0);

protected:
    void showEvent(QShowEvent *event);

private slots:
    void showAdvancedSettings(bool show);
    void displayContentTreeMenu(const QPoint&);
    void updateDiskSpaceLabel();
    void onSavePathChanged(int);
    void relayout();
    void renameSelectedFile();
    void setdialogPosition();
    void updateMetadata(const BitTorrent::TorrentInfo &info);
    void browseButton_clicked();
    void handleDownloadFailed(const QString &url, const QString &reason);
    void handleRedirectedToMagnet(const QString &url, const QString &magnetUri);
    void handleDownloadFinished(const QString &url, const QString &filePath);

protected slots:
    virtual void accept();
    virtual void reject();

private:
    explicit AddNewTorrentDialog(QWidget *parent = 0);
    bool loadTorrent(const QString& torrent_path);
    bool loadMagnet(const QString& magnet_uri);
    void loadSavePathHistory();
    void saveSavePathHistory() const;
    int indexOfSavePath(const QString& save_path);
    void updateFileNameInSavePaths(const QString& new_filename);
    void loadState();
    void saveState();
    void setMetadataProgressIndicator(bool visibleIndicator, const QString &labelText = QString());
    void setupTreeview();

private:
    Ui::AddNewTorrentDialog *ui;
    TorrentContentFilterModel *m_contentModel;
    PropListDelegate *m_contentDelegate;
    bool m_hasMetadata;
    QString m_filePath;
    BitTorrent::InfoHash m_hash;
    BitTorrent::TorrentInfo m_torrentInfo;
    QShortcut *editHotkey;
    QByteArray m_headerState;
    int m_oldIndex;
};

#endif // ADDNEWTORRENTDIALOG_H
