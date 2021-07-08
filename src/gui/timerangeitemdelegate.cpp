#include "timerangeitemdelegate.h"

#include <QCheckBox>
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

    if (col == PAUSE)
        return new QCheckBox(parent);

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
        auto *timeEdit = qobject_cast<QTimeEdit*>(editor);
        timeEdit->setTime(index.data(Qt::UserRole).toTime());
        connect(timeEdit, &QTimeEdit::timeChanged, this, [this, timeEdit, index](const QTime time)
        {
            bool ok = (index.column() == FROM && m_scheduleDay.canSetStartTime(index.row(), time))
                || (index.column() == TO && m_scheduleDay.canSetEndTime(index.row(), time));

            QString color = ok ? "green" : "red";
            timeEdit->setStyleSheet("border: 1px solid " + color);
        });
    }
    else if (col == PAUSE)
    {
        auto *checkBox = qobject_cast<QCheckBox*>(editor);
        checkBox->setChecked(index.data(Qt::UserRole).toBool());
    }
    else if (col == DOWNLOAD || col == UPLOAD)
    {
        auto *spinBox = qobject_cast<QSpinBox*>(editor);
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
        QTime time = qobject_cast<QTimeEdit*>(editor)->time();

        if (col == FROM)
            m_scheduleDay.editStartTimeAt(row, time);
        else
            m_scheduleDay.editEndTimeAt(row, time);
    }
    else if (col == PAUSE)
    {
        bool pause = qobject_cast<QCheckBox*>(editor)->isChecked();
        m_scheduleDay.editPauseAt(row, pause);
    }
    else if (col == DOWNLOAD || col == UPLOAD)
    {
        int value = qobject_cast<QSpinBox*>(editor)->value();

        if (col == DOWNLOAD)
            m_scheduleDay.editDownloadSpeedAt(row, value);
        else
            m_scheduleDay.editUploadSpeedAt(row, value);
    }
}

void TimeRangeItemDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(index)
    editor->setGeometry(option.rect);
}
