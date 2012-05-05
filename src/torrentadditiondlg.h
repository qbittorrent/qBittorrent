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

#ifndef TORRENTADDITION_H
#define TORRENTADDITION_H

#include <QStringList>

#include "ui_torrentadditiondlg.h"

#include <libtorrent/torrent_info.hpp>

class TorrentContentFilterModel;
class PropListDelegate;

class torrentAdditionDialog : public QDialog, private Ui_addTorrentDialog{
  Q_OBJECT

public:
  torrentAdditionDialog(QWidget *parent);
  ~torrentAdditionDialog();
  void showLoadMagnetURI(const QString& magnet_uri);
  void showLoadTorrent(const QString& filePath, const QString& fromUrl = QString());
  QString getCurrentTruncatedSavePath(QString* root_folder_or_file_name = 0) const;
  QString getTruncatedSavePath(QString save_path, QString* root_folder_or_file_name = 0) const;
  bool allFiltered() const;

public slots:
  void displayContentListMenu(const QPoint&);
  void renameSelectedFile();
  void updateDiskSpaceLabels();
  void on_browseButton_clicked();
  void on_CancelButton_clicked();
  void savePiecesPriorities();
  void on_OkButton_clicked();
  void hideTorrentContent();
  void limitDialogWidth();
  void saveTruncatedPathHistory();
  void loadSavePathHistory();
  void updateLabelInSavePath(QString label);
  void updateSavePathCurrentText();
  void resetComboLabelIndex(QString text);

protected:
  void closeEvent(QCloseEvent *event);

private:
  void readSettings();
  void saveSettings();

private:
  QString m_fileName;
  QString m_hash;
  QString m_filePath;
  QString m_fromUrl;
  QString m_defaultSavePath;
  QString m_oldLabel;
  bool m_appendLabelToSavePath;
  TorrentContentFilterModel *m_propListModel;
  PropListDelegate *m_propListDelegate;
  unsigned int m_nbFiles;
  boost::intrusive_ptr<libtorrent::torrent_info> m_torrentInfo;
  QStringList m_filesPath;
  bool m_isMagnet;
  int m_hiddenHeight;
  QStringList m_pathHistory;
  bool m_showContentList;
};

#endif
