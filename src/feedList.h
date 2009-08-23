#ifndef FEEDLIST_H
#define FEEDLIST_H

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QDropEvent>
#include <QDragMoveEvent>
#include <QStringList>
#include "rss.h"

class FeedList : public QTreeWidget {

private:
  RssManager *rssmanager;

public:
  FeedList(QWidget *parent, RssManager *rssmanager): QTreeWidget(parent), rssmanager(rssmanager) {
    setContextMenuPolicy(Qt::CustomContextMenu);
    setDragDropMode(QAbstractItemView::InternalMove);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setColumnCount(3);
    QTreeWidgetItem *___qtreewidgetitem = headerItem();
    ___qtreewidgetitem->setText(2, QApplication::translate("RSS", "type", 0, QApplication::UnicodeUTF8));
    ___qtreewidgetitem->setText(1, QApplication::translate("RSS", "url", 0, QApplication::UnicodeUTF8));
    ___qtreewidgetitem->setText(0, QApplication::translate("RSS", "RSS feeds", 0, QApplication::UnicodeUTF8));
    // Hide second column (url) and third column (type)
    hideColumn(1);
    hideColumn(2);
  }

  QStringList getItemPath(QTreeWidgetItem *item) const {
    QStringList path;
    if(item) {
      if(item->parent()) {
        path = getItemPath(item->parent());
      }
      path << item->text(1);
    }
    return path;
  }

protected:
  void dragMoveEvent(QDragMoveEvent * event) {
    QTreeWidgetItem *item = itemAt(event->pos());
    if(item && rssmanager->getFile(getItemPath(item))->getType() != RssFile::FOLDER)
      event->ignore();
    else {
      QAbstractItemView::dragMoveEvent(event);
    }
  }

  void dropEvent(QDropEvent *event) {
    qDebug("dropEvent");
    QTreeWidgetItem *dest_item =  itemAt(event->pos());
    QStringList dest_folder_path = getItemPath(dest_item);
    QList<QTreeWidgetItem *> src_items = selectedItems();
    foreach(QTreeWidgetItem *src_item, src_items) {
      QStringList src_path = getItemPath(src_item);
      QStringList dest_path = dest_folder_path;
      dest_path << src_item->text(1);
      qDebug("Moving file %s to %s", src_path.join("\\").toLocal8Bit().data(), dest_path.join("\\").toLocal8Bit().data());
      rssmanager->moveFile(src_path, dest_path);
    }
    QAbstractItemView::dropEvent (event);
    if(dest_item)
      dest_item->setExpanded(true);
  }

};

#endif // FEEDLIST_H
