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
    for (QAction *a : asConst(actions()))
    {
        if (widgetForAction(a) == widget)
        {
            action = a;
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
        QAction *a = acts[i];
        if (a == m_dragAction)
            continue;
        QWidget *w = widgetForAction(a);
        if (!w && !a->isSeparator())
            continue;

        const int threshold = w ? (a->isSeparator() ? (movingLeft ? w->x() + w->width() + fw / 2 : w->x() - fw / 2) : w->x() + w->width() / 2) : 0;

        const int compareX = a->isSeparator() ? dragFloatCentre : (movingLeft ? dragFloatLeft : dragFloatRight);
        if (compareX < threshold)
        {
            newInsertBefore = a;
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
        QWidget *bw = widgetForAction(actions()[bi]);
        if (bw)
            return mapToGlobal(QPoint(bw->x(), 0)).x();
    }
    return mapToGlobal(QPoint(width(), 0)).x();
}
