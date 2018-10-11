/****************************************************************************
**
** Copyright (c) 2007 Trolltech ASA <info@trolltech.com>
**
** Use, modification and distribution is allowed without limitation,
** warranty, liability or support of any kind.
**
****************************************************************************/

#include "lineedit.h"

#include <algorithm>

#include <QResizeEvent>
#include <QStyle>
#include <QToolButton>

#include "guiiconprovider.h"

LineEdit::LineEdit(QWidget *parent)
    : QLineEdit(parent)
{
    m_searchButton = new QToolButton(this);
    m_searchButton->setIcon(GuiIconProvider::instance()->getIcon("edit-find"));
    m_searchButton->setCursor(Qt::ArrowCursor);
    m_searchButton->setStyleSheet("QToolButton {border: none; padding: 2px;}");

    // padding between text and widget borders
    setStyleSheet(QString("QLineEdit {padding-left: %1px;}").arg(m_searchButton->sizeHint().width()));

    setClearButtonEnabled(true);

    const int frameWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
    setMaximumHeight(std::max(sizeHint().height(), m_searchButton->sizeHint().height()) + frameWidth * 2);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void LineEdit::resizeEvent(QResizeEvent *e)
{
    const int frameWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
    m_searchButton->move(frameWidth, (e->size().height() - m_searchButton->sizeHint().height()) / 2);
}

void LineEdit::keyPressEvent(QKeyEvent *event)
{
    if ((event->modifiers() == Qt::NoModifier) && (event->key() == Qt::Key_Escape)) {
        clear();
    }
    QLineEdit::keyPressEvent(event);
}
