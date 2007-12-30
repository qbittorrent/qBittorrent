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

#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/fstream.hpp>

#include <libtorrent/entry.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/file.hpp>
#include <libtorrent/storage.hpp>
#include <libtorrent/hasher.hpp>
#include <libtorrent/file_pool.hpp>

#include "createtorrent_imp.h"
#include "misc.h"

using namespace libtorrent;
using namespace boost::filesystem;

createtorrent::createtorrent(QWidget *parent): QDialog(parent){
  setupUi(this);
  setAttribute(Qt::WA_DeleteOnClose);
  show();
}

void createtorrent::on_addFolder_button_clicked(){
  QString dir = QFileDialog::getExistingDirectory(this, tr("Select a folder to add to the torrent"), QDir::homePath(), QFileDialog::ShowDirsOnly);
  if(!dir.isEmpty())
    textInputPath->setText(dir);
}

void createtorrent::on_addFile_button_clicked(){
  QString file = QFileDialog::getOpenFileName(this, tr("Select a file to add to the torrent"), QDir::homePath(), QString(), 0, QFileDialog::ShowDirsOnly);
  if(!file.isEmpty())
    textInputPath->setText(file);
}

void createtorrent::on_removeTracker_button_clicked() {
  QModelIndexList selectedIndexes = trackers_list->selectionModel()->selectedIndexes();
  for(int i=selectedIndexes.size()-1; i>=0; --i){
    QListWidgetItem *item = trackers_list->takeItem(selectedIndexes.at(i).row());
    delete item;
  }
}

int createtorrent::getPieceSize() const {
  switch(comboPieceSize->currentIndex()) {
    case 0:
      return 32*1024;
    case 1:
      return 64*1024;
    case 2:
      return 128*1024;
    case 3:
      return 256*1024;
    case 4:
      return 512*1024;
    case 5:
      return 1024*1024;
    case 6:
      return 2048*1024;
    default:
      return 256*1024;
  }
}

void createtorrent::on_addTracker_button_clicked() {
  bool ok;
  QString URL = QInputDialog::getText(this, tr("Please type an announce URL"),
                                       tr("Announce URL:", "Tracker URL"), QLineEdit::Normal,
                                       "http://", &ok);
  if(ok){
    if(trackers_list->findItems(URL, Qt::MatchFixedString).size() == 0)
      trackers_list->addItem(URL);
  }
}

void createtorrent::on_removeURLSeed_button_clicked(){
  QModelIndexList selectedIndexes = URLSeeds_list->selectionModel()->selectedIndexes();
  for(int i=selectedIndexes.size()-1; i>=0; --i){
    QListWidgetItem *item = URLSeeds_list->takeItem(selectedIndexes.at(i).row());
    delete item;
  }
}

void createtorrent::on_addURLSeed_button_clicked(){
  bool ok;
  QString URL = QInputDialog::getText(this, tr("Please type a web seed url"),
                                       tr("Web seed URL:"), QLineEdit::Normal,
                                       "http://", &ok);
  if(ok){
    if(URLSeeds_list->findItems(URL, Qt::MatchFixedString).size() == 0)
      URLSeeds_list->addItem(URL);
  }
}

// Subfunction to add files to a torrent_info structure
// Written by Arvid Norberg (libtorrent Author)
void add_files(torrent_info& t, path const& p, path const& l){
  qDebug("p: %s, l: %s, l.leaf(): %s", p.string().c_str(), l.string().c_str(), l.leaf().c_str());
  path f(p / l);
  if (is_directory(f)){
    for (directory_iterator i(f), end; i != end; ++i)
      add_files(t, p, l / i->leaf());
  }else{
    qDebug("Adding %s", l.string().c_str());
    t.add_file(l, file_size(f));
  }
}

QStringList createtorrent::allItems(QListWidget *list){
  QStringList res;
  unsigned int nbItems = list->count();
  for(unsigned int i=0; i< nbItems; ++i){
    res << list->item(i)->text();
  }
  return res;
}

// Main function that create a .torrent file
void createtorrent::on_createButton_clicked(){
  QString input = textInputPath->text().trimmed();
  if (input.endsWith(QDir::separator()))
    input.chop(1);
  if(input.isEmpty()){
    QMessageBox::critical(0, tr("No input path set"), tr("Please type an input path first"));
    return;
  }
  QStringList trackers = allItems(trackers_list);
  if(!trackers.size()){
    QMessageBox::critical(0, tr("No tracker path set"), tr("Please set at least one tracker"));
    return;
  }
  QString destination = QFileDialog::getSaveFileName(this, tr("Select destination torrent file"), QDir::homePath(), tr("Torrent Files")+QString::fromUtf8(" (*.torrent)"));
  if(!destination.isEmpty()) {
    if(!destination.endsWith(QString::fromUtf8(".torrent")))
      destination += QString::fromUtf8(".torrent");
  } else {
    return;
  }
  char const* creator_str = "qBittorrent "VERSION;
  try {
    boost::intrusive_ptr<torrent_info> t(new torrent_info);
    ofstream out(complete(path((const char*)destination.toUtf8())), std::ios_base::binary);
    // Adding files to the torrent
    path full_path = complete(path(input.toUtf8().data()));
    add_files(*t, full_path.branch_path(), full_path.leaf());
    // Set piece size
    int piece_size = getPieceSize();
    t->set_piece_size(piece_size);
    // Add url seeds
    QStringList urlSeeds = allItems(URLSeeds_list);
    QString seed;
    foreach(seed, urlSeeds){
      t->add_url_seed(seed.toUtf8().data());
    }
    for(int i=0; i<trackers.size(); ++i){
      t->add_tracker(trackers.at(i).toUtf8().data());
    }

    // calculate the hash for all pieces
    file_pool fp;
    boost::scoped_ptr<storage_interface> st(default_storage_constructor(t, full_path.branch_path(), fp));
    int num = t->num_pieces();
    std::vector<char> buf(piece_size);
    for (int i = 0; i < num; ++i) {
      st->read(&buf[0], i, 0, t->piece_size(i));
      hasher h(&buf[0], t->piece_size(i));
      t->set_hash(i, h.final());
    }
    // Set qBittorrent as creator and add user comment to
    // torrent_info structure
    t->set_creator(creator_str);
    t->set_comment((const char*)txt_comment->toPlainText().toUtf8());
    // Is private ?
    if(check_private->isChecked()){
      t->set_priv(true);
    }
    // create the torrent and print it to out
    entry e = t->create_torrent();
    libtorrent::bencode(std::ostream_iterator<char>(out), e);
    out.flush();
    if(checkStartSeeding->isChecked()) {
      // Create save path file
      QFile savepath_file(misc::qBittorrentPath()+QString::fromUtf8("BT_backup")+QDir::separator()+misc::toQString(t->info_hash())+QString::fromUtf8(".savepath"));
      savepath_file.open(QIODevice::WriteOnly | QIODevice::Text);
      savepath_file.write(full_path.branch_path().string().c_str());
      savepath_file.close();
      emit torrent_to_seed(destination);
    }
  }
  catch (std::exception& e){
    std::cerr << e.what() << "\n";
    QMessageBox::information(0, tr("Torrent creation"), tr("Torrent creation was unsuccessful, reason: %1").arg(QString::fromUtf8(e.what())));
    hide();
    return;
  }
  hide();
  QMessageBox::information(0, tr("Torrent creation"), tr("Torrent was created successfully:")+" "+destination);
}
