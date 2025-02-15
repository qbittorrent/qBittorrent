#include "macosshiftclickhandler.h"

#include <QMouseEvent>
#include "transferlistwidget.h"

MacOSShiftClickHandler::MacOSShiftClickHandler(TransferListWidget *parent)
    : QObject(parent)
    , m_treeView(parent)
{
    parent->installEventFilter(this);
}

bool MacOSShiftClickHandler::eventFilter(QObject *watched, QEvent *event)
{
    if ((watched == m_treeView) && (event->type() == QEvent::MouseButtonPress))
    {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() != Qt::LeftButton)
            return false;

        const QModelIndex clickedIndex = m_treeView->indexAt(mouseEvent->pos());
        if (!clickedIndex.isValid())
            return false;

        const Qt::KeyboardModifiers modifiers = mouseEvent->modifiers();
        const bool shiftPressed = modifiers.testFlag(Qt::ShiftModifier);
        const bool commandPressed = modifiers.testFlag(Qt::ControlModifier);

        if (shiftPressed && m_lastClickedIndex.isValid())
        {
            const QItemSelection selection(m_lastClickedIndex, clickedIndex);
            if (commandPressed)
                m_treeView->selectionModel()->select(selection, QItemSelectionModel::Select | QItemSelectionModel::Rows);
            else
                m_treeView->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            m_treeView->selectionModel()->setCurrentIndex(clickedIndex, QItemSelectionModel::NoUpdate);
            return true;
        }

        if (!(modifiers & (Qt::AltModifier | Qt::MetaModifier)))
            m_lastClickedIndex = clickedIndex;
    }

    return false;
} 