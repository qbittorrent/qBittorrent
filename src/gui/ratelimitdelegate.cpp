#include "ratelimitdelegate.h"

#include <QDebug>
#include <QFileDialog>
#include <QSpinBox>
#include <QTableWidget>
#include <qnamespace.h>

#include "base/scheduler/schedule.h"
#include "optionsdialog.h"

RateLimitDelegate::RateLimitDelegate(Scheduler::ScheduleDay &scheduleDay, QObject *parent)
    : QStyledItemDelegate(parent)
    , m_scheduleDay(scheduleDay)
{
}

QWidget *RateLimitDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const
{
    auto *spinBox = new QSpinBox(parent);
    spinBox->setSuffix(" KiB/s");
    spinBox->setSpecialValueText("∞");
    spinBox->setMaximum(1000000);

    return spinBox;
}

void RateLimitDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    auto *spinBox = static_cast<QSpinBox*>(editor);
    int userData = index.data(Qt::UserRole).toInt();
    spinBox->setValue(userData);
}

void RateLimitDelegate::setModelData(QWidget *editor, QAbstractItemModel *, const QModelIndex &index) const
{
    auto *spinBox = static_cast<QSpinBox*>(editor);
    int value = spinBox->value();
    int column = index.column();

    if (column == 2)
        m_scheduleDay.editDownloadRateAt(index.row(), value);
    else if (column == 3)
        m_scheduleDay.editUploadRateAt(index.row(), value);
}

void RateLimitDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const
{
    qDebug("UpdateEditor Geometry called");
    editor->setGeometry(option.rect);
}
