#pragma once

#include <QStyledItemDelegate>

#include "base/scheduler/scheduleday.h"

using namespace Scheduler;

namespace Gui
{
    enum ScheduleColumn
    {
        FROM,
        TO,
        DOWNLOAD,
        UPLOAD,
        COL_COUNT
    };
}

class RateLimitDelegate final : public QStyledItemDelegate
{
    Q_OBJECT

public:
    RateLimitDelegate(ScheduleDay &scheduleDay, QObject *parent);

private:
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    ScheduleDay &m_scheduleDay;
};
