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

#include <QGuiApplication>
#include <QResizeEvent>
#include <QStyle>
#include <QToolButton>

#include "base/global.h"
#include "uithememanager.h"

LineEdit::LineEdit(QWidget *parent)
    : QLineEdit(parent)
{
    m_searchButton = new QToolButton(this);
    m_searchButton->setIcon(UIThemeManager::instance()->getIcon(u"edit-find"_qs));
    m_searchButton->setCursor(Qt::ArrowCursor);
    m_searchButton->setStyleSheet(u"QToolButton {border: none; padding: 2px;}"_qs);

    // padding between text and widget borders
    setStyleSheet(u"QLineEdit {padding-left: %1px;}"_qs.arg(m_searchButton->sizeHint().width()));

    setClearButtonEnabled(true);

    const int frameWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
    setMaximumHeight(std::max(sizeHint().height(), m_searchButton->sizeHint().height()) + frameWidth * 2);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void LineEdit::resizeEvent(QResizeEvent *e)
{
    const int frameWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
    const int xPos = QGuiApplication::isLeftToRight()
                     ? frameWidth
                     : (e->size().width() - m_searchButton->sizeHint().width() - frameWidth);
    m_searchButton->move(xPos, (e->size().height() - m_searchButton->sizeHint().height()) / 2);
}

void LineEdit::keyPressEvent(QKeyEvent *event)
{
    if ((event->modifiers() == Qt::NoModifier) && (event->key() == Qt::Key_Escape))
    {
        clear();
    }
    QLineEdit::keyPressEvent(event);
}
