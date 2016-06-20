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
#include <QStyle>
#include <QToolButton>
#include <QResizeEvent>

LineEdit::LineEdit(QWidget *parent)
    : QLineEdit(parent)
{
    QPixmap pixmap1(":/lineeditimages/search.png");
    searchButton = new QToolButton(this);
    searchButton->setIcon(QIcon(pixmap1));
    searchButton->setIconSize(pixmap1.size());
    searchButton->setCursor(Qt::ArrowCursor);
    searchButton->setStyleSheet("QToolButton { border: none; padding: 2px; }");
    QSize searchButtonHint = searchButton->sizeHint();

    QSize clearButtonHint(0, 0);
#ifndef QBT_USES_QT5
    QPixmap pixmap2(":/lineeditimages/clear_left.png");
    clearButton = new QToolButton(this);
    clearButton->setIcon(QIcon(pixmap2));
    clearButton->setIconSize(pixmap2.size());
    clearButton->setCursor(Qt::ArrowCursor);
    clearButton->setStyleSheet("QToolButton { border: none; padding: 2px; }");
    clearButton->setToolTip(tr("Clear the text"));
    clearButton->hide();
    connect(clearButton, SIGNAL(clicked()), this, SLOT(clear()));
    connect(this, SIGNAL(textChanged(const QString &)), this, SLOT(updateCloseButton(const QString &)));

    clearButtonHint = clearButton->sizeHint();
    setStyleSheet(QString("QLineEdit { padding-left: %1px; padding-right: %2px; }").arg(searchButtonHint.width()).arg(clearButtonHint.width()));
#else
    setClearButtonEnabled(true);
    setStyleSheet(QString("QLineEdit { padding-left: %1px; }").arg(searchButtonHint.width()));  // padding between text and widget borders
#endif

    QSize widgetHint = sizeHint();
    int frameWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
    setMaximumHeight(std::max({ widgetHint.height(), searchButtonHint.height(), clearButtonHint.height() }) + frameWidth * 2);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void LineEdit::resizeEvent(QResizeEvent *e)
{
    int frameWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth);

    QSize sz = searchButton->sizeHint();
    searchButton->move(frameWidth, (e->size().height() - sz.height()) / 2);
#ifndef QBT_USES_QT5
    QSize cz = clearButton->sizeHint();
    clearButton->move((e->size().width() - frameWidth - cz.width()), (e->size().height() - sz.height()) / 2);
#endif
}

#ifndef QBT_USES_QT5
void LineEdit::updateCloseButton(const QString &text)
{
    clearButton->setVisible(!text.isEmpty());
}
#endif
