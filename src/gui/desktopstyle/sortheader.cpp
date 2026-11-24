// SPDX-License-Identifier: GPL-2.0-or-later
#include "desktopstyle.h"

#include <QHeaderView>
#include <QPainter>
#include <QStyleOptionHeader>
#include <QTreeView>

#include "base/preferences.h"

namespace
{
    // Space for two hollow arrows and the gap after the label, in logical pixels.
    constexpr int SORT_WIDTH = 12;

    class SortHeader final : public QHeaderView
    {
    public:
        using QHeaderView::QHeaderView;

    private:
        void paintSection(QPainter *painter, const QRect &rect, const int logical) const override
        {
            if (!isSortIndicatorShown() || !sectionsClickable() || (rect.width() < SORT_WIDTH))
            {
                QHeaderView::paintSection(painter, rect, logical);
                return;
            }

            QStyleOptionHeaderV2 option;
            initStyleOption(&option);
            initStyleOptionForIndex(&option, logical);
            option.rect = rect;
            option.sortIndicator = QStyleOptionHeader::None;
            painter->save();
            painter->setClipRect(rect, Qt::IntersectClip);
            style()->drawControl(QStyle::CE_HeaderSection, &option, painter, this);

            const QRect contents = style()->subElementRect(QStyle::SE_HeaderLabel, &option, this);
            const QFontMetrics metrics = painter->fontMetrics();
            const int iconWidth = option.icon.isNull() ? 0 : (style()->pixelMetric(QStyle::PM_SmallIconSize, &option, this) + 2);
            const int available = qMax(0, contents.width() - SORT_WIDTH - iconWidth);
            const Qt::TextElideMode elide = (textElideMode() == Qt::ElideNone) ? Qt::ElideRight : textElideMode();
            option.text = metrics.elidedText(option.text, elide, available);
            const int labelWidth = qMin(available, metrics.horizontalAdvance(option.text)) + iconWidth;
            const QRect group = QStyle::alignedRect(option.direction, option.textAlignment,
                {labelWidth + SORT_WIDTH, contents.height()}, contents);
            const QRect label(group.topLeft(), QSize(labelWidth, group.height()));
            option.rect = QStyle::visualRect(option.direction, group, label);
            style()->drawControl(QStyle::CE_HeaderLabel, &option, painter, this);

            const QRect arrows = QStyle::visualRect(option.direction, group,
                {label.right() + 4, group.top(), 8, group.height()});
            painter->translate(arrows.center().x() + 0.5, arrows.center().y() + 0.5);
            painter->setRenderHint(QPainter::Antialiasing);
            const QColor ink = option.palette.color(QPalette::Text);
            const QColor muted = option.palette.color(QPalette::PlaceholderText);
            const auto triangle = [&](const bool ascending)
            {
                const bool active = (logical == sortIndicatorSection())
                        && (ascending == (sortIndicatorOrder() == Qt::AscendingOrder));
                const qreal sign = ascending ? -1.0 : 1.0;
                painter->setPen(QPen(active ? ink : muted, 1.1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                painter->setBrush(active ? QBrush(ink) : Qt::NoBrush);
                painter->drawPolygon(QPolygonF {{-2.8, sign}, {0, 4.1 * sign}, {2.8, sign}});
            };
            triangle(true);
            triangle(false);
            painter->restore();
        }

        QSize sectionSizeFromContents(const int logical) const override
        {
            if (!isSortIndicatorShown() || !sectionsClickable()
                || model()->headerData(logical, orientation(), Qt::SizeHintRole).isValid())
            {
                return QHeaderView::sectionSizeFromContents(logical);
            }

            ensurePolished();
            QStyleOptionHeaderV2 option;
            initStyleOption(&option);
            initStyleOptionForIndex(&option, logical);
            // The painted chevrons replace the native sort indicator in every section.
            option.sortIndicator = QStyleOptionHeader::None;
            QSize size = style()->sizeFromContents(QStyle::CT_HeaderSection, &option, {}, this);
            size.rwidth() += SORT_WIDTH;
            return size;
        }
    };
}

void DesktopStyle::installSortHeader(QTreeView *view)
{
    if (Preferences::instance()->useCustomUITheme())
        return;

    const QHeaderView *original = view->header();
    auto *header = new SortHeader(Qt::Horizontal, view);
    header->setDefaultAlignment(original->defaultAlignment());
    header->setTextElideMode(original->textElideMode());
    header->setSectionsMovable(original->sectionsMovable());
    header->setSectionsClickable(original->sectionsClickable());
    header->setHighlightSections(original->highlightSections());
    header->setStretchLastSection(original->stretchLastSection());
    const QByteArray state = original->saveState();
    view->setHeader(header);
    header->restoreState(state);
}
