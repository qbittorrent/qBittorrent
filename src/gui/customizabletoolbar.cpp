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
    if (m_locked)
        return QToolBar::eventFilter(watched, event);

    auto *widget = qobject_cast<QWidget *>(watched);
    if (!widget)
        return QToolBar::eventFilter(watched, event);

    switch (event->type())
    {
    case QEvent::MouseButtonPress:
    {
        m_dragJustFinished = false;

        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() != Qt::LeftButton)
            break;

        QAction *action = nullptr;
        for (QAction *a : actions())
        {
            if (widgetForAction(a) == widget)
            {
                action = a;
                break;
            }
        }

        if (!action || m_lockedActions.contains(action) || action->isSeparator())
            break;

        m_dragAction = action;
        m_dragWidget = widget;
        m_dragStartPos = me->globalPosition().toPoint();
        m_dragOffsetX = me->position().toPoint().x();
        break;
    }

    case QEvent::MouseMove:
    {
        if (!m_dragAction)
            break;

        auto *me = static_cast<QMouseEvent *>(event);
        const QPoint globalPos = me->globalPosition().toPoint();

        if (!m_dragging)
        {
            if ((globalPos - m_dragStartPos).manhattanLength() < QApplication::startDragDistance())
                break;
            startDrag(m_dragWidget, globalPos);
        }

        if (m_dragging)
            return true;
        break;
    }

    case QEvent::MouseButtonRelease:
    {
        if (m_dragJustFinished)
        {
            m_dragJustFinished = false;
            return true;
        }
        if (m_dragging)
        {
            auto *me = static_cast<QMouseEvent *>(event);
            endDrag(me->globalPosition().toPoint());
            return true;
        }
        if (m_dragAction)
            m_dragAction = nullptr;
        break;
    }

    default:
        break;
    }

    return QToolBar::eventFilter(watched, event);
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
    const bool movingLeft = (dragFloatCentre < m_lastFloatCentreX);
    m_lastFloatCentreX = dragFloatCentre;

    const int bi = findBoundaryIndex();
    const QList<QAction *> acts = actions();

    QAction *newInsertBefore = (bi >= 0) ? acts[bi] : nullptr;
    for (int i = 0; i < bi; ++i)
    {
        QAction *a = acts[i];
        if (a == m_dragAction)
            continue;
        QWidget *w = widgetForAction(a);
        if (!w && !a->isSeparator())
            continue;

        int threshold = 0;
        if (a->isSeparator())
        {
            threshold = w ? w->x() + w->width() : 0;
        }
        else
        {
            threshold = w->x() + w->width() / 2;
        }

        // Use float left edge for separators when moving left, centre otherwise
        const int compareX = a->isSeparator() ? (movingLeft ? dragFloatLeft : dragFloatRight) : dragFloatCentre;
        if (compareX < threshold)
        {
            newInsertBefore = a;
            break;
        }
    }

    // Only swap when target slot changes, one atomic reorder per crossing
    if (newInsertBefore == m_gapTarget)
        return;

    m_gapTarget = newInsertBefore;

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

void CustomizableToolBar::endDrag(const QPoint &globalPos)
{
    Q_UNUSED(globalPos)

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
