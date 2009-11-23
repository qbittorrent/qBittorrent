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
#include <QIcon>
#include <QSettings>

#include "transferlistwidget.h"

class TransferListFiltersWidget: public QListWidget {
  Q_OBJECT

private:
  TransferListWidget *transferList;

public:
  TransferListFiltersWidget(QWidget *parent, TransferListWidget *transferList): QListWidget(parent), transferList(transferList) {
    // Add filters
    QListWidgetItem *all = new QListWidgetItem(this);
    all->setData(Qt::DisplayRole, tr("All") + " (0)");
    all->setData(Qt::DecorationRole, QIcon(":/Icons/skin/filterall.png"));
    QListWidgetItem *downloading = new QListWidgetItem(this);
    downloading->setData(Qt::DisplayRole, tr("Downloading") + " (0)");
    downloading->setData(Qt::DecorationRole, QIcon(":/Icons/skin/downloading.png"));
    QListWidgetItem *completed = new QListWidgetItem(this);
    completed->setData(Qt::DisplayRole, tr("Completed") + " (0)");
    completed->setData(Qt::DecorationRole, QIcon(":/Icons/skin/uploading.png"));
    QListWidgetItem *active = new QListWidgetItem(this);
    active->setData(Qt::DisplayRole, tr("Active") + " (0)");
    active->setData(Qt::DecorationRole, QIcon(":/Icons/skin/filteractive.png"));
    QListWidgetItem *inactive = new QListWidgetItem(this);
    inactive->setData(Qt::DisplayRole, tr("Inactive") + " (0)");
    inactive->setData(Qt::DecorationRole, QIcon(":/Icons/skin/filterinactive.png"));

    // SIGNAL/SLOT
    connect(this, SIGNAL(currentRowChanged(int)), transferList, SLOT(applyFilter(int)));
    connect(transferList, SIGNAL(torrentStatusUpdate(uint,uint,uint,uint)), this, SLOT(updateTorrentNumbers(uint, uint, uint, uint)));

    // Load settings
    loadSettings();
  }

  ~TransferListFiltersWidget() {
    saveSettings();
  }

  void saveSettings() const {
    QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
    settings.beginGroup(QString::fromUtf8("TransferListFilters"));
    settings.setValue("selectedFilterIndex", QVariant(currentRow()));
  }

  void loadSettings() {
    QSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent"));
    settings.beginGroup(QString::fromUtf8("TransferListFilters"));
    setCurrentRow(settings.value("selectedFilterIndex", 0).toInt());
  }

protected slots:
  void updateTorrentNumbers(uint nb_downloading, uint nb_seeding, uint nb_active, uint nb_inactive) {
    item(FILTER_ALL)->setData(Qt::DisplayRole, tr("All")+" ("+QString::number(nb_active+nb_inactive)+")");
    item(FILTER_DOWNLOADING)->setData(Qt::DisplayRole, tr("Downloading")+" ("+QString::number(nb_downloading)+")");
    item(FILTER_COMPLETED)->setData(Qt::DisplayRole, tr("Completed")+" ("+QString::number(nb_seeding)+")");
    item(FILTER_ACTIVE)->setData(Qt::DisplayRole, tr("Active")+" ("+QString::number(nb_active)+")");
    item(FILTER_INACTIVE)->setData(Qt::DisplayRole, tr("Inactive")+" ("+QString::number(nb_inactive)+")");
  }

};

#endif // TRANSFERLISTFILTERSWIDGET_H
