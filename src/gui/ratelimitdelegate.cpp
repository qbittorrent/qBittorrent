#include "ratelimitdelegate.h"

#include <QDebug>
#include <QFileDialog>
#include <QSpinBox>
#include <QTableWidget>
#include <QTimeEdit>
#include <qnamespace.h>

#include "base/scheduler/schedule.h"
#include "optionsdialog.h"

using namespace Gui;

RateLimitDelegate::RateLimitDelegate(Scheduler::ScheduleDay &scheduleDay, QObject *parent)
    : QStyledItemDelegate(parent)
    , m_scheduleDay(scheduleDay)
{
}

QWidget *RateLimitDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &index) const
{
    int col = index.column();

    if (col == ScheduleColumn::FROM || col == ScheduleColumn::TO) {
        auto *timeEdit = new QTimeEdit(parent);
        timeEdit->setDisplayFormat("hh:mm");
        return timeEdit;
    }

    if (col == ScheduleColumn::DOWNLOAD || col == ScheduleColumn::UPLOAD) {
        auto *spinBox = new QSpinBox(parent);
        spinBox->setSuffix(" KiB/s");
        spinBox->setSpecialValueText("∞");
        spinBox->setMaximum(1000000);
        return spinBox;
    }

    return nullptr;
}

void RateLimitDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    int col = index.column();

    if (col == ScheduleColumn::FROM || col == ScheduleColumn::TO) {
        auto *timeEdit = static_cast<QTimeEdit*>(editor);
        timeEdit->setTime(index.data(Qt::UserRole).toTime());
    }
    else if (col == ScheduleColumn::DOWNLOAD || col == ScheduleColumn::UPLOAD) {
        auto *spinBox = static_cast<QSpinBox*>(editor);
        spinBox->setValue(index.data(Qt::UserRole).toInt());
    }
}

void RateLimitDelegate::setModelData(QWidget *editor, QAbstractItemModel *, const QModelIndex &index) const
{
    int col = index.column();
    int row = index.row();

    if (col == FROM || col == TO) {
        QTime time = static_cast<QTimeEdit*>(editor)->time();

        if (col == FROM)
            m_scheduleDay.editStartTimeAt(row, time);
        else
            m_scheduleDay.editEndTimeAt(row, time);
    }
    else if (col == DOWNLOAD || col == UPLOAD) {
        int value = static_cast<QSpinBox*>(editor)->value();

        if (col == DOWNLOAD)
            m_scheduleDay.editDownloadRateAt(row, value);
        else
            m_scheduleDay.editUploadRateAt(row, value);
    }
}

void RateLimitDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const
{
    editor->setGeometry(option.rect);
}
