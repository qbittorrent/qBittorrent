/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2023-2025  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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

#include "transferlistfilterswidget.h"

#include <QCheckBox>
#include <QEnterEvent>
#include <QIcon>
#include <QKeyEvent>
#include <QListWidgetItem>
#include <QMenu>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include "base/bittorrent/session.h"
#include "base/global.h"
#include "base/preferences.h"
#include "base/settingsstorage.h"
#include "base/utils/compare.h"
#include "transferlistfilters/categoryfilterwidget.h"
#include "transferlistfilters/statusfilterwidget.h"
#include "transferlistfilters/tagfilterwidget.h"
#include "transferlistfilters/trackersfilterwidget.h"
#include "transferlistfilters/trackerstatusfilterwidget.h"
#include "transferlistfilterswidgetitem.h"
#include "transferlistwidget.h"
#include "uithememanager.h"
#include "utils.h"

namespace
{
    const QString COMPACT_KEY = u"GUI/MainWindow/CompactFiltersSidebar"_s;
}

// One transparent overlay keeps the filter views in control of selection, tooltips and context menus.
class TransferListFiltersWidget::Labels final : public QWidget
{
public:
    explicit Labels(TransferListFiltersWidget *sidebar)
        : QWidget(sidebar->window())
        , m_sidebar {sidebar}
    {
        setObjectName(u"filtersSidebarLabels"_s);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setFocusPolicy(Qt::NoFocus);
        hide();
    }

    void display(const int localY)
    {
        m_activeY = localY;
        const QPoint origin = m_sidebar->mapTo(parentWidget(), QPoint());
        const int avail = isRightToLeft() ? origin.x() : parentWidget()->width() - origin.x() - m_sidebar->width();
        const int width = qMin(320, avail);
        if (width <= 0)
            return;
        setGeometry(isRightToLeft() ? origin.x() - width : origin.x() + m_sidebar->width(), origin.y(), width, m_sidebar->height());
        raise();
        show();
        update();
    }

private:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const auto *clip = m_sidebar->m_scroll->viewport();
        const QRect content(clip->mapTo(m_sidebar, QPoint()), clip->size());
        painter.setClipRect(QRect(0, content.y(), width(), content.height()));
        const auto label = [this, &painter, &content](QWidget *source, const QRect &row, const QString &text)
        {
            const int top = source->mapTo(m_sidebar, row.topLeft()).y();
            const qreal center = top + row.height() / 2.0;
            if ((center < content.top()) || (center >= content.y() + content.height()))
                return;
            const bool active = (m_activeY >= top) && (m_activeY < top + row.height());
            const int inset = active ? 3 : 12;
            constexpr int padding = 8;
            const auto metrics = painter.fontMetrics();
            const QString title = metrics.elidedText(text, Qt::ElideRight, qMax(0, width() - inset - 2 * padding));
            if (title.isEmpty())
                return;
            const qreal height = qMin(row.height() - 2, metrics.height() + 4);
            const int labelWidth = metrics.horizontalAdvance(title) + 2 * padding;
            const QRectF box(isRightToLeft() ? width() - inset - labelWidth : inset,
                center - height / 2, labelWidth, height);
            painter.setPen(Qt::NoPen);
            painter.setBrush(active ? QColor(72, 72, 72) : QColor(110, 110, 110));
            painter.drawRoundedRect(box, 4, 4);
            painter.setPen(Qt::white);
            painter.drawText(box, Qt::AlignCenter, title);
        };

        const auto sections = m_sidebar->findChildren<TransferListFiltersWidgetItem *>();
        for (auto *section : sections)
        {
            auto *caption = section->caption();
            label(caption, caption->rect(), caption->accessibleName());
            auto *view = section->view();
            if (!view->isVisible())
                continue;
            auto *viewport = view->viewport();
            const int top = viewport->mapTo(m_sidebar, QPoint()).y();
            const int end = qMin(viewport->height(), content.bottom() + 1 - top);
            for (int y = qMax(0, content.top() - top); y < end;)
            {
                const auto index = view->indexAt({viewport->width() / 2, y});
                if (!index.isValid())
                {
                    ++y;
                    continue;
                }
                const QRect row = view->visualRect(index);
                label(viewport, row, index.data().toString());
                y = qMax(y + 1, row.bottom() + 1);
            }
        }
    }

    TransferListFiltersWidget *m_sidebar;
    int m_activeY = 0;
};

TransferListFiltersWidget::TransferListFiltersWidget(QWidget *parent, TransferListWidget *transferList, const bool downloadFavicon)
    : QWidget(parent)
    , m_transferList {transferList}
{
    setBackgroundRole(QPalette::Base);

    Preferences *const pref = Preferences::instance();

    // Construct lists
    auto *mainWidget = new QWidget;
    auto *mainWidgetLayout = new QVBoxLayout(mainWidget);
    mainWidgetLayout->setContentsMargins(0, 2, 0, 0);
    mainWidgetLayout->setSpacing(2);
    mainWidgetLayout->setAlignment(Qt::AlignTop);

    {
        auto *statusFilterWidget = new StatusFilterWidget(this, transferList);
        auto *item = new TransferListFiltersWidgetItem(tr("Status"), statusFilterWidget, this);
        item->setChecked(pref->getStatusFilterState());
        connect(item, &TransferListFiltersWidgetItem::toggled, statusFilterWidget, &StatusFilterWidget::toggleFilter);
        connect(item, &TransferListFiltersWidgetItem::toggled, pref, &Preferences::setStatusFilterState);
        statusFilterWidget->toggleFilter(item->isChecked());
        mainWidgetLayout->addWidget(item);
    }

    {
        auto *categoryFilterWidget = new CategoryFilterWidget(this);
        connect(categoryFilterWidget, &CategoryFilterWidget::actionDeleteTorrentsTriggered
                , transferList, &TransferListWidget::deleteVisibleTorrents);
        connect(categoryFilterWidget, &CategoryFilterWidget::actionStopTorrentsTriggered
                , transferList, &TransferListWidget::stopVisibleTorrents);
        connect(categoryFilterWidget, &CategoryFilterWidget::actionStartTorrentsTriggered
                , transferList, &TransferListWidget::startVisibleTorrents);
        connect(categoryFilterWidget, &CategoryFilterWidget::actionForceStartTorrentsTriggered
                , transferList, &TransferListWidget::forceStartVisibleTorrents);
        connect(categoryFilterWidget, &CategoryFilterWidget::categoryChanged
                , transferList, &TransferListWidget::applyCategoryFilter);

        auto *item = new TransferListFiltersWidgetItem(tr("Categories"), categoryFilterWidget, this);
        item->setChecked(pref->getCategoryFilterState());
        connect(item, &TransferListFiltersWidgetItem::toggled, this, [this, categoryFilterWidget](const bool enabled)
        {
            m_transferList->applyCategoryFilter(enabled ? categoryFilterWidget->currentCategory() : QString());
        });
        connect(item, &TransferListFiltersWidgetItem::toggled, pref, &Preferences::setCategoryFilterState);
        mainWidgetLayout->addWidget(item);
    }

    {
        auto *tagFilterWidget = new TagFilterWidget(this);
        connect(tagFilterWidget, &TagFilterWidget::actionDeleteTorrentsTriggered
                , transferList, &TransferListWidget::deleteVisibleTorrents);
        connect(tagFilterWidget, &TagFilterWidget::actionStopTorrentsTriggered
                , transferList, &TransferListWidget::stopVisibleTorrents);
        connect(tagFilterWidget, &TagFilterWidget::actionStartTorrentsTriggered
                , transferList, &TransferListWidget::startVisibleTorrents);
        connect(tagFilterWidget, &TagFilterWidget::actionForceStartTorrentsTriggered
                , transferList, &TransferListWidget::forceStartVisibleTorrents);
        connect(tagFilterWidget, &TagFilterWidget::tagChanged
                , transferList, &TransferListWidget::applyTagFilter);

        auto *item = new TransferListFiltersWidgetItem(tr("Tags"), tagFilterWidget, this);
        item->setChecked(pref->getTagFilterState());
        connect(item, &TransferListFiltersWidgetItem::toggled, this, [this, tagFilterWidget](const bool enabled)
        {
            m_transferList->applyTagFilter(enabled ? tagFilterWidget->currentTag() : std::nullopt);
        });
        connect(item, &TransferListFiltersWidgetItem::toggled, pref, &Preferences::setTagFilterState);
        mainWidgetLayout->addWidget(item);
    }

    const int trackerStatusItemPos = mainWidgetLayout->count();

    {
        m_trackersFilterWidget = new TrackersFilterWidget(this, transferList, downloadFavicon);

        auto *item = new TransferListFiltersWidgetItem(tr("Trackers"), m_trackersFilterWidget, this);
        item->setChecked(pref->getTrackerFilterState());
        connect(item, &TransferListFiltersWidgetItem::toggled, m_trackersFilterWidget, &TrackersFilterWidget::toggleFilter);
        connect(item, &TransferListFiltersWidgetItem::toggled, pref, &Preferences::setTrackerFilterState);
        m_trackersFilterWidget->toggleFilter(item->isChecked());
        mainWidgetLayout->addWidget(item);
    }

    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setWidget(mainWidget);

    auto *vLayout = new QVBoxLayout(this);
    vLayout->setContentsMargins(0, 0, 0, 0);
    m_toggle = new QToolButton(this);
    m_toggle->setObjectName(u"filtersSidebarToggle"_s);
    m_toggle->setCheckable(true);
    m_toggle->setFocusPolicy(Qt::StrongFocus);
    m_toggle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    vLayout->setSpacing(0);
    vLayout->addWidget(m_toggle);
    vLayout->addWidget(m_scroll);

    m_labels = new Labels(this);
    m_dismissLabels = new QShortcut(Qt::Key_Escape, this);
    m_dismissLabels->setEnabled(false);
    connect(m_dismissLabels, &QShortcut::activated, m_labels, &QWidget::hide);
    m_labels->installEventFilter(this);
    connect(m_scroll->verticalScrollBar(), &QScrollBar::valueChanged, m_labels, qOverload<>(&QWidget::update));
    for (auto *section : findChildren<TransferListFiltersWidgetItem *>())
        prepareSection(section);

    const auto createTrackerStatusItem = [this, mainWidgetLayout, trackerStatusItemPos, pref]
    {
        auto *trackerStatusFilterWidget = new TrackerStatusFilterWidget(this, m_transferList);
        auto *item = new TransferListFiltersWidgetItem(tr("Tracker status"), trackerStatusFilterWidget, this);
        item->setChecked(pref->getTrackerStatusFilterState());
        connect(item, &TransferListFiltersWidgetItem::toggled, trackerStatusFilterWidget, &TrackerStatusFilterWidget::toggleFilter);
        connect(item, &TransferListFiltersWidgetItem::toggled, pref, &Preferences::setTrackerStatusFilterState);
        trackerStatusFilterWidget->toggleFilter(item->isChecked());
        mainWidgetLayout->insertWidget(trackerStatusItemPos, item);
        prepareSection(item);
    };

    const auto removeTrackerStatusItem = [mainWidgetLayout, trackerStatusItemPos]
    {
        QLayoutItem *layoutItem = mainWidgetLayout->takeAt(trackerStatusItemPos);
        delete layoutItem->widget();
        delete layoutItem;
    };

    m_useSeparateTrackerStatusFilter = pref->useSeparateTrackerStatusFilter();
    if (m_useSeparateTrackerStatusFilter)
        createTrackerStatusItem();

    connect(pref, &Preferences::changed, this, [this, pref, createTrackerStatusItem, removeTrackerStatusItem]
    {
        if (m_useSeparateTrackerStatusFilter == pref->useSeparateTrackerStatusFilter())
            return;

        m_useSeparateTrackerStatusFilter = !m_useSeparateTrackerStatusFilter;
        if (m_useSeparateTrackerStatusFilter)
            createTrackerStatusItem();
        else
            removeTrackerStatusItem();
    });

    auto *settings = SettingsStorage::instance();
    m_autoCompact = !settings->hasKey(COMPACT_KEY);
    m_toggle->setChecked(settings->loadValue<bool>(COMPACT_KEY, window()->width() < 760));
    setCompact(isCompact());
    connect(m_toggle, &QToolButton::toggled, this, &TransferListFiltersWidget::setCompact);
    connect(m_toggle, &QToolButton::clicked, this, [this](const bool checked)
    {
        m_autoCompact = false;
        SettingsStorage::instance()->storeValue(COMPACT_KEY, checked);
    });
    window()->installEventFilter(this);
}

void TransferListFiltersWidget::setDownloadTrackerFavicon(bool value)
{
    m_trackersFilterWidget->setDownloadTrackerFavicon(value);
}

TransferListFiltersWidget::~TransferListFiltersWidget()
{
    delete m_labels;
}

bool TransferListFiltersWidget::isCompact() const
{
    return m_toggle->isChecked();
}

void TransferListFiltersWidget::prepareSection(TransferListFiltersWidgetItem *section)
{
    auto *view = section->view();
    view->setMouseTracking(true);
    view->viewport()->installEventFilter(this);
    view->installEventFilter(this);
    section->caption()->installEventFilter(this);
    section->caption()->setMouseTracking(true);
    connect(view->model(), &QAbstractItemModel::dataChanged, m_labels, qOverload<>(&QWidget::update));
    section->setCompact(isCompact());
}

void TransferListFiltersWidget::setCompact(const bool compact)
{
    m_labels->hide();
    m_toggle->setToolTip(compact ? tr("Expand filters sidebar") : tr("Collapse filters sidebar"));
    m_toggle->setAccessibleName(m_toggle->toolTip());
    const bool pointsRight = compact != isRightToLeft();
    const auto icon = UIThemeManager::instance()->getIcon(pointsRight ? u"sidebar-expand"_s : u"sidebar-collapse"_s
            , pointsRight ? u"go-next"_s : u"go-previous"_s);
    m_toggle->setIcon(icon);
    m_toggle->setArrowType(icon.isNull() ? (pointsRight ? Qt::RightArrow : Qt::LeftArrow) : Qt::NoArrow);
    for (auto *section : findChildren<TransferListFiltersWidgetItem *>())
        section->setCompact(compact);

    if (compact)
        setFixedWidth(42 + style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, m_scroll));
    else
    {
        setMinimumWidth(0);
        setMaximumWidth(QWIDGETSIZE_MAX);
    }
    emit compactChanged(compact);
}

bool TransferListFiltersWidget::eventFilter(QObject *watched, QEvent *event)
{
    const auto type = event->type();
    if (watched == m_labels)
    {
        if ((type == QEvent::Hide) || (type == QEvent::Show))
            m_dismissLabels->setEnabled(type == QEvent::Show);
    }
    else if (watched == window())
    {
        if (type == QEvent::Resize)
        {
            m_labels->hide();
            if (m_autoCompact)
                m_toggle->setChecked(window()->width() < 760);
        }
        else if ((type == QEvent::WindowDeactivate) || (type == QEvent::Hide))
            m_labels->hide();
    }
    else if (isCompact())
    {
        auto *widget = qobject_cast<QWidget *>(watched);
        if ((type == QEvent::Enter) || (type == QEvent::MouseMove))
        {
            const auto *pointer = static_cast<QSinglePointEvent *>(event);
            m_labels->display(widget->mapTo(this, pointer->position().toPoint()).y());
        }
        else if ((type == QEvent::Leave) || (type == QEvent::FocusOut) || (type == QEvent::Hide)
                || (type == QEvent::ContextMenu) || (type == QEvent::MouseButtonPress))
            m_labels->hide();
        else if (type == QEvent::ToolTip)
            return true;
        else if ((type == QEvent::KeyPress) && m_labels->isVisible())
        {
            const auto *key = static_cast<QKeyEvent *>(event);
            if ((key->key() == Qt::Key_Escape) && (key->modifiers() == Qt::NoModifier))
            {
                m_labels->hide();
                return true;
            }
        }
        else if ((type == QEvent::FocusIn) || (type == QEvent::KeyRelease))
        {
            if ((type == QEvent::KeyRelease) && (static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape))
                return false;
            if (auto *view = qobject_cast<QAbstractItemView *>(widget); view && view->currentIndex().isValid())
                m_labels->display(view->viewport()->mapTo(this, view->visualRect(view->currentIndex()).center()).y());
            else if (qobject_cast<QCheckBox *>(widget))
                m_labels->display(widget->mapTo(this, widget->rect().center()).y());
        }
    }
    return QWidget::eventFilter(watched, event);
}
