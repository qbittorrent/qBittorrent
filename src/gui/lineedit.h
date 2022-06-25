/****************************************************************************
**
** Copyright (c) 2007 Trolltech ASA <info@trolltech.com>
**
** Use, modification and distribution is allowed without limitation,
** warranty, liability or support of any kind.
**
****************************************************************************/

#pragma once

#include <QLineEdit>

class QToolButton;

class LineEdit final : public QLineEdit
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(LineEdit)

public:
    LineEdit(QWidget *parent);

protected:
    void resizeEvent(QResizeEvent *e) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QToolButton *m_searchButton = nullptr;
};
