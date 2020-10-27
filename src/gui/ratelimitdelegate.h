#pragma once

#include <QStyledItemDelegate>

#include "base/scheduler/scheduleday.h"

class QAbstractItemModel;
class QModelIndex;
class QStyleOptionViewItem;

class RateLimitDelegate final : public QStyledItemDelegate
{
    Q_OBJECT

public:
    RateLimitDelegate(Scheduler::ScheduleDay &scheduleDay, QObject *parent);

private:
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &, const QModelIndex &index) const override;

    Scheduler::ScheduleDay &m_scheduleDay;
};
