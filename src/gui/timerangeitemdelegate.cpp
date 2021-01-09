#include "timerangeitemdelegate.h"

#include <QSpinBox>
#include <QTimeEdit>

#include "base/bittorrent/scheduler/bandwidthscheduler.h"
#include "base/preferences.h"
#include "base/unicodestrings.h"

using namespace Gui;

TimeRangeItemDelegate::TimeRangeItemDelegate(ScheduleDay &scheduleDay, QObject *parent)
    : QStyledItemDelegate {parent}
    , m_scheduleDay {scheduleDay}
{
}

QWidget *TimeRangeItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option)
    int col = index.column();

    if (col == FROM || col == TO)
    {
        const QLocale locale{Preferences::instance()->getLocale()};
        auto *timeEdit = new QTimeEdit(parent);
        timeEdit->setDisplayFormat(locale.timeFormat(QLocale::ShortFormat));
        timeEdit->setWrapping(true);
        return timeEdit;
    }

    if (col == DOWNLOAD || col == UPLOAD)
    {
        auto *spinBox = new QSpinBox(parent);
        spinBox->setSuffix(" KiB/s");
        spinBox->setSpecialValueText(C_INFINITY);
        spinBox->setMaximum(1000000);
        return spinBox;
    }

    return nullptr;
}

void TimeRangeItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    int col = index.column();

    if (col == FROM || col == TO)
    {
        auto *timeEdit = static_cast<QTimeEdit*>(editor);
        timeEdit->setTime(index.data(Qt::UserRole).toTime());
    }
    else if (col == DOWNLOAD || col == UPLOAD)
    {
        auto *spinBox = static_cast<QSpinBox*>(editor);
        spinBox->setValue(index.data(Qt::UserRole).toInt());
    }
}

void TimeRangeItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *option, const QModelIndex &index) const
{
    Q_UNUSED(option)
    int col = index.column();
    int row = index.row();

    if (col == FROM || col == TO)
    {
        QTime time = static_cast<QTimeEdit*>(editor)->time();

        if (col == FROM)
            m_scheduleDay.editStartTimeAt(row, time);
        else
            m_scheduleDay.editEndTimeAt(row, time);
    }
    else if (col == DOWNLOAD || col == UPLOAD)
    {
        int value = static_cast<QSpinBox*>(editor)->value();

        if (col == DOWNLOAD)
            m_scheduleDay.editDownloadRateAt(row, value);
        else
            m_scheduleDay.editUploadRateAt(row, value);
    }
}

void TimeRangeItemDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(index)
    editor->setGeometry(option.rect);
}
