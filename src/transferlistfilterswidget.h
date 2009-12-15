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

#include "transferlistwidget.h"

class TransferListFiltersWidget: public QFrame {
  Q_OBJECT

private:
  QStringList customLabels;
  QListWidget* statusFilters;
  QListWidget* labelFilters;
  QVBoxLayout* vLayout;
  TransferListWidget *transferList;

public:
  TransferListFiltersWidget(QWidget *parent, TransferListWidget *transferList): QFrame(parent), transferList(transferList) {
    // Construct lists
    vLayout = new QVBoxLayout();
    statusFilters = new QListWidget();
    vLayout->addWidget(statusFilters);
    labelFilters = new QListWidget();
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
    connect(transferList, SIGNAL(newLabel(QString)), this, SLOT(addLabel(QString)));

    // Load settings
    loadSettings();

    // Add Label filters
    QListWidgetItem *allLabels = new QListWidgetItem(labelFilters);
    allLabels->setData(Qt::DisplayRole, tr("All labels") + " (0)");
    QListWidgetItem *noLabel = new QListWidgetItem(labelFilters);
    noLabel->setData(Qt::DisplayRole, tr("No label") + " (0)");
    foreach(const QString& label, customLabels) {
      QListWidgetItem *newLabel = new QListWidgetItem(labelFilters);
      newLabel->setText(label + " (0)");
    }
    labelFilters->selectionModel()->select(labelFilters->model()->index(0,0), QItemSelectionModel::Select);
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
  }

protected slots:
  void updateTorrentNumbers(uint nb_downloading, uint nb_seeding, uint nb_active, uint nb_inactive) {
    statusFilters->item(FILTER_ALL)->setData(Qt::DisplayRole, tr("All")+" ("+QString::number(nb_active+nb_inactive)+")");
    statusFilters->item(FILTER_DOWNLOADING)->setData(Qt::DisplayRole, tr("Downloading")+" ("+QString::number(nb_downloading)+")");
    statusFilters->item(FILTER_COMPLETED)->setData(Qt::DisplayRole, tr("Completed")+" ("+QString::number(nb_seeding)+")");
    statusFilters->item(FILTER_ACTIVE)->setData(Qt::DisplayRole, tr("Active")+" ("+QString::number(nb_active)+")");
    statusFilters->item(FILTER_INACTIVE)->setData(Qt::DisplayRole, tr("Inactive")+" ("+QString::number(nb_inactive)+")");
  }

  void addLabel(QString label) {
    if(label.trimmed().isEmpty()) return;
    if(customLabels.contains(label)) return;
    QListWidgetItem *newLabel = new QListWidgetItem(labelFilters);
    newLabel->setText(label + " (0)");
    customLabels << label;
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
        QString label = QInputDialog::getText(this, tr("New Label"), tr("Label:"), QLineEdit::Normal, "", &ok);
        if (ok && !label.isEmpty()) {
          addLabel(label);
        }
        return;
      }
    }
  }

  void removeSelectedLabel() {
    int row = labelFilters->row(labelFilters->selectedItems().first());
    Q_ASSERT(row > 1);
    QString label = customLabels.takeAt(row - 2);
    labelFilters->removeItemWidget(labelFilters->selectedItems().first());
    transferList->removeLabelFromRows(label);
    // Select first label
    labelFilters->selectionModel()->select(labelFilters->model()->index(0,0), QItemSelectionModel::Select);
    applyLabelFilter(0);
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

};

#endif // TRANSFERLISTFILTERSWIDGET_H
