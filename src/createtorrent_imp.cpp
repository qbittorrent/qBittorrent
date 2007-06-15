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

using namespace libtorrent;
using namespace boost::filesystem;

createtorrent::createtorrent(QWidget *parent): QDialog(parent){
  setupUi(this);
  addTracker_button->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/add.png")));
  removeTracker_button->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/remove.png")));
  addURLSeed_button->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/add.png")));
  removeURLSeed_button->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/remove.png")));
  removeFolder_button->setIcon(QIcon(QString::fromUtf8(":/Icons/skin/remove.png")));
  addFolder_button->setIcon(QIcon(QString::fromUtf8(":/Icons/add_folder.png")));
  addFile_button->setIcon(QIcon(QString::fromUtf8(":/Icons/add_file.png")));
  setAttribute(Qt::WA_DeleteOnClose);
  show();
}

void createtorrent::on_browse_destination_clicked(){
  QString destination = QFileDialog::getSaveFileName(this, tr("Select destination torrent file"), QDir::homePath(), tr("Torrent Files")+" (*.torrent)");
  if(!destination.isEmpty()){
    if(!destination.endsWith(".torrent"))
      destination += ".torrent";
    txt_destination->setText(destination);
  }
}

void createtorrent::on_addFolder_button_clicked(){
  QString dir = QFileDialog::getExistingDirectory(this, tr("Select a folder to add to the torrent"), QDir::homePath(), QFileDialog::ShowDirsOnly);
  if(!dir.isEmpty() && input_list->findItems(dir, Qt::MatchCaseSensitive).size() == 0)
    input_list->addItem(dir);
}

void createtorrent::on_addFile_button_clicked(){
  QStringList files = QFileDialog::getOpenFileNames(this, tr("Select files to add to the torrent"), QDir::homePath(), QString(), 0, QFileDialog::ShowDirsOnly);
  QString file;
  foreach(file, files){
    if(input_list->findItems(file, Qt::MatchCaseSensitive).size() == 0){
      input_list->addItem(file);
    }
  }
}

void createtorrent::on_removeFolder_button_clicked(){
  QModelIndexList selectedIndexes = input_list->selectionModel()->selectedIndexes();
  for(int i=selectedIndexes.size()-1; i>=0; --i){
    QListWidgetItem *item = input_list->takeItem(selectedIndexes.at(i).row());
    delete item;
  }
}

void createtorrent::on_removeTracker_button_clicked(){
  QModelIndexList selectedIndexes = trackers_list->selectionModel()->selectedIndexes();
  for(int i=selectedIndexes.size()-1; i>=0; --i){
    QListWidgetItem *item = trackers_list->takeItem(selectedIndexes.at(i).row());
    delete item;
  }
}

void createtorrent::on_addTracker_button_clicked(){
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
  path f(p / l);
  if (is_directory(f)){
    for (directory_iterator i(f), end; i != end; ++i)
      add_files(t, p, l / i->leaf());
  }else{
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
  QString destination = txt_destination->text();
  if(destination.isEmpty()){
    QMessageBox::critical(0, tr("No destination path set"), tr("Please type a destination path first"));
    return;
  }
  QStringList input = allItems(input_list);
  if(input.size() == 0){
    QMessageBox::critical(0, tr("No input path set"), tr("Please type an input path first"));
    return;
  }
  char const* creator_str = "qBittorrent "VERSION;
  try {
    torrent_info t;
    QString input_path;
    path full_path;
    ofstream out(complete(path((const char*)destination.toUtf8())), std::ios_base::binary);
    foreach(input_path, input){
      full_path = complete(path((const char*)input_path.toUtf8()));
      add_files(t, full_path.branch_path(), full_path.leaf());
    }
    int piece_size = 256 * 1024;
    t.set_piece_size(piece_size);
    // Add url seeds
    QStringList urlSeeds = allItems(URLSeeds_list);
    QString seed;
    foreach(seed, urlSeeds){
      t.add_url_seed((const char*)seed.toUtf8());
    }
    QStringList trackers = allItems(trackers_list);
    for(int i=0; i<trackers.size(); ++i){
      t.add_tracker((const char*)trackers.at(i).toUtf8());
    }

    // calculate the hash for all pieces
    file_pool fp;
    boost::scoped_ptr<storage_interface> st(default_storage_constructor(t, full_path.branch_path(), fp));
    int num = t.num_pieces();
    std::vector<char> buf(piece_size);
    for (int i = 0; i < num; ++i) {
      st->read(&buf[0], i, 0, t.piece_size(i));
      hasher h(&buf[0], t.piece_size(i));
      t.set_hash(i, h.final());
    }
    // Set qBittorrent as creator and add user comment to
    // torrent_info structure
    t.set_creator(creator_str);
    t.set_comment((const char*)txt_comment->toPlainText().toUtf8());
    // Is private ?
    if(check_private->isChecked()){
      t.set_priv(true);
    }
    // create the torrent and print it to out
    entry e = t.create_torrent();
    libtorrent::bencode(std::ostream_iterator<char>(out), e);
  }
  catch (std::exception& e){
    std::cerr << e.what() << "\n";
    QMessageBox::information(0, tr("Torrent creation"), tr("Torrent creation was unsuccessful, reason: %1").arg(QString(e.what())));
    hide();
    return;
  }
  hide();
  QMessageBox::information(0, tr("Torrent creation"), tr("Torrent was created successfully:")+" "+destination);
}
