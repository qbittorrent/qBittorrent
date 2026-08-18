/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  FTA7700
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

#include "customizabletoolbar.h"

#include <QAction>
#include <QActionEvent>
#include <QApplication>
#include <QCursor>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QMouseEvent>
#include <QPixmap>
#include <QTimer>

#include "base/global.h"

CustomizableToolBar::CustomizableToolBar(QWidget *parent)
    : QToolBar(parent)
{
    setMouseTracking(true);
}

CustomizableToolBar::CustomizableToolBar(const QString &title, QWidget *parent)
    : QToolBar(title, parent)
{
    setMouseTracking(true);
}

void CustomizableToolBar::lockAction(QAction *action)
{
    if (action && !m_lockedActions.contains(action))
        m_lockedActions.append(action);
}

void CustomizableToolBar::setLocked(const bool locked)
{
    m_locked = locked;
}

void CustomizableToolBar::actionEvent(QActionEvent *event)
{
    QToolBar::actionEvent(event);
    if (event->type() == QEvent::ActionAdded)
    {
        if (QWidget *widget = widgetForAction(event->action()))
            widget->installEventFilter(this);
    }
}

bool CustomizableToolBar::eventFilter(QObject *watched, QEvent *event)
{
    if (m_locked)
        return QToolBar::eventFilter(watched, event);

    auto *widget = qobject_cast<QWidget *>(watched);
    if (!widget)
        return QToolBar::eventFilter(watched, event);

    auto *mouseEvent = static_cast<QMouseEvent *>(event);

    switch (event->type())
    {
    case QEvent::MouseButtonPress:
        if (handleMousePress(widget, mouseEvent))
            return true;
        break;

    case QEvent::MouseMove:
        if (handleMouseMove(mouseEvent))
            return true;
        break;

    case QEvent::MouseButtonRelease:
        if (handleMouseRelease(mouseEvent))
            return true;
        break;

    default:
        break;
    }

    return QToolBar::eventFilter(watched, event);
}

bool CustomizableToolBar::handleMousePress(QWidget *widget, QMouseEvent *mouseEvent)
{
    m_dragJustFinished = false;

    if (mouseEvent->button() != Qt::LeftButton)
        return false;

    QAction *action = nullptr;
    for (QAction *candidateAction : asConst(actions()))
    {
        if (widgetForAction(candidateAction) == widget)
        {
            action = candidateAction;
            break;
        }
    }

    if (!action || m_lockedActions.contains(action) || action->isSeparator())
        return false;

    m_dragAction = action;
    m_dragWidget = widget;
    m_dragStartPos = mouseEvent->globalPosition().toPoint();
    m_dragOffsetX = mouseEvent->position().toPoint().x();
    return false;
}

bool CustomizableToolBar::handleMouseMove(QMouseEvent *mouseEvent)
{
    if (!m_dragAction)
        return false;

    const QPoint globalPos = mouseEvent->globalPosition().toPoint();

    if (!m_dragging)
    {
        if ((globalPos - m_dragStartPos).manhattanLength() < QApplication::startDragDistance())
            return false;
        startDrag(m_dragWidget, globalPos);
    }

    return m_dragging;
}

bool CustomizableToolBar::handleMouseRelease(QMouseEvent *mouseEvent)
{
    if (m_dragJustFinished)
    {
        m_dragJustFinished = false;
        return true;
    }
    if (m_dragging)
    {
        endDrag(mouseEvent->globalPosition().toPoint());
        return true;
    }
    if (m_dragAction)
        m_dragAction = nullptr;
    return false;
}

void CustomizableToolBar::onDragTimer()
{
    if (!m_dragging)
        return;

    updateDrag(QCursor::pos());

    if (!(QApplication::mouseButtons() & Qt::LeftButton))
        endDrag(QCursor::pos());
}

void CustomizableToolBar::startDrag(QWidget *widget, const QPoint &globalPos)
{
    m_dragging = true;

    // Snapshot BEFORE making transparent
    const QPixmap snap = widget->grab();

    // Make drag widget invisible but keep it in layout, avoiding reflow
    auto *effect = new QGraphicsOpacityEffect(widget);
    effect->setOpacity(0.0);
    widget->setGraphicsEffect(effect);

    // Float label follows cursor globally
    m_floatLabel = new QLabel(nullptr, Qt::ToolTip | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    m_floatLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_floatLabel->setAttribute(Qt::WA_ShowWithoutActivating);
    m_floatLabel->setPixmap(snap);
    m_floatLabel->setFixedSize(snap.size());
    m_floatLabel->show();

    m_dragTimer = new QTimer(this);
    m_dragTimer->setInterval(16);
    connect(m_dragTimer, &QTimer::timeout, this, &CustomizableToolBar::onDragTimer);
    m_dragTimer->start();

    updateDrag(globalPos);
}

void CustomizableToolBar::updateDrag(const QPoint &globalPos)
{
    if (!m_floatLabel)
        return;

    const int fw = m_floatLabel->width();
    const int fh = m_floatLabel->height();
    const QPoint toolbarTopLeft = mapToGlobal(QPoint(0, 0));
    const int boundaryGX = getBoundaryGlobalX();

    const int clampedX = qBound(toolbarTopLeft.x(), globalPos.x() - m_dragOffsetX, boundaryGX - fw + 20);
    const int clampedY = toolbarTopLeft.y() + (height() - fh) / 2;
    m_floatLabel->move(clampedX, clampedY);

    const int dragFloatLeft = mapFromGlobal(QPoint(clampedX, 0)).x();
    const int dragFloatRight = mapFromGlobal(QPoint(clampedX + fw, 0)).x();
    const int dragFloatCentre = mapFromGlobal(QPoint(clampedX + fw / 2, 0)).x();
    const bool atLeftWall = (clampedX <= toolbarTopLeft.x());
    const bool movingLeft = atLeftWall || (dragFloatCentre < m_lastFloatCentreX)
        || ((m_lastFloatCentreX == 0) && (globalPos.x() < m_dragStartPos.x()));
    m_lastFloatCentreX = dragFloatCentre;

    const int bi = findBoundaryIndex();
    const QList<QAction *> acts = actions();

    if (!acts.isEmpty() && (acts.first() == m_dragAction) && !movingLeft
            && (m_lastSwapX >= 0) && (qAbs(dragFloatLeft - m_lastSwapX) < 20))
    {
        return;
    }
    QAction *newInsertBefore = (bi >= 0) ? acts[bi] : nullptr;
    for (int i = 0; i < bi; ++i)
    {
        QAction *action = acts[i];
        if (action == m_dragAction)
            continue;
        QWidget *widget = widgetForAction(action);
        if (!widget && !action->isSeparator())
            continue;

        const int threshold = widget ? (action->isSeparator() ? (movingLeft ? widget->x() + widget->width() + fw / 2 : widget->x() - fw / 2) : widget->x() + widget->width() / 2) : 0;

        const int compareX = action->isSeparator() ? dragFloatCentre : (movingLeft ? dragFloatLeft : dragFloatRight);
        if (compareX < threshold)
        {
            newInsertBefore = action;
            break;
        }
    }

    const bool targetIsSeparator = newInsertBefore && newInsertBefore->isSeparator();
    if (!targetIsSeparator && (m_lastSwapX >= 0) && (qAbs(dragFloatLeft - m_lastSwapX) < 14))
        return;
    // Only swap when target slot changes, one atomic reorder per crossing
    if (newInsertBefore == m_gapTarget)
        return;

    m_gapTarget = newInsertBefore;
    m_lastSwapX = dragFloatLeft;

    // Briefly remove opacity effect during reorder, reapply to new widget
    if (m_dragWidget)
        m_dragWidget->setGraphicsEffect(nullptr);

    removeAction(m_dragAction);
    insertAction(m_gapTarget, m_dragAction);

    QWidget *newWidget = widgetForAction(m_dragAction);
    if (newWidget)
    {
        m_dragWidget = newWidget;
        auto *effect = new QGraphicsOpacityEffect(newWidget);
        effect->setOpacity(0.0);
        newWidget->setGraphicsEffect(effect);
    }
}

void CustomizableToolBar::endDrag(const QPoint &)
{

    if (m_dragTimer)
    {
        m_dragTimer->stop();
        delete m_dragTimer;
        m_dragTimer = nullptr;
    }

    delete m_floatLabel;
    m_floatLabel = nullptr;

    // Remove opacity effect, restore widget appearance
    if (m_dragWidget)
        m_dragWidget->setGraphicsEffect(nullptr);

    // Reorder already committed live in updateDrag, just save state
    emit actionOrderChanged();

    m_gapTarget = nullptr;
    m_lastSwapX = -1;
    m_lastFloatCentreX = 0;
    m_dragAction = nullptr;
    m_dragWidget = nullptr;
    m_dragJustFinished = true;
    m_dragging = false;
}

int CustomizableToolBar::findBoundaryIndex() const
{
    const QList<QAction *> acts = actions();
    for (int i = 0; i < acts.size(); ++i)
    {
        if (m_lockedActions.contains(acts[i]))
            return i;
    }
    return -1;
}

int CustomizableToolBar::getBoundaryGlobalX() const
{
    const int bi = findBoundaryIndex();
    if (bi >= 0)
    {
        QWidget *boundaryWidget = widgetForAction(actions()[bi]);
        if (boundaryWidget)
            return mapToGlobal(QPoint(boundaryWidget->x(), 0)).x();
    }
    return mapToGlobal(QPoint(width(), 0)).x();
}

void CustomizableToolBar::registerCustomizableActions()
{
    m_allActions.clear();
    for (QAction *action : asConst(actions()))
    {
        if (action->isSeparator() || action->text().isEmpty() || m_lockedActions.contains(action))
            continue;

        m_allActions.append(action);
        connect(action, &QAction::enabledChanged, this, [this, action](bool enabled)
        {
            setActionVisible(action, enabled);
        });
    }
}

QList<QAction *> CustomizableToolBar::customizableActions() const
{
    return m_allActions;
}

void CustomizableToolBar::clearHiddenActions()
{
    m_hiddenActions.clear();
}

void CustomizableToolBar::setActionVisible(QAction *action, const bool visible)
{
    if (!action)
        return;

    if (!visible)
    {
        const QList<QAction *> acts = actions();
        if (acts.contains(action))
        {
            m_hiddenActions[action->objectName()] = acts.indexOf(action);
            removeAction(action);
        }
    }
    else
    {
        if (!actions().contains(action))
        {
            const int savedIdx = m_hiddenActions.value(action->objectName(), -1);
            const QList<QAction *> acts = actions();
            if ((savedIdx >= 0) && (savedIdx < acts.size()))
            {
                insertAction(acts[savedIdx], action);
            }
            else
            {
                const int bi = findBoundaryIndex();
                insertAction((bi >= 0) ? acts.value(bi) : nullptr, action);
            }
        }
        m_hiddenActions.remove(action->objectName());
    }
}

bool CustomizableToolBar::isActionVisible(QAction *action) const
{
    return action && actions().contains(action);
}

int CustomizableToolBar::nearestActionIndexBefore(QAction *action) const
{
    const QList<QAction *> acts = actions();
    const int idx = acts.indexOf(action);
    if (idx < 0)
        return -1;

    int prevIdx = idx - 1;
    while ((prevIdx >= 0) && !acts[prevIdx]->isSeparator() && !acts[prevIdx]->isVisible())
        --prevIdx;
    return prevIdx;
}

int CustomizableToolBar::nearestActionIndexAfter(QAction *action) const
{
    const QList<QAction *> acts = actions();
    const int idx = acts.indexOf(action);
    if (idx < 0)
        return acts.size();

    int nextIdx = idx + 1;
    while ((nextIdx < acts.size()) && !acts[nextIdx]->isSeparator() && !acts[nextIdx]->isVisible())
        ++nextIdx;
    return nextIdx;
}

bool CustomizableToolBar::hasSeparatorBefore(QAction *action) const
{
    const int prevIdx = nearestActionIndexBefore(action);
    return (prevIdx >= 0) && actions()[prevIdx]->isSeparator();
}

bool CustomizableToolBar::hasSeparatorAfter(QAction *action) const
{
    const QList<QAction *> acts = actions();
    const int nextIdx = nearestActionIndexAfter(action);
    return (nextIdx < acts.size()) && acts[nextIdx]->isSeparator();
}

void CustomizableToolBar::addSeparatorBefore(QAction *action)
{
    insertSeparator(action);
}

void CustomizableToolBar::addSeparatorAfter(QAction *action)
{
    const QList<QAction *> acts = actions();
    const int nextIdx = nearestActionIndexAfter(action);
    if (nextIdx < acts.size())
        insertSeparator(acts[nextIdx]);
    else
        addSeparator();
}

void CustomizableToolBar::removeSeparatorBefore(QAction *action)
{
    const int prevIdx = nearestActionIndexBefore(action);
    const QList<QAction *> acts = actions();
    if ((prevIdx >= 0) && acts[prevIdx]->isSeparator())
        removeAction(acts[prevIdx]);
}

void CustomizableToolBar::removeSeparatorAfter(QAction *action)
{
    const QList<QAction *> acts = actions();
    const int nextIdx = nearestActionIndexAfter(action);
    if ((nextIdx < acts.size()) && acts[nextIdx]->isSeparator())
        removeAction(acts[nextIdx]);
}

QAction *CustomizableToolBar::actionByName(const QString &name) const
{
    for (QAction *action : asConst(m_allActions))
    {
        if (action->objectName() == name)
            return action;
    }
    return nullptr;
}

void CustomizableToolBar::resetToDefault(const QList<DefaultEntry> &order)
{
    QHash<QString, QAction *> actionMap;
    for (QAction *action : asConst(m_allActions))
        actionMap[action->objectName()] = action;
    m_hiddenActions.clear();

    const int bi = findBoundaryIndex();
    const QList<QAction *> current = actions();
    for (int i = 0; i < bi; ++i)
        removeAction(current[i]);

    QAction *insertAnchor = (bi >= 0) ? current[bi] : nullptr;
    for (auto it = order.rbegin(); it != order.rend(); ++it)
    {
        if (QAction *action = actionMap.value(it->name))
        {
            action->setVisible(true);
            insertAction(insertAnchor, action);
            if (it->sepAfter)
                insertSeparator(insertAnchor);
            insertAnchor = action;
        }
    }
}
