/****************************************************************************
**
** Copyright (c) 2007 Trolltech ASA <info@trolltech.com>
**
** Use, modification and distribution is allowed without limitation,
** warranty, liability or support of any kind.
**
****************************************************************************/

#include "lineedit.h"
#include <QToolButton>
#include <QStyle>
#include <QtDebug>

LineEdit::LineEdit(QWidget *parent)
  : QLineEdit(parent)
{
  searchButton = new QToolButton(this);
  QPixmap pixmap1(":/lineeditimages/search.png");
  searchButton->setIcon(QIcon(pixmap1));
  searchButton->setIconSize(pixmap1.size());
  searchButton->setCursor(Qt::ArrowCursor);
  searchButton->setStyleSheet("QToolButton { border: none; padding: 2px; }");
  clearButton = new QToolButton(this);
  QPixmap pixmap2(":/lineeditimages/clear_left.png");
  clearButton->setIcon(QIcon(pixmap2));
  clearButton->setIconSize(pixmap2.size());
  clearButton->setCursor(Qt::ArrowCursor);
  clearButton->setStyleSheet("QToolButton { border: none; padding: 2px; }");
  clearButton->setToolTip(tr("Clear the text"));
  clearButton->hide();
  connect(clearButton, SIGNAL(clicked()), this, SLOT(clear()));
  connect(this, SIGNAL(textChanged(const QString&)), this, SLOT(updateCloseButton(const QString&)));
  int frameWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
  setStyleSheet(QString("QLineEdit { padding-right: %1px; padding-left: %2px; }").arg(clearButton->sizeHint().width() + frameWidth + 1).arg(clearButton->sizeHint().width() + frameWidth + 1));
  QSize msz = minimumSizeHint();
  setMinimumSize(qMax(msz.width(), clearButton->sizeHint().width() + searchButton->sizeHint().width() + frameWidth * 2 + 2),
                 qMax(msz.height(), clearButton->sizeHint().height() + frameWidth * 2 + 2));
}

void LineEdit::resizeEvent(QResizeEvent *)
{
  QSize sz = searchButton->sizeHint();
  int frameWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
  searchButton->move(rect().left() + frameWidth, (rect().bottom() + 2 - sz.height())/2);
  sz = clearButton->sizeHint();
  clearButton->move(rect().right() - frameWidth - sz.width(),
                    (rect().bottom() + 2 - sz.height())/2);
}

void LineEdit::updateCloseButton(const QString& text)
{
  clearButton->setVisible(!text.isEmpty());
}

