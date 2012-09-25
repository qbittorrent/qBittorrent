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
#include <QVBoxLayout>
#include <QMenu>
#include <QInputDialog>
#include <QDragMoveEvent>
#include <QStandardItemModel>
#include <QMessageBox>
#include <QScrollBar>
#include <QLabel>

#include "transferlistdelegate.h"
#include "transferlistwidget.h"
#include "preferences.h"
#include "qinisettings.h"
#include "torrentmodel.h"
#include "iconprovider.h"
#include "fs_utils.h"

class LabelFiltersList: public QListWidget {
  Q_OBJECT

private:
  QListWidgetItem *itemHover;

public:
  LabelFiltersList(QWidget *parent): QListWidget(parent) {
    itemHover = 0;
    // Accept drop
    setAcceptDrops(true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    setStyleSheet("QListWidget { background: transparent; border: 0 }");
#if defined(Q_WS_MAC)
    setAttribute(Qt::WA_MacShowFocusRect, false);
#endif
  }

  // Redefine addItem() to make sure the list stays sorted
  void addItem(QListWidgetItem *it) {
    Q_ASSERT(count() >= 2);
    for (int i=2; i<count(); ++i) {
      if (item(i)->text().localeAwareCompare(it->text()) >= 0) {
        insertItem(i, it);
        return;
      }
    }
    QListWidget::addItem(it);
  }

  QString labelFromRow(int row) const {
    Q_ASSERT(row > 1);
    const QString &label = item(row)->text();
    QStringList parts = label.split(" ");
    Q_ASSERT(parts.size() >= 2);
    parts.removeLast(); // Remove trailing number
    return parts.join(" ");
  }

  int rowFromLabel(QString label) const {
    Q_ASSERT(!label.isEmpty());
    for (int i=2; i<count(); ++i) {
      if (label == labelFromRow(i)) return i;
    }
    return -1;
  }

signals:
  void torrentDropped(int label_row);

protected:
  void dragMoveEvent(QDragMoveEvent *event) {
    if (itemAt(event->pos()) && row(itemAt(event->pos())) > 0) {
      if (itemHover) {
        if (itemHover != itemAt(event->pos())) {
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
      if (itemHover)
        setItemHover(false);
      event->ignore();
    }
  }

  void dropEvent(QDropEvent *event) {
    qDebug("Drop Event in labels list");
    if (itemAt(event->pos())) {
      emit torrentDropped(row(itemAt(event->pos())));
    }
    event->ignore();
    setItemHover(false);
    // Select current item again
    currentItem()->setSelected(true);
  }

  void dragLeaveEvent(QDragLeaveEvent*) {
    if (itemHover)
      setItemHover(false);
    // Select current item again
    currentItem()->setSelected(true);
  }

  void setItemHover(bool hover) {
    Q_ASSERT(itemHover);
    if (hover) {
      itemHover->setData(Qt::DecorationRole, IconProvider::instance()->getIcon("folder-documents.png"));
      itemHover->setSelected(true);
      //setCurrentItem(itemHover);
    } else {
      itemHover->setData(Qt::DecorationRole, IconProvider::instance()->getIcon("inode-directory.png"));
      //itemHover->setSelected(false);
      itemHover = 0;
    }
  }
};

class StatusFiltersWidget : public QListWidget {
  Q_OBJECT

public:
  StatusFiltersWidget(QWidget *parent) : QListWidget(parent), m_shown(false) {
    setUniformItemSizes(true);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // Height is fixed (sizeHint().height() is used)
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setStyleSheet("QListWidget { background: transparent; border: 0 }");
#if defined(Q_WS_MAC)
    setAttribute(Qt::WA_MacShowFocusRect, false);
#endif
  }

protected:
  QSize sizeHint() const {
    QSize size = QListWidget::sizeHint();
    // Height should be exactly the height of the content
    size.setHeight(contentsSize().height() + 2 * frameWidth()+6);
    return size;
  }

private:
  bool m_shown;
  
};

class TransferListFiltersWidget: public QFrame {
  Q_OBJECT

private:
  QHash<QString, int> customLabels;
  StatusFiltersWidget* statusFilters;
  LabelFiltersList* labelFilters;
  QVBoxLayout* vLayout;
  TransferListWidget *transferList;
  int nb_labeled;
  int nb_torrents;

public:
  TransferListFiltersWidget(QWidget *parent, TransferListWidget *transferList): QFrame(parent), transferList(transferList), nb_labeled(0), nb_torrents(0) {
    // Construct lists
    vLayout = new QVBoxLayout();
    vLayout->setContentsMargins(0, 4, 0, 4);
    QFont font;
    font.setBold(true);
    font.setCapitalization(QFont::SmallCaps);
    QLabel *torrentsLabel = new QLabel(tr("Torrents"));
    torrentsLabel->setIndent(2);
    torrentsLabel->setFont(font);
    vLayout->addWidget(torrentsLabel);
    statusFilters = new StatusFiltersWidget(this);
    vLayout->addWidget(statusFilters);
    QLabel *labelsLabel = new QLabel(tr("Labels"));
    labelsLabel->setIndent(2);
    labelsLabel->setFont(font);
    vLayout->addWidget(labelsLabel);
    labelFilters = new LabelFiltersList(this);
    vLayout->addWidget(labelFilters);
    setLayout(vLayout);
    labelFilters->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    statusFilters->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    statusFilters->setSpacing(0);
    setContentsMargins(0,0,0,0);
    vLayout->setSpacing(2);
    // Add status filters
    QListWidgetItem *all = new QListWidgetItem(statusFilters);
    all->setData(Qt::DisplayRole, QVariant(tr("All") + " (0)"));
    all->setData(Qt::DecorationRole, QIcon(":/Icons/skin/filterall.png"));
    QListWidgetItem *downloading = new QListWidgetItem(statusFilters);
    downloading->setData(Qt::DisplayRole, QVariant(tr("Downloading") + " (0)"));
    downloading->setData(Qt::DecorationRole, QIcon(":/Icons/skin/downloading.png"));
    QListWidgetItem *completed = new QListWidgetItem(statusFilters);
    completed->setData(Qt::DisplayRole, QVariant(tr("Completed") + " (0)"));
    completed->setData(Qt::DecorationRole, QIcon(":/Icons/skin/uploading.png"));
    QListWidgetItem *paused = new QListWidgetItem(statusFilters);
    paused->setData(Qt::DisplayRole, QVariant(tr("Paused") + " (0)"));
    paused->setData(Qt::DecorationRole, QIcon(":/Icons/skin/paused.png"));
    QListWidgetItem *active = new QListWidgetItem(statusFilters);
    active->setData(Qt::DisplayRole, QVariant(tr("Active") + " (0)"));
    active->setData(Qt::DecorationRole, QIcon(":/Icons/skin/filteractive.png"));
    QListWidgetItem *inactive = new QListWidgetItem(statusFilters);
    inactive->setData(Qt::DisplayRole, QVariant(tr("Inactive") + " (0)"));
    inactive->setData(Qt::DecorationRole, QIcon(":/Icons/skin/filterinactive.png"));

    // SIGNAL/SLOT
    connect(statusFilters, SIGNAL(currentRowChanged(int)), transferList, SLOT(applyStatusFilter(int)));
    connect(transferList->getSourceModel(), SIGNAL(dataChanged(QModelIndex,QModelIndex)), SLOT(updateTorrentNumbers()));
    connect(transferList->getSourceModel(), SIGNAL(torrentAdded(TorrentModelItem*)), SLOT(handleNewTorrent(TorrentModelItem*)));
    connect(labelFilters, SIGNAL(currentRowChanged(int)), this, SLOT(applyLabelFilter(int)));
    connect(labelFilters, SIGNAL(torrentDropped(int)), this, SLOT(torrentDropped(int)));
    connect(transferList->getSourceModel(), SIGNAL(torrentAboutToBeRemoved(TorrentModelItem*)), SLOT(torrentAboutToBeDeleted(TorrentModelItem*)));
    connect(transferList->getSourceModel(), SIGNAL(torrentChangedLabel(TorrentModelItem*,QString,QString)), SLOT(torrentChangedLabel(TorrentModelItem*, QString, QString)));

    // Add Label filters
    QListWidgetItem *allLabels = new QListWidgetItem(labelFilters);
    allLabels->setData(Qt::DisplayRole, QVariant(tr("All labels") + " (0)"));
    allLabels->setData(Qt::DecorationRole, IconProvider::instance()->getIcon("inode-directory"));
    QListWidgetItem *noLabel = new QListWidgetItem(labelFilters);
    noLabel->setData(Qt::DisplayRole, QVariant(tr("Unlabeled") + " (0)"));
    noLabel->setData(Qt::DecorationRole, IconProvider::instance()->getIcon("inode-directory"));

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

  StatusFiltersWidget* getStatusFilters() const {
    return statusFilters;
  }

  void saveSettings() const {
    QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
    settings.beginGroup(QString::fromUtf8("TransferListFilters"));
    settings.setValue("selectedFilterIndex", QVariant(statusFilters->currentRow()));
    //settings.setValue("selectedLabelIndex", QVariant(labelFilters->currentRow()));
    settings.setValue("customLabels", QVariant(customLabels.keys()));
  }

  void loadSettings() {
    QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
    statusFilters->setCurrentRow(settings.value("TransferListFilters/selectedFilterIndex", 0).toInt());
    const QStringList label_list = Preferences().getTorrentLabels();
    foreach (const QString &label, label_list) {
      customLabels.insert(label, 0);
      qDebug("Creating label QListWidgetItem: %s", qPrintable(label));
      QListWidgetItem *newLabel = new QListWidgetItem();
      newLabel->setText(label + " (0)");
      newLabel->setData(Qt::DecorationRole, IconProvider::instance()->getIcon("inode-directory"));
      labelFilters->addItem(newLabel);
    }
  }

protected slots:
  void updateTorrentNumbers() {
    const TorrentStatusReport report = transferList->getSourceModel()->getTorrentStatusReport();
    statusFilters->item(FILTER_ALL)->setData(Qt::DisplayRole, QVariant(tr("All")+" ("+QString::number(report.nb_active+report.nb_inactive)+")"));
    statusFilters->item(FILTER_DOWNLOADING)->setData(Qt::DisplayRole, QVariant(tr("Downloading")+" ("+QString::number(report.nb_downloading)+")"));
    statusFilters->item(FILTER_COMPLETED)->setData(Qt::DisplayRole, QVariant(tr("Completed")+" ("+QString::number(report.nb_seeding)+")"));
    statusFilters->item(FILTER_PAUSED)->setData(Qt::DisplayRole, QVariant(tr("Paused")+" ("+QString::number(report.nb_paused)+")"));
    statusFilters->item(FILTER_ACTIVE)->setData(Qt::DisplayRole, QVariant(tr("Active")+" ("+QString::number(report.nb_active)+")"));
    statusFilters->item(FILTER_INACTIVE)->setData(Qt::DisplayRole, QVariant(tr("Inactive")+" ("+QString::number(report.nb_inactive)+")"));
  }

  void torrentDropped(int row) {
    Q_ASSERT(row > 0);
    if (row == 1) {
      transferList->setSelectionLabel("");
    } else {
      transferList->setSelectionLabel(labelFilters->labelFromRow(row));
    }
  }

  void addLabel(QString& label) {
    label = fsutils::toValidFileSystemName(label.trimmed());
    if (label.isEmpty() || customLabels.contains(label)) return;
    QListWidgetItem *newLabel = new QListWidgetItem();
    newLabel->setText(label + " (0)");
    newLabel->setData(Qt::DecorationRole, IconProvider::instance()->getIcon("inode-directory"));
    labelFilters->addItem(newLabel);
    customLabels.insert(label, 0);
    Preferences().addTorrentLabel(label);
  }

  void showLabelMenu(QPoint) {
    QMenu labelMenu(labelFilters);
    QAction *removeAct = 0;
    if (!labelFilters->selectedItems().empty() && labelFilters->row(labelFilters->selectedItems().first()) > 1)
      removeAct = labelMenu.addAction(IconProvider::instance()->getIcon("list-remove"), tr("Remove label"));
    QAction *addAct = labelMenu.addAction(IconProvider::instance()->getIcon("list-add"), tr("Add label..."));
    labelMenu.addSeparator();
    QAction *startAct = labelMenu.addAction(IconProvider::instance()->getIcon("media-playback-start"), tr("Resume torrents"));
    QAction *pauseAct = labelMenu.addAction(IconProvider::instance()->getIcon("media-playback-pause"), tr("Pause torrents"));
    QAction *deleteTorrentsAct = labelMenu.addAction(IconProvider::instance()->getIcon("edit-delete"), tr("Delete torrents"));
    QAction *act = 0;
    act = labelMenu.exec(QCursor::pos());
    if (act) {
      if (act == removeAct) {
        removeSelectedLabel();
        return;
      }
      if (act == deleteTorrentsAct) {
        transferList->deleteVisibleTorrents();
        return;
      }
      if (act == startAct) {
        transferList->startVisibleTorrents();
        return;
      }
      if (act == pauseAct) {
        transferList->pauseVisibleTorrents();
        return;
      }
      if (act == addAct) {
        bool ok;
        QString label = "";
        bool invalid;
        do {
          invalid = false;
          label = QInputDialog::getText(this, tr("New Label"), tr("Label:"), QLineEdit::Normal, label, &ok);
          if (ok && !label.isEmpty()) {
            if (fsutils::isValidFileSystemName(label)) {
              addLabel(label);
            } else {
              QMessageBox::warning(this, tr("Invalid label name"), tr("Please don't use any special characters in the label name."));
              invalid = true;
            }
          }
        } while(invalid);
        return;
      }
    }
  }

  void removeSelectedLabel() {
    const int row = labelFilters->row(labelFilters->selectedItems().first());
    Q_ASSERT(row > 1);
    const QString &label = labelFilters->labelFromRow(row);
    Q_ASSERT(customLabels.contains(label));
    customLabels.remove(label);
    transferList->removeLabelFromRows(label);
    // Select first label
    labelFilters->setCurrentItem(labelFilters->item(0));
    labelFilters->selectionModel()->select(labelFilters->model()->index(0,0), QItemSelectionModel::Select);
    applyLabelFilter(0);
    // Un display filter
    delete labelFilters->takeItem(row);
    // Save custom labels to remember it was deleted
    Preferences().removeTorrentLabel(label);
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
      transferList->applyLabelFilter(labelFilters->labelFromRow(row));
    }
  }

  void torrentChangedLabel(TorrentModelItem *torrentItem, QString old_label, QString new_label) {
    Q_UNUSED(torrentItem);
    qDebug("Torrent label changed from %s to %s", qPrintable(old_label), qPrintable(new_label));
    if (!old_label.isEmpty()) {
      if (customLabels.contains(old_label)) {
        const int new_count = customLabels.value(old_label, 0) - 1;
        Q_ASSERT(new_count >= 0);
        customLabels.insert(old_label, new_count);
        const int row = labelFilters->rowFromLabel(old_label);
        Q_ASSERT(row >= 2);
        labelFilters->item(row)->setText(old_label + " ("+ QString::number(new_count) +")");
      }
      --nb_labeled;
    }
    if (!new_label.isEmpty()) {
      if (!customLabels.contains(new_label))
        addLabel(new_label);
      const int new_count = customLabels.value(new_label, 0) + 1;
      Q_ASSERT(new_count >= 1);
      customLabels.insert(new_label, new_count);
      const int row = labelFilters->rowFromLabel(new_label);
      Q_ASSERT(row >= 2);
      labelFilters->item(row)->setText(new_label + " ("+ QString::number(new_count) +")");
      ++nb_labeled;
    }
    updateStickyLabelCounters();
  }

  void handleNewTorrent(TorrentModelItem* torrentItem) {
    QString label = torrentItem->data(TorrentModelItem::TR_LABEL).toString();
    qDebug("New torrent was added with label: %s", qPrintable(label));
    if (!label.isEmpty()) {
      if (!customLabels.contains(label)) {
        addLabel(label);
        // addLabel may have changed the label, update the model accordingly.
        torrentItem->setData(TorrentModelItem::TR_LABEL, label);
      }
      // Update label counter
      Q_ASSERT(customLabels.contains(label));
      const int new_count = customLabels.value(label, 0) + 1;
      customLabels.insert(label, new_count);
      const int row = labelFilters->rowFromLabel(label);
      qDebug("torrentAdded, Row: %d", row);
      Q_ASSERT(row >= 2);
      Q_ASSERT(labelFilters->item(row));
      labelFilters->item(row)->setText(label + " ("+ QString::number(new_count) +")");
      ++nb_labeled;
    }
    ++nb_torrents;
    Q_ASSERT(nb_torrents >= 0);
    Q_ASSERT(nb_labeled >= 0);
    Q_ASSERT(nb_labeled <= nb_torrents);
    updateStickyLabelCounters();
  }

  void torrentAboutToBeDeleted(TorrentModelItem* torrentItem) {
    Q_ASSERT(torrentItem);
    QString label = torrentItem->data(TorrentModelItem::TR_LABEL).toString();
    if (!label.isEmpty()) {
      // Update label counter
      const int new_count = customLabels.value(label, 0) - 1;
      customLabels.insert(label, new_count);
      const int row = labelFilters->rowFromLabel(label);
      Q_ASSERT(row >= 2);
      labelFilters->item(row)->setText(label + " ("+ QString::number(new_count) +")");
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
