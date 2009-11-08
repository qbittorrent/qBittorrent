#ifndef TRANSFERLISTFILTERSWIDGET_H
#define TRANSFERLISTFILTERSWIDGET_H

#include <QListWidget>
#include <QListWidgetItem>
#include <QIcon>

class TransferListFiltersWidget: public QListWidget {
public:
  TransferListFiltersWidget(QWidget *parent): QListWidget(parent) {
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
  }
};

#endif // TRANSFERLISTFILTERSWIDGET_H
