#ifndef TRANSFERLISTFILTERSWIDGET_H
#define TRANSFERLISTFILTERSWIDGET_H

#include <QListWidget>
#include <QListWidgetItem>
#include <QIcon>
#include <QSettings>

#include "TransferListWidget.h"

class TransferListFiltersWidget: public QListWidget {

private:
  TransferListWidget *transferList;

public:
  TransferListFiltersWidget(QWidget *parent, TransferListWidget *transferList): QListWidget(parent), transferList(transferList) {
    // Add filters
    QListWidgetItem *all = new QListWidgetItem(this);
    all->setData(Qt::DisplayRole, tr("All"));
    all->setData(Qt::DecorationRole, QIcon(":/Icons/oxygen/folder-remote16.png"));
    QListWidgetItem *downloading = new QListWidgetItem(this);
    downloading->setData(Qt::DisplayRole, tr("Downloading"));
    downloading->setData(Qt::DecorationRole, QIcon(":/Icons/skin/downloading.png"));
    QListWidgetItem *completed = new QListWidgetItem(this);
    completed->setData(Qt::DisplayRole, tr("Completed"));
    completed->setData(Qt::DecorationRole, QIcon(":/Icons/skin/seeding.png"));
    QListWidgetItem *active = new QListWidgetItem(this);
    active->setData(Qt::DisplayRole, tr("Active"));
    active->setData(Qt::DecorationRole, QIcon(":/Icons/oxygen/draw-triangle2.png"));
    QListWidgetItem *inactive = new QListWidgetItem(this);
    inactive->setData(Qt::DisplayRole, tr("Inactive"));
    inactive->setData(Qt::DecorationRole, QIcon(":/Icons/oxygen/draw-rectangle.png"));
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

};

#endif // TRANSFERLISTFILTERSWIDGET_H
