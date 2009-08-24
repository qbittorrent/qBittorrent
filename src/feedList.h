#ifndef FEEDLIST_H
#define FEEDLIST_H

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QDropEvent>
#include <QDragMoveEvent>
#include <QStringList>
#include <QHash>
#include <QUrl>
#include "rss.h"

class FeedList: public QTreeWidget {
  Q_OBJECT

private:
  RssManager *rssmanager;
  QHash<QTreeWidgetItem*, RssFile*> mapping;
  QHash<QString, QTreeWidgetItem*> feeds_items;
  QTreeWidgetItem* current_feed;

public:
  FeedList(QWidget *parent, RssManager *rssmanager): QTreeWidget(parent), rssmanager(rssmanager) {
    setContextMenuPolicy(Qt::CustomContextMenu);
    setDragDropMode(QAbstractItemView::InternalMove);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setColumnCount(1);
    QTreeWidgetItem *___qtreewidgetitem = headerItem();
    ___qtreewidgetitem->setText(0, QApplication::translate("RSS", "RSS feeds", 0, QApplication::UnicodeUTF8));
    connect(this, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)), this, SLOT(updateCurrentFeed(QTreeWidgetItem*)));
  }

  void itemAdded(QTreeWidgetItem *item, RssFile* file) {
    mapping[item] = file;
    if(file->getType() == RssFile::STREAM) {
      feeds_items[file->getID()] = item;
      if(topLevelItemCount() == 1)
        setCurrentItem(item);
    }
  }

  void itemRemoved(QTreeWidgetItem *item) {
    RssFile* file = mapping.take(item);
    if(file->getType() == RssFile::STREAM)
      feeds_items.remove(file->getID());
  }

  bool hasFeed(QString url) {
    return feeds_items.contains(QUrl(url).toString());
  }

  QList<QTreeWidgetItem*> getAllFeedItems() {
    return feeds_items.values();
  }

  QStringList getItemPath(QTreeWidgetItem* item) {
    QStringList path;
    if(item) {
      if(item->parent())
        path.append(getItemPath(item->parent()));
      path.append(getRSSItem(item)->getID());
    }
    return path;
  }

  QList<QTreeWidgetItem*> getAllOpenFolders(QTreeWidgetItem *parent=0) {
    QList<QTreeWidgetItem*> open_folders;
    int nbChildren;
    if(parent)
      nbChildren = parent->childCount();
    else
      nbChildren = topLevelItemCount();
    for(int i=0; i<nbChildren; ++i) {
      QTreeWidgetItem *item;
      if(parent)
        item = parent->child(i);
      else
        item = topLevelItem(i);
      if(getItemType(item) == RssFile::FOLDER && item->isExpanded()) {
        QList<QTreeWidgetItem*> open_subfolders = getAllOpenFolders(item);
        if(!open_subfolders.empty()) {
          open_folders.append(open_subfolders);
        } else {
          open_folders << item;
        }
      }
    }
    return open_folders;
  }

  QList<QTreeWidgetItem*> getAllFeedItems(QTreeWidgetItem* folder) {
    QList<QTreeWidgetItem*> feeds;
    int nbChildren = folder->childCount();
    for(int i=0; i<nbChildren; ++i) {
      QTreeWidgetItem *item = folder->child(i);
      if(getItemType(item) == RssFile::STREAM) {
        feeds << item;
      } else {
        feeds.append(getAllFeedItems(item));
      }
    }
    return feeds;
  }

  RssFile* getRSSItem(QTreeWidgetItem *item) {
    return mapping[item];
  }

  RssFile* getCurrentRSSItem() {
    return mapping[current_feed];
  }

  RssFile::FileType getItemType(QTreeWidgetItem *item) {
    return mapping[item]->getType();
  }

  QString getItemID(QTreeWidgetItem *item) {
    return mapping[item]->getID();
  }

  QTreeWidgetItem* getTreeItemFromUrl(QString url) const{
    return feeds_items[url];
  }

  QTreeWidgetItem* currentItem() const {
    return current_feed;
  }

  QTreeWidgetItem* currentFeed() const {
    return current_feed;
  }

signals:
  void foldersAltered(QList<QTreeWidgetItem*> folders);
  void overwriteAttempt(QString filename);

protected slots:
  void updateCurrentFeed(QTreeWidgetItem* new_item) {
    if(new_item && getItemType(new_item) == RssFile::STREAM)
      current_feed = new_item;
  }

protected:
  void dragMoveEvent(QDragMoveEvent * event) {
    QTreeWidgetItem *item = itemAt(event->pos());
    if(item && getItemType(item) != RssFile::FOLDER)
      event->ignore();
    else {
      QTreeWidget::dragMoveEvent(event);
    }
  }

  void dropEvent(QDropEvent *event) {
    qDebug("dropEvent");
    QList<QTreeWidgetItem*> folders_altered;
    QTreeWidgetItem *dest_folder_item =  itemAt(event->pos());
    RssFolder *dest_folder;
    if(dest_folder_item) {
      dest_folder = (RssFolder*)getRSSItem(dest_folder_item);
      folders_altered << dest_folder_item;
    } else {
      dest_folder = rssmanager;
    }
    QList<QTreeWidgetItem *> src_items = selectedItems();
    // Check if there is not going to overwrite another file
    foreach(QTreeWidgetItem *src_item, src_items) {
      RssFile *file = getRSSItem(src_item);
      if(dest_folder->hasChild(file->getID())) {
        emit overwriteAttempt(file->getID());
        return;
      }
    }
    // Proceed with the move
    foreach(QTreeWidgetItem *src_item, src_items) {
      QTreeWidgetItem *parent_folder = src_item->parent();
      if(parent_folder && !folders_altered.contains(parent_folder))
        folders_altered << parent_folder;
      // Actually move the file
      RssFile *file = getRSSItem(src_item);
      rssmanager->moveFile(file, dest_folder);
    }
    QTreeWidget::dropEvent(event);
    if(dest_folder_item)
      dest_folder_item->setExpanded(true);
    // Emit signal for update
    if(!folders_altered.empty())
      emit foldersAltered(folders_altered);
  }

};

#endif // FEEDLIST_H
