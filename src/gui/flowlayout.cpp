/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2023  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2016  The Qt Company Ltd.
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

#include "flowlayout.h"

#include <QHash>
#include <QWidget>

#include "base/global.h"

FlowLayout::FlowLayout(QWidget *parent, const int margin, const int hSpacing, const int vSpacing)
    : QLayout(parent)
    , m_hSpace {hSpacing}
    , m_vSpace {vSpacing}
{
    setContentsMargins(margin, margin, margin, margin);
}

FlowLayout::FlowLayout(const int margin, const int hSpacing, const int vSpacing)
    : m_hSpace {hSpacing}
    , m_vSpace {vSpacing}
{
    setContentsMargins(margin, margin, margin, margin);
}

FlowLayout::~FlowLayout()
{
    QLayoutItem *item;
    while ((item = takeAt(0)))
        delete item;
}

void FlowLayout::addItem(QLayoutItem *item)
{
    m_itemList.append(item);
}

int FlowLayout::horizontalSpacing() const
{
    if (m_hSpace >= 0)
        return m_hSpace;

    return smartSpacing(QStyle::PM_LayoutHorizontalSpacing);
}

int FlowLayout::verticalSpacing() const
{
    if (m_vSpace >= 0)
        return m_vSpace;

    return smartSpacing(QStyle::PM_LayoutVerticalSpacing);
}

int FlowLayout::count() const
{
    return m_itemList.size();
}

QLayoutItem *FlowLayout::itemAt(const int index) const
{
    return m_itemList.value(index);
}

QLayoutItem *FlowLayout::takeAt(const int index)
{
    if ((index >= 0) && (index < m_itemList.size()))
        return m_itemList.takeAt(index);

    return nullptr;
}

Qt::Orientations FlowLayout::expandingDirections() const
{
    return {};
}

bool FlowLayout::hasHeightForWidth() const
{
    return true;
}

int FlowLayout::heightForWidth(const int width) const
{
    const int height = doLayout(QRect(0, 0, width, 0), true);
    return height;
}

void FlowLayout::setGeometry(const QRect &rect)
{
    QLayout::setGeometry(rect);
    doLayout(rect, false);
}

QSize FlowLayout::sizeHint() const
{
    return minimumSize();
}

QSize FlowLayout::minimumSize() const
{
    QSize size;
    for (const QLayoutItem *item : asConst(m_itemList))
        size = size.expandedTo(item->minimumSize());

    const QMargins margins = contentsMargins();
    size += QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
    return size;
}

int FlowLayout::doLayout(const QRect &rect, const bool testOnly) const
{
    int left, top, right, bottom;
    getContentsMargins(&left, &top, &right, &bottom);

    const QRect effectiveRect = rect.adjusted(+left, +top, -right, -bottom);
    int x = effectiveRect.x();
    int y = effectiveRect.y();
    int lineHeight = 0;

    QHash<QLayoutItem *, QPoint> lineItems;
    for (QLayoutItem *item : asConst(m_itemList))
    {
        const QWidget *wid = item->widget();

        int spaceX = horizontalSpacing();
        if (spaceX == -1)
        {
            spaceX = wid->style()->layoutSpacing(
                QSizePolicy::PushButton, QSizePolicy::PushButton, Qt::Horizontal);
        }

        int spaceY = verticalSpacing();
        if (spaceY == -1)
        {
            spaceY = wid->style()->layoutSpacing(
                QSizePolicy::PushButton, QSizePolicy::PushButton, Qt::Vertical);
        }

        int nextX = x + item->sizeHint().width() + spaceX;
        if (((nextX - spaceX) > effectiveRect.right()) && (lineHeight > 0))
        {
            if (!testOnly)
            {
                applyItemsGeometry(lineItems, lineHeight);
                lineItems.clear();
            }

            x = effectiveRect.x();
            y = y + lineHeight + spaceY;
            nextX = x + item->sizeHint().width() + spaceX;
            lineHeight = 0;
        }

        if (!testOnly)
            lineItems[item] = QPoint(x, y);

        x = nextX;
        lineHeight = std::max(lineHeight, item->sizeHint().height());
    }

    if (!testOnly)
        applyItemsGeometry(lineItems, lineHeight);

    return y + lineHeight - rect.y() + bottom;
}

int FlowLayout::smartSpacing(const QStyle::PixelMetric pm) const
{
    QObject *parent = this->parent();
    if (!parent)
        return -1;

    if (parent->isWidgetType())
    {
        auto *pw = static_cast<QWidget *>(parent);
        return pw->style()->pixelMetric(pm, nullptr, pw);
    }

    return static_cast<QLayout *>(parent)->spacing();
}

void FlowLayout::applyItemsGeometry(const QHash<QLayoutItem *, QPoint> &items, const int lineHeight) const
{
    for (auto it = items.cbegin(); it != items.cend(); ++it)
    {
        QLayoutItem *item = it.key();
        QPoint point = it.value();

        const auto alignment = item->alignment();
        const int vSpace = lineHeight - item->sizeHint().height();
        if (alignment & Qt::AlignVCenter)
            point.ry() += vSpace / 2;
        else if (alignment & Qt::AlignBottom)
            point.ry() += vSpace;
        item->setGeometry(QRect(point, item->sizeHint()));
    }
}
