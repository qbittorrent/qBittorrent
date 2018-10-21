#ifndef CLICKABLELABEL_H
#define CLICKABLELABEL_H

#include <QLabel>
#include <QMouseEvent>

/**
 * @brief The ClickableLabel class
 * https://wiki.qt.io/Clickable_QLabel
 */
class ClickableLabel : public QLabel
{
    Q_OBJECT
public:
    explicit ClickableLabel(QWidget *parent = Q_NULLPTR, Qt::WindowFlags f = Qt::WindowFlags());
    explicit ClickableLabel(const QString &text, QWidget *parent = Q_NULLPTR, Qt::WindowFlags f = Qt::WindowFlags());
    ~ClickableLabel() override;

signals:
    void clicked();

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;

};

#endif // CLICKABLELABEL_H
