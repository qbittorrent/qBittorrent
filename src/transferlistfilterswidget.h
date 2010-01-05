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

#ifndef TRANSFERLISTFILTERSWIDGET_H
#define TRANSFERLISTFILTERSWIDGET_H

#include <QListWidget>
#include <QListWidgetItem>
#include <QFrame>
#include <QIcon>
#include <QSettings>
#include <QVBoxLayout>
#include <QMenu>
#include <QInputDialog>
#include <QDragMoveEvent>
#include <QStandardItemModel>
#include <QMessageBox>

#include "transferlistdelegate.h"
#include "transferlistwidget.h"

class LabelFiltersList: public QListWidget {
  Q_OBJECT

private:
  QListWidgetItem *itemHover;

public:
  LabelFiltersList(QWidget *parent): QListWidget(parent) {
    itemHover = 0;
    // Accept drop
    setAcceptDrops(true);
  }

signals:
  void torrentDropped(int label_row);

protected:
  void dragMoveEvent(QDragMoveEvent *event) {
    //qDebug("filters, dragmoveevent");
    if(itemAt(event->pos()) && row(itemAt(event->pos())) > 0) {
      //qDebug("Name: %s", itemAt(event->pos())->text().toLocal8Bit().data());
      if(itemHover) {
        if(itemHover != itemAt(event->pos())) {
          setItemHover(false);
          itemHover = itemAt(event->pos());
          setItemHover(true);
        }
      } else {
        itemHover = itemAt(event->pos());
        setItemHover(true);
      }
      event->acceptProposedAction();
    } else {
      if(itemHover)
        setItemHover(false);
      event->ignore();
    }
  }

  void dropEvent(QDropEvent *event) {
    qDebug("Drop Event in labels list");
    if(itemAt(event->pos())) {
      emit torrentDropped(row(itemAt(event->pos())));
    }
    event->ignore();
    setItemHover(false);
    // Select current item again
    currentItem()->setSelected(true);
  }

  void dragLeaveEvent(QDragLeaveEvent*) {
    if(itemHover)
      setItemHover(false);
    // Select current item again
    currentItem()->setSelected(true);
  }

  void setItemHover(bool hover) {
    Q_ASSERT(itemHover);
    if(hover) {
      itemHover->setData(Qt::DecorationRole, QIcon(":/Icons/oxygen/folder-documents.png"));
      itemHover->setSelected(true);
      //setCurrentItem(itemHover);
    } else {
      itemHover->setData(Qt::DecorationRole, QIcon(":/Icons/oxygen/folder.png"));
      //itemHover->setSelected(false);
      itemHover = 0;
    }
  }
};

class TransferListFiltersWidget: public QFrame {
  Q_OBJECT

private:
  QStringList customLabels;
  QList<int> labelCounters;
  QListWidget* statusFilters;
  LabelFiltersList* labelFilters;
  QVBoxLayout* vLayout;
  TransferListWidget *transferList;
  int nb_labeled;
  int nb_torrents;

public:
  TransferListFiltersWidget(QWidget *parent, TransferListWidget *transferList): QFrame(parent), transferList(transferList), nb_labeled(0), nb_torrents(0) {
    // Construct lists
    vLayout = new QVBoxLayout();
    statusFilters = new QListWidget(this);
    vLayout->addWidget(statusFilters);
    labelFilters = new LabelFiltersList(this);
    vLayout->addWidget(labelFilters);
    setLayout(vLayout);
    // Limit status filters list height
    statusFilters->setFixedHeight(100);
    setContentsMargins(0,0,0,0);
    vLayout->setSpacing(2);
    // Add status filters
    QListWidgetItem *all = new QListWidgetItem(statusFilters);
    all->setData(Qt::DisplayRole, tr("All") + " (0)");
    all->setData(Qt::DecorationRole, QIcon(":/Icons/skin/filterall.png"));
    QListWidgetItem *downloading = new QListWidgetItem(statusFilters);
    downloading->setData(Qt::DisplayRole, tr("Downloading") + " (0)");
    downloading->setData(Qt::DecorationRole, QIcon(":/Icons/skin/downloading.png"));
    QListWidgetItem *completed = new QListWidgetItem(statusFilters);
    completed->setData(Qt::DisplayRole, tr("Completed") + " (0)");
    completed->setData(Qt::DecorationRole, QIcon(":/Icons/skin/uploading.png"));
    QListWidgetItem *active = new QListWidgetItem(statusFilters);
    active->setData(Qt::DisplayRole, tr("Active") + " (0)");
    active->setData(Qt::DecorationRole, QIcon(":/Icons/skin/filteractive.png"));
    QListWidgetItem *inactive = new QListWidgetItem(statusFilters);
    inactive->setData(Qt::DisplayRole, tr("Inactive") + " (0)");
    inactive->setData(Qt::DecorationRole, QIcon(":/Icons/skin/filterinactive.png"));

    // SIGNAL/SLOT
    connect(statusFilters, SIGNAL(currentRowChanged(int)), transferList, SLOT(applyStatusFilter(int)));
    connect(transferList, SIGNAL(torrentStatusUpdate(uint,uint,uint,uint)), this, SLOT(updateTorrentNumbers(uint, uint, uint, uint)));
    connect(labelFilters, SIGNAL(currentRowChanged(int)), this, SLOT(applyLabelFilter(int)));
    connect(labelFilters, SIGNAL(torrentDropped(int)), this, SLOT(torrentDropped(int)));
    connect(transferList, SIGNAL(torrentAdded(QModelIndex)), this, SLOT(torrentAdded(QModelIndex)));
    connect(transferList, SIGNAL(torrentAboutToBeRemoved(QModelIndex)), this, SLOT(torrentAboutToBeDeleted(QModelIndex)));
    connect(transferList, SIGNAL(torrentChangedLabel(QString,QString)), this, SLOT(torrentChangedLabel(QString, QString)));

    // Add Label filters
    QListWidgetItem *allLabels = new QListWidgetItem(labelFilters);
    allLabels->setData(Qt::DisplayRole, tr("All labels") + " (0)");
    allLabels->setData(Qt::DecorationRole, QIcon(":/Icons/oxygen/folder.png"));
    QListWidgetItem *noLabel = new QListWidgetItem(labelFilters);
    noLabel->setData(Qt::DisplayRole, tr("Unlabeled") + " (0)");
    noLabel->setData(Qt::DecorationRole, QIcon(":/Icons/oxygen/folder.png"));

    // Load settings
    loadSettings();

    labelFilters->setCurrentRow(0);
    //labelFilters->selectionModel()->select(labelFilters->model()->index(0,0), QItemSelectionModel::Select);

    // Label menu
    labelFilters->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(labelFilters, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showLabelMenu(QPoint)));
  }

  ~TransferListFiltersWidget() {
    saveSettings();
    delete statusFilters;
    delete labelFilters;
    delete vLayout;
  }

  void saveSettings() const {
    QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
    settings.beginGroup(QString::fromUtf8("TransferListFilters"));
    settings.setValue("selectedFilterIndex", QVariant(statusFilters->currentRow()));
    //settings.setValue("selectedLabelIndex", QVariant(labelFilters->currentRow()));
    settings.setValue("customLabels", customLabels);
  }

  void saveCustomLabels() const {
    QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
    settings.beginGroup(QString::fromUtf8("TransferListFilters"));
    settings.setValue("customLabels", customLabels);
  }

  void loadSettings() {
    QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
    settings.beginGroup(QString::fromUtf8("TransferListFilters"));
    statusFilters->setCurrentRow(settings.value("selectedFilterIndex", 0).toInt());
    customLabels = settings.value("customLabels", QStringList()).toStringList();
    for(int i=0; i<customLabels.size(); ++i) {
      labelCounters << 0;
    }
    foreach(const QString& label, customLabels) {
      QListWidgetItem *newLabel = new QListWidgetItem(labelFilters);
      newLabel->setText(label + " (0)");
      newLabel->setData(Qt::DecorationRole, QIcon(":/Icons/oxygen/folder.png"));
    }

  }

protected slots:
  void updateTorrentNumbers(uint nb_downloading, uint nb_seeding, uint nb_active, uint nb_inactive) {
    statusFilters->item(FILTER_ALL)->setData(Qt::DisplayRole, tr("All")+" ("+QString::number(nb_active+nb_inactive)+")");
    statusFilters->item(FILTER_DOWNLOADING)->setData(Qt::DisplayRole, tr("Downloading")+" ("+QString::number(nb_downloading)+")");
    statusFilters->item(FILTER_COMPLETED)->setData(Qt::DisplayRole, tr("Completed")+" ("+QString::number(nb_seeding)+")");
    statusFilters->item(FILTER_ACTIVE)->setData(Qt::DisplayRole, tr("Active")+" ("+QString::number(nb_active)+")");
    statusFilters->item(FILTER_INACTIVE)->setData(Qt::DisplayRole, tr("Inactive")+" ("+QString::number(nb_inactive)+")");
  }

  void torrentDropped(int row) {
    Q_ASSERT(row > 0);
    if(row == 1) {
      transferList->setSelectionLabel("");
    } else {
      QString label = customLabels.at(row-2);
      transferList->setSelectionLabel(label);
    }
  }

  void addLabel(QString label) {
    label = misc::toValidFileSystemName(label.trimmed());
    if(label.isEmpty() || customLabels.contains(label)) return;
    QListWidgetItem *newLabel = new QListWidgetItem(labelFilters);
    newLabel->setText(label + " (0)");
    newLabel->setData(Qt::DecorationRole, QIcon(":/Icons/oxygen/folder.png"));
    customLabels << label;
    labelCounters << 0;
    saveCustomLabels();
  }

  void showLabelMenu(QPoint) {
    QMenu labelMenu(labelFilters);
    QAction *removeAct = 0;
    if(!labelFilters->selectedItems().empty() && labelFilters->row(labelFilters->selectedItems().first()) > 1)
      removeAct = labelMenu.addAction(QIcon(":/Icons/oxygen/list-remove.png"), tr("Remove label"));
    QAction *addAct = labelMenu.addAction(QIcon(":/Icons/oxygen/list-add.png"), tr("Add label"));
    QAction *act = 0;
    act = labelMenu.exec(QCursor::pos());
    if(act) {
      if(act == removeAct) {
        removeSelectedLabel();
        return;
      }
      if(act == addAct) {
        bool ok;
        QString label = "";
        bool invalid;
        do {
          invalid = false;
          label = QInputDialog::getText(this, tr("New Label"), tr("Label:"), QLineEdit::Normal, label, &ok);
          if (ok && !label.isEmpty()) {
            if(misc::isValidFileSystemName(label)) {
              addLabel(label);
            } else {
              QMessageBox::warning(this, tr("Invalid label name"), tr("Please don't use any special characters in the label name."));
              invalid = true;
            }
          }
        }while(invalid);
        return;
      }
    }
  }

  void removeSelectedLabel() {
    int row = labelFilters->row(labelFilters->selectedItems().first());
    Q_ASSERT(row > 1);
    QString label = customLabels.takeAt(row - 2);
    labelCounters.removeAt(row-2);
    transferList->removeLabelFromRows(label);
    // Select first label
    labelFilters->setCurrentItem(labelFilters->item(0));
    labelFilters->selectionModel()->select(labelFilters->model()->index(0,0), QItemSelectionModel::Select);
    applyLabelFilter(0);
    // Un display filter
    delete labelFilters->takeItem(row);
    // Save custom labels to remember it was deleted
    saveCustomLabels();
  }

  void applyLabelFilter(int row) {
    switch(row) {
    case 0:
      transferList->applyLabelFilter("all");
      break;
    case 1:
      transferList->applyLabelFilter("none");
      break;
    default:
      transferList->applyLabelFilter(customLabels.at(row-2));
    }
  }

  void torrentChangedLabel(QString old_label, QString new_label) {
    qDebug("Torrent label changed from %s to %s", old_label.toLocal8Bit().data(), new_label.toLocal8Bit().data());
    if(!old_label.isEmpty()) {
      int i = customLabels.indexOf(old_label);
      if(i >= 0) {
        int new_count = labelCounters[i]-1;
        Q_ASSERT(new_count >= 0);
        labelCounters.replace(i, new_count);
        labelFilters->item(i+2)->setText(old_label + " ("+ QString::number(new_count) +")");
      }
      --nb_labeled;
    }
    if(!new_label.isEmpty()) {
      if(!customLabels.contains(new_label))
        addLabel(new_label);
      int i = customLabels.indexOf(new_label);
      int new_count = labelCounters[i]+1;
      labelCounters.replace(i, new_count);
      labelFilters->item(i+2)->setText(new_label + " ("+ QString::number(new_count) +")");
      ++nb_labeled;
    }
    updateStickyLabelCounters();
  }

  void torrentAdded(QModelIndex index) {
    Q_ASSERT(index.isValid());
    if(!index.isValid()) return;
    QString label = transferList->getSourceModel()->index(index.row(), TR_LABEL).data(Qt::DisplayRole).toString().trimmed();
    qDebug("New torrent was added with label: %s", label.toLocal8Bit().data());
    if(!label.isEmpty()) {
      if(!customLabels.contains(label)) {
        addLabel(label);
      }
      // Update label counter
      int i = customLabels.indexOf(label);
      Q_ASSERT(i >= 0);
      int new_count = labelCounters[i]+1;
      labelCounters.replace(i, new_count);
      Q_ASSERT(labelFilters->item(i+2));
      labelFilters->item(i+2)->setText(label + " ("+ QString::number(new_count) +")");
      ++nb_labeled;
    }
    ++nb_torrents;
    Q_ASSERT(nb_torrents >= 0);
    Q_ASSERT(nb_labeled >= 0);
    Q_ASSERT(nb_labeled <= nb_torrents);
    updateStickyLabelCounters();
  }

  void torrentAboutToBeDeleted(QModelIndex index) {
    Q_ASSERT(index.isValid());
    if(!index.isValid()) return;
    QString label = transferList->getSourceModel()->index(index.row(), TR_LABEL).data(Qt::DisplayRole).toString().trimmed();
    if(!label.isEmpty()) {
      // Update label counter
      int i = customLabels.indexOf(label);
      Q_ASSERT(i >= 0);
      int new_count = labelCounters[i]-1;
      labelCounters.replace(i, new_count);
      labelFilters->item(i+2)->setText(label + " ("+ QString::number(new_count) +")");
      --nb_labeled;
    }
    --nb_torrents;
    qDebug("nb_torrents: %d, nb_labeled: %d", nb_torrents, nb_labeled);
    Q_ASSERT(nb_torrents >= 0);
    Q_ASSERT(nb_labeled >= 0);
    Q_ASSERT(nb_labeled <= nb_torrents);
    updateStickyLabelCounters();
  }

  void updateStickyLabelCounters() {
    labelFilters->item(0)->setText(tr("All labels") + " ("+QString::number(nb_torrents)+")");
    labelFilters->item(1)->setText(tr("Unlabeled") + " ("+QString::number(nb_torrents-nb_labeled)+")");
  }

};

#endif // TRANSFERLISTFILTERSWIDGET_H
