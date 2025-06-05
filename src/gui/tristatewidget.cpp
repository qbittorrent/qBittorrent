/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2019  Mike Tzou (Chocobo1)
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

#include "tristatewidget.h"

#include <QApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QString>
#include <QStyle>
#include <QStyleOptionMenuItem>

TriStateWidget::TriStateWidget(const QString &text, QWidget *parent)
    : QWidget {parent}
    , m_text {text}
{
    setMouseTracking(true);  // for visual effects via mouse navigation
    setFocusPolicy(Qt::TabFocus);  // for visual effects via keyboard navigation
}

void TriStateWidget::setCheckState(const Qt::CheckState checkState)
{
    m_checkState = checkState;
}

void TriStateWidget::setCloseOnInteraction(const bool enabled)
{
    m_closeOnInteraction = enabled;
}

QSize TriStateWidget::minimumSizeHint() const
{
    QStyleOptionMenuItem opt;
    opt.initFrom(this);
    opt.menuHasCheckableItems = true;
    const QSize contentSize = fontMetrics().size(Qt::TextSingleLine, m_text);
    return style()->sizeFromContents(QStyle::CT_MenuItem, &opt, contentSize, this);
}

void TriStateWidget::paintEvent(QPaintEvent *)
{
    QStyleOptionMenuItem opt;
    opt.initFrom(this);
    opt.menuItemType = QStyleOptionMenuItem::Normal;
    opt.checkType = QStyleOptionMenuItem::NonExclusive;
    opt.menuHasCheckableItems = true;
    opt.text = m_text;

    switch (m_checkState)
    {
    case Qt::PartiallyChecked:
        opt.state |= QStyle::State_NoChange;
        break;
    case Qt::Checked:
        opt.checked = true;
        break;
    case Qt::Unchecked:
        opt.checked = false;
        break;
    };

    if ((opt.state & QStyle::State_HasFocus)
        || rect().contains(mapFromGlobal(QCursor::pos())))
        {
        opt.state |= QStyle::State_Selected;

        if (QApplication::mouseButtons() != Qt::NoButton)
            opt.state |= QStyle::State_Sunken;
    }

    QPainter painter {this};
    style()->drawControl(QStyle::CE_MenuItem, &opt, &painter, this);
}

void TriStateWidget::mouseReleaseEvent(QMouseEvent *event)
{
    toggleCheckState();

    if (m_closeOnInteraction)
    {
        // parent `triggered` signal will be emitted
        QWidget::mouseReleaseEvent(event);
    }
    else
    {
        update();
        // need to emit `triggered` signal manually
        emit triggered(m_checkState == Qt::Checked);
    }
}

void TriStateWidget::keyPressEvent(QKeyEvent *event)
{
    if ((event->key() == Qt::Key_Return)
        || (event->key() == Qt::Key_Enter))
        {
        toggleCheckState();

        if (!m_closeOnInteraction)
        {
            update();
            // need to emit parent `triggered` signal manually
            emit triggered(m_checkState == Qt::Checked);
            return;
        }
    }

    QWidget::keyPressEvent(event);
}

void TriStateWidget::toggleCheckState()
{
    switch (m_checkState)
    {
    case Qt::Unchecked:
    case Qt::PartiallyChecked:
        m_checkState = Qt::Checked;
        break;
    case Qt::Checked:
        m_checkState = Qt::Unchecked;
        break;
    };
}
