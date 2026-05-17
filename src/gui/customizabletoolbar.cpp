#include "customizabletoolbar.h"

#include <QAction>
#include <QActionEvent>

CustomizableToolBar::CustomizableToolBar(QWidget *parent)
    : QToolBar(parent)
{
}

CustomizableToolBar::CustomizableToolBar(const QString &title, QWidget *parent)
    : QToolBar(title, parent)
{
}

void CustomizableToolBar::lockAction(QAction *action)
{
    if (action && !m_lockedActions.contains(action))
        m_lockedActions.append(action);
}

void CustomizableToolBar::setLocked(bool locked)
{
    m_locked = locked;
}

void CustomizableToolBar::actionEvent(QActionEvent *event)
{
    QToolBar::actionEvent(event);
    if (event->type() == QEvent::ActionAdded)
    {
        QWidget *w = widgetForAction(event->action());
        if (w)
            w->installEventFilter(this);
    }
}

bool CustomizableToolBar::eventFilter(QObject *watched, QEvent *event)
{
    return QToolBar::eventFilter(watched, event);
}
