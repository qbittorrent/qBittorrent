#pragma once

#include <QList>
#include <QToolBar>

class QAction;

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

protected:
    void actionEvent(QActionEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QList<QAction *> m_lockedActions;
    bool m_locked = true;
};
