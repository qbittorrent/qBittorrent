#include "scheduleentryitemdelegate.h"

#include <QCheckBox>
#include <QSpinBox>
#include <QTimeEdit>

#include "base/bittorrent/scheduler/bandwidthscheduler.h"
#include "base/preferences.h"
#include "base/unicodestrings.h"

using namespace Gui;

ScheduleEntryItemDelegate::ScheduleEntryItemDelegate(ScheduleDay &scheduleDay, QObject *parent)
    : QStyledItemDelegate {parent}
    , m_scheduleDay {scheduleDay}
{
}

QWidget *ScheduleEntryItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
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

void ScheduleEntryItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    int col = index.column();

    if (col == FROM || col == TO)
    {
        auto *timeEdit = qobject_cast<QTimeEdit*>(editor);
        timeEdit->setTime(index.data(Qt::UserRole).toTime());
        connect(timeEdit, &QTimeEdit::timeChanged, this, [this, timeEdit, index](const QTime time)
        {
            bool hasChanged = (index.column() == FROM && time != m_scheduleDay.entries()[index.row()].startTime)
                || (index.column() == TO && time != m_scheduleDay.entries()[index.row()].endTime);

            if (!hasChanged)
                timeEdit->setStyleSheet("border: none");
            else
            {
                bool ok = (index.column() == FROM && m_scheduleDay.canSetStartTime(index.row(), time))
                    || (index.column() == TO && m_scheduleDay.canSetEndTime(index.row(), time));

                QString color = ok ? "green" : "red";
                timeEdit->setStyleSheet("border: 1px solid " + color);
            }
        });
    }
    else if (col == PAUSE)
    {
        auto *checkBox = qobject_cast<QCheckBox*>(editor);
        checkBox->setChecked(index.data(Qt::UserRole).toBool());
        connect(checkBox, &QCheckBox::stateChanged, this, [this, checkBox, index](const int value)
        {
            bool hasChanged = m_scheduleDay.entries()[index.row()].pause != (value == 2);
            if (!hasChanged)
                checkBox->setStyleSheet("border: none");
            else
                checkBox->setStyleSheet("border: 2px solid green");
        });
    }
    else if (col == DOWNLOAD || col == UPLOAD)
    {
        auto *spinBox = qobject_cast<QSpinBox*>(editor);
        spinBox->setValue(index.data(Qt::UserRole).toInt());
    }
}

void ScheduleEntryItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *option, const QModelIndex &index) const
{
    Q_UNUSED(option)
    int col = index.column();
    int row = index.row();

    if (col == FROM || col == TO)
    {
        QTime time = qobject_cast<QTimeEdit*>(editor)->time();

        if (col == FROM)
            m_scheduleDay.setStartTimeAt(row, time);
        else
            m_scheduleDay.setEndTimeAt(row, time);
    }
    else if (col == PAUSE)
    {
        bool pause = qobject_cast<QCheckBox*>(editor)->isChecked();
        m_scheduleDay.setPauseAt(row, pause);
    }
    else if (col == DOWNLOAD || col == UPLOAD)
    {
        int value = qobject_cast<QSpinBox*>(editor)->value();

        if (col == DOWNLOAD)
            m_scheduleDay.setDownloadSpeedAt(row, value);
        else
            m_scheduleDay.setUploadSpeedAt(row, value);
    }
}

void ScheduleEntryItemDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(index)
    editor->setGeometry(option.rect);
}
