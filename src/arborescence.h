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

#ifndef ARBORESCENCE_H
#define ARBORESCENCE_H

#include <QFileInfo>
#include <QStringList>
#include <QDir>
#include "misc.h"

class torrent_file {
  private:
    torrent_file *parent;
    bool is_dir;
    QString rel_path;
    QList<torrent_file*> children;
    size_type size;
    float progress;
    int priority;
    int index; // Index in torrent_info

  public:
    torrent_file(torrent_file *parent, QString path, bool dir, size_type size=0, int index=-1, float progress=0., int priority=1): parent(parent), is_dir(dir), size(size), progress(progress), priority(priority), index(index){
      qDebug("created a file with index %d", index);
      rel_path = QDir::cleanPath(path);
      if(parent) {
        parent->updateProgress();
        parent->updatePriority(priority);
      }
    }

    ~torrent_file() {
      qDeleteAll(children);
    }

    QString path() const {
      return rel_path;
    }

    QString name() const {
      return rel_path.split(QDir::separator()).last();
    }

    void updateProgress() {
      Q_ASSERT(is_dir);
      if(children.isEmpty()) {
        progress = 0.;
        return;
      }
      float wanted = 0.;
      float done = 0.;
      torrent_file *child;
      foreach(child, children) {
        wanted += child->getSize();
        done += child->getSize()*child->getProgress();
      }
      progress = done / wanted;
      Q_ASSERT(progress >= 0.);
      Q_ASSERT(progress <= 1.);
    }

    void updatePriority(int prio) {
      Q_ASSERT(is_dir);
      torrent_file *child;
      foreach(child, children) {
        if(child->getPriority() != prio) return;
      }
      priority = prio;
    }

    int getPriority() const {
      return priority;
    }

    size_type getSize() const {
      return size;
    }

    float getProgress() const {
      return progress;
    }

    int getIndex() const {
      return index;
    }

    bool isDir() const {
      return is_dir;
    }

    bool hasChildren() const {
      return (!children.isEmpty());
    }

    QList<torrent_file*> getChildren() const {
      return children;
    }

    torrent_file* getChild(QString fileName) const {
      Q_ASSERT(is_dir);
      torrent_file* f;
      foreach(f, children) {
        if(f->name() == fileName) return f;
      }
      return 0;
    }

    void addBytes(size_type b) {
      size += b;
      if(parent)
        parent->addBytes(b);
    }

    torrent_file* addChild(QString fileName, bool dir, size_type size=0, int index = -1, float progress=0., int priority=1) {
      Q_ASSERT(is_dir);
      qDebug("Adding a new child of size: %ld", (long)size);
      torrent_file *f = new torrent_file(this, QDir::cleanPath(rel_path+QDir::separator()+fileName), dir, size, index, progress, priority);
      children << f;
      if(size) {
        addBytes(size);
      }
      return f;
    }

    bool removeFromFS(QString saveDir) {
      QString full_path = saveDir + QDir::separator() + rel_path;
      if(!QFile::exists(full_path)) {
        qDebug("%s does not exist, no need to remove it", full_path.toUtf8().data());
        return true;
      }
      bool success = true;
      torrent_file *f;
      qDebug("We have %d children", children.size());
      foreach(f, children) {
        bool s = f->removeFromFS(saveDir);
        success = s && success;
      }
      if(is_dir) {
        qDebug("trying to remove directory: %s", full_path.toUtf8().data());
        QDir dir(full_path);
        dir.rmdir(full_path);
      } else {
        qDebug("trying to remove file: %s", full_path.toUtf8().data());
        bool s = QFile::remove(full_path);
        success = s && success;
      }
      return success;
    }
};

class arborescence {
  private:
    torrent_file *root;

  public:
    arborescence(torrent_info t) {
      torrent_info::file_iterator fi = t.begin_files();
      if(t.num_files() > 1) {
        root = new torrent_file(0, misc::toQString(t.name()), true);
      } else {
        // XXX: Will crash if there is no file in torrent
        root = new torrent_file(0, misc::toQString(t.name()), false, fi->size, 0);
        return;
      }
      int i = 0;
      while(fi != t.end_files()) {
        QString path = QDir::cleanPath(misc::toQString(fi->path.string()));
        addFile(path, fi->size, i);
        fi++;
        ++i;
      }
      qDebug("real size: %ld, tree size: %ld", (long)t.total_size(), (long)root->getSize());
      Q_ASSERT(root->getSize() == t.total_size());
    }

    arborescence(torrent_info t, std::vector<float> fp, int *prioritiesTab) {
      torrent_info::file_iterator fi = t.begin_files();
      if(t.num_files() > 1) {
        qDebug("More than one file in the torrent, setting a folder as root");
        root = new torrent_file(0, misc::toQString(t.name()), true);
      } else {
        // XXX: Will crash if there is no file in torrent
        qDebug("one file in the torrent, setting it as root with index 0");
        root = new torrent_file(0, misc::toQString(t.name()), false, fi->size, 0, fp[0], prioritiesTab[0]);
        return;
      }
      int i = 0;
      while(fi != t.end_files()) {
        QString path = QDir::cleanPath(misc::toQString(fi->path.string()));
        addFile(path, fi->size, i, fp[i], prioritiesTab[i]);
        fi++;
        ++i;
      }
      qDebug("real size: %ld, tree size: %ld", (long)t.total_size(), (long)root->getSize());
      Q_ASSERT(root->getSize() == t.total_size());
    }

    ~arborescence() {
      delete root;
    }

    torrent_file* getRoot() const {
      return root;
    }

    bool removeFromFS(QString saveDir) {
      if(!QFile::exists(saveDir+QDir::separator()+root->path())) return true;
      bool success = root->removeFromFS(saveDir);
      QDir root_dir(root->path());
      root_dir.rmdir(saveDir+QDir::separator()+root->path());
      return success;
    }

  protected:
    void addFile(QString path, size_type file_size, int index, float progress=0., int priority=1) {
      Q_ASSERT(root->isDir());
      path = QDir::cleanPath(path);
      Q_ASSERT(path.startsWith(root->path()));
      QString relative_path = path.remove(0, root->path().size());
      if(relative_path.at(0) ==QDir::separator())
        relative_path.remove(0, 1);
      QStringList fileNames = relative_path.split(QDir::separator());
      QString fileName;
      torrent_file *dad = root;
      unsigned int nb_i = 0;
      unsigned int size = fileNames.size();
      foreach(fileName, fileNames) {
        ++nb_i;
        if(fileName == ".") continue;
        torrent_file* child = dad->getChild(fileName);
        if(!child) {
          if(nb_i != size) {
            // Folder
            child = dad->addChild(fileName, true);
          } else {
            // File
            child = dad->addChild(fileName, false, file_size, index, progress, priority);
          }
        }
        dad = child;
      }
    }
};

#endif
