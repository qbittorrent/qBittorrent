#pragma once

#include <QList>
#include <QPoint>
#include <QToolBar>

class QAction;
class QLabel;
class QTimer;
class QWidget;

class CustomizableToolBar final : public QToolBar
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(CustomizableToolBar)

public:
    explicit CustomizableToolBar(QWidget *parent = nullptr);
    explicit CustomizableToolBar(const QString &title, QWidget *parent = nullptr);
    ~CustomizableToolBar() override = default;

    void lockAction(QAction *action);
    void setLocked(bool locked);

signals:
    void actionOrderChanged();

protected:
    void actionEvent(QActionEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void onDragTimer();

    int findBoundaryIndex() const;
    int getBoundaryGlobalX() const;
    void startDrag(QWidget *widget, const QPoint &globalPos);
    void updateDrag(const QPoint &globalPos);
    void endDrag(const QPoint &globalPos);

    QList<QAction *> m_lockedActions;
    bool m_locked = true;

    QAction *m_dragAction = nullptr;
    QAction *m_gapTarget = nullptr;
    QWidget *m_dragWidget = nullptr;
    QLabel *m_floatLabel = nullptr;
    QTimer *m_dragTimer = nullptr;
    QPoint m_dragStartPos;
    int m_dragOffsetX = 0;
    bool m_dragging = false;
    bool m_dragJustFinished = false;
};
