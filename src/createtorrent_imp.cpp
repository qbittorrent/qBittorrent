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

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/fstream.hpp>

#include "libtorrent/entry.hpp"
#include "libtorrent/bencode.hpp"
#include "libtorrent/torrent_info.hpp"
#include "libtorrent/file.hpp"
#include "libtorrent/storage.hpp"
#include "libtorrent/hasher.hpp"

#include "createtorrent_imp.h"

using namespace libtorrent;
using namespace boost::filesystem;

createtorrent::createtorrent(QWidget *parent): QDialog(parent){
  setupUi(this);
  setAttribute(Qt::WA_DeleteOnClose);
  show();
}

void createtorrent::on_browse_destination_clicked(){
  QString destination = QFileDialog::getSaveFileName(this, tr("Select destination torrent file"), QDir::homePath(), tr("Torrent Files")+" (*.torrent)");
  if(!destination.isEmpty()){
    txt_destination->setText(destination);
  }
}

void createtorrent::on_browse_input_clicked(){
  // Can't use QFileDialog static functions for this because
  // user can select a file or a directory
  QFileDialog *fd = new QFileDialog(this, tr("Select input directory or file"), QDir::homePath());
  if(checkDirectory->isChecked()){
    fd->setFileMode(QFileDialog::DirectoryOnly);
  }else{
    fd->setFileMode(QFileDialog::ExistingFile);
  }
  QStringList fileNames;
  if (fd->exec()){
    fileNames = fd->selectedFiles();
    txt_input->setText(fileNames.first());
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

// Main function that create a .torrent file
void createtorrent::on_createButton_clicked(){
  QString destination = txt_destination->text();
  if(destination.isEmpty()){
    QMessageBox::critical(0, tr("No destination path set"), tr("Please type a destination path first"));
    return;
  }
  QString input = txt_input->text();
  if(input.isEmpty()){
    QMessageBox::critical(0, tr("No input path set"), tr("Please type an input path first"));
    return;
  }
  if(!QFile::exists(input)){
    QMessageBox::critical(0, tr("Input path does not exist"), tr("Please type a valid input path first"));
    return;
  }
  char const* creator_str = "qBittorrent";
  int piece_size = 256 * 1024;
  try {
    torrent_info t;
    path full_path = complete(path(input.toStdString().c_str()));
    ofstream out(complete(path(destination.toStdString().c_str())), std::ios_base::binary);

    add_files(t, full_path.branch_path(), full_path.leaf());
    t.set_piece_size(piece_size);

    storage st(t, full_path.branch_path());
    QStringList trackers = txt_announce->toPlainText().split('\n');
    for(int i=0; i<trackers.size(); ++i){
      t.add_tracker(trackers.at(i).toStdString().c_str());
    }

    // calculate the hash for all pieces
    int num = t.num_pieces();
    std::vector<char> buf(piece_size);
    for (int i = 0; i < num; ++i)
    {
      st.read(&buf[0], i, 0, t.piece_size(i));
      hasher h(&buf[0], t.piece_size(i));
      t.set_hash(i, h.final());
    }
    // Set qBittorrent as creator and add user comment to
    // torrent_info structure
    t.set_creator(creator_str);
    t.set_comment(txt_comment->toPlainText().toStdString().c_str());
    // create the torrent and print it to out
    entry e = t.create_torrent();
    libtorrent::bencode(std::ostream_iterator<char>(out), e);
  }
  catch (std::exception& e){
    std::cerr << e.what() << "\n";
  }
  QMessageBox::information(0, tr("Torrent creation"), tr("Torrent was created successfully:")+" "+destination);
  hide();
}
