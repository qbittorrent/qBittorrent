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

#include <QDir>
#include <QFileDialog>
#include <QFile>
#include <fstream>
#include <QMessageBox>
#include <QMenu>
#include <QHeaderView>
#include <QApplication>
#include <QDesktopWidget>
#include <QInputDialog>

#include <libtorrent/version.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/bencode.hpp>
#include "bittorrent.h"
#include "misc.h"
#include "proplistdelegate.h"
#include "ui_torrentadditiondlg.h"
#include "torrentpersistentdata.h"
#include "torrentfilesmodel.h"
#include "preferences.h"
#include "GUI.h"
#include "transferlistwidget.h"
#include "qinisettings.h"

using namespace libtorrent;

class torrentAdditionDialog : public QDialog, private Ui_addTorrentDialog{
  Q_OBJECT

public:
  torrentAdditionDialog(GUI *parent, Bittorrent* _BTSession);
  ~torrentAdditionDialog();
  void readSettings();
  void saveSettings();
  void showLoadMagnetURI(QString magnet_uri);
  void showLoad(QString filePath, QString from_url=QString::null);
  QString getCurrentTruncatedSavePath() const;

public slots:
  void displayContentListMenu(const QPoint&);
  void renameSelectedFile();
  void updateDiskSpaceLabels();
  void on_browseButton_clicked();
  void on_CancelButton_clicked();
  bool allFiltered() const;
  void savePiecesPriorities();
  void on_OkButton_clicked();
  void renameTorrentNameInModel(QString file_path);
  void hideTorrentContent();
  void saveTruncatedPathHistory();
  QStringList getSavePathHistory() const;
  void updateLabelInSavePath(QString label);

signals:
  void torrentPaused(QTorrentHandle &h);

private:
  Bittorrent *BTSession;
  QString fileName;
  QString hash;
  QString filePath;
  QString from_url;
  QString defaultSavePath;
  QString old_label;
  bool appendLabelToSavePath;
  TorrentFilesModel *PropListModel;
  PropListDelegate *PropDelegate;
  unsigned int nbFiles;
  boost::intrusive_ptr<torrent_info> t;
  QStringList files_path;
  bool is_magnet;
  int hidden_height;
};

#endif
