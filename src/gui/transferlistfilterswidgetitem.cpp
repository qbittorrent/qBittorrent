/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2025  Vladimir Golovnev <glassez@yandex.ru>
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

#include "transferlistfilterswidgetitem.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QFont>
#include <QPainter>
#include <QString>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QVBoxLayout>

#include "transferlistfilters/categoryfilterwidget.h"

namespace
{
    class RailIconDelegate final : public QStyledItemDelegate
    {
    public:
        using QStyledItemDelegate::QStyledItemDelegate;

        QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &) const override
        {
            return {28, qMax(24, option.fontMetrics.height() + 6)};
        }

        void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
        {
            auto *view = static_cast<QAbstractItemView *>(parent());
            QStyleOptionViewItem tile(option);
            // A tree header can be wider than the rail even with its scroll bar hidden.
            const QRect row(0, option.rect.top(), view->viewport()->width(), option.rect.height());
            tile.rect = row.adjusted(2, 1, -2, -1);
            tile.text.clear();
            tile.icon = {};
            tile.features.setFlag(QStyleOptionViewItem::HasDisplay, false);
            tile.features.setFlag(QStyleOptionViewItem::HasDecoration, false);
            painter->save();
            const bool selected = option.state.testFlag(QStyle::State_Selected);
            if (selected || option.state.testFlag(QStyle::State_MouseOver))
            {
                // The native alternate-base colour can be as saturated as the status icon.
                painter->fillRect(row, option.palette.base());
                QColor tint = option.palette.text().color();
                tint.setAlpha(20);
                QColor edge = tint;
                edge.setAlpha(selected ? 62 : 0);
                painter->setRenderHint(QPainter::Antialiasing);
                painter->setPen(QPen(edge, 1));
                painter->setBrush(selected ? option.palette.highlight() : QBrush(tint));
                painter->drawRoundedRect(tile.rect.adjusted(1, 1, -1, -1), 5, 5);
            }
            else
            {
                view->style()->drawControl(QStyle::CE_ItemViewItem, &tile, painter, view);
            }
            const auto icon = index.data(Qt::DecorationRole).value<QIcon>();
            const QRect iconRect = QStyle::alignedRect(option.direction, Qt::AlignCenter, option.decorationSize, tile.rect);
            icon.paint(painter, iconRect);
            if (option.state.testFlag(QStyle::State_HasFocus) && option.state.testFlag(QStyle::State_KeyboardFocusChange))
            {
                painter->setPen(QPen(option.palette.text().color(), 1, Qt::DotLine));
                painter->setBrush(Qt::NoBrush);
                painter->drawRoundedRect(tile.rect.adjusted(1, 1, -1, -1), 5, 5);
            }
            painter->restore();
        }
    };

    class ArrowCheckBox final : public QCheckBox
    {
    public:
        using QCheckBox::QCheckBox;

    private:
        bool hitButton(const QPoint &position) const override
        {
            return text().isEmpty() ? rect().contains(position) : QCheckBox::hitButton(position);
        }

        void paintEvent(QPaintEvent *) override
        {
            QPainter painter {this};

            QStyleOptionViewItem indicatorOption;
            indicatorOption.initFrom(this);
            indicatorOption.rect = style()->subElementRect(QStyle::SE_CheckBoxIndicator, &indicatorOption, this);
            if (text().isEmpty())
                indicatorOption.rect.moveCenter(rect().center());
            indicatorOption.state |= (QStyle::State_Children
                | (isChecked() ? QStyle::State_Open : QStyle::State_None));
            style()->drawPrimitive(QStyle::PE_IndicatorBranch, &indicatorOption, &painter, this);

            QStyleOptionButton labelOption;
            initStyleOption(&labelOption);
            labelOption.rect = style()->subElementRect(QStyle::SE_CheckBoxContents, &labelOption, this);
            style()->drawControl(QStyle::CE_CheckBoxLabel, &labelOption, &painter, this);
        }
    };
}

TransferListFiltersWidgetItem::TransferListFiltersWidgetItem(const QString &caption, QAbstractItemView *filterWidget, QWidget *parent)
    : QWidget(parent)
    , m_caption {new ArrowCheckBox(caption, this)}
    , m_filterWidget {filterWidget}
{
    m_originalDelegate = filterWidget->itemDelegate();
    m_iconDelegate = new RailIconDelegate(filterWidget);
    m_caption->setAccessibleName(caption);

    QFont font;
    font.setBold(true);
    font.setCapitalization(QFont::AllUppercase);
    m_caption->setFont(font);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 2, 0, 0);
    layout->setSpacing(2);
    layout->setAlignment(Qt::AlignTop);
    layout->addWidget(m_caption);
    layout->addWidget(m_filterWidget);

    m_filterWidget->setVisible(m_caption->isChecked());

    connect(m_caption, &QCheckBox::toggled, m_filterWidget, &QWidget::setVisible);
    connect(m_caption, &QCheckBox::toggled, this, &TransferListFiltersWidgetItem::toggled);
}

void TransferListFiltersWidgetItem::setCompact(const bool compact)
{
    m_caption->setText(compact ? QString() : m_caption->accessibleName());
    m_filterWidget->setItemDelegate(compact ? m_iconDelegate : m_originalDelegate);
    if (auto *categories = qobject_cast<CategoryFilterWidget *>(m_filterWidget))
        categories->setCompact(compact);
    m_filterWidget->doItemsLayout();
    m_filterWidget->updateGeometry();
}

bool TransferListFiltersWidgetItem::isChecked() const
{
    return m_caption->isChecked();
}

void TransferListFiltersWidgetItem::setChecked(const bool value)
{
    m_caption->setChecked(value);
}
