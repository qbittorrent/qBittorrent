/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2023  Vladimir Golovnev <glassez@yandex.ru>
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

#include <QIcon>
#include <QListWidgetItem>
#include <QMenu>
#include <QScrollArea>
#include <QUrl>
#include <QVBoxLayout>

#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/bittorrent/trackerentrystatus.h"
#include "base/global.h"
#include "base/logger.h"
#include "base/net/downloadmanager.h"
#include "base/preferences.h"
#include "base/torrentfilter.h"
#include "base/utils/compare.h"
#include "base/utils/fs.h"
#include "transferlistfilters/categoryfilterwidget.h"
#include "transferlistfilters/statusfilterwidget.h"
#include "transferlistfilters/tagfilterwidget.h"
#include "transferlistfilters/trackersfilterwidget.h"
#include "transferlistfilterswidgetitem.h"
#include "transferlistwidget.h"
#include "uithememanager.h"
#include "utils.h"

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
    mainWidgetLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    {
        auto *item = new TransferListFiltersWidgetItem(tr("Status"), new StatusFilterWidget(this, transferList), this);
        item->setChecked(pref->getStatusFilterState());
        connect(item, &TransferListFiltersWidgetItem::toggled, pref, &Preferences::setStatusFilterState);
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

    {
        m_trackersFilterWidget = new TrackersFilterWidget(this, transferList, downloadFavicon);

        auto *item = new TransferListFiltersWidgetItem(tr("Trackers"), m_trackersFilterWidget, this);
        item->setChecked(pref->getTrackerFilterState());
        connect(item, &TransferListFiltersWidgetItem::toggled, pref, &Preferences::setTrackerFilterState);
        mainWidgetLayout->addWidget(item);
    }

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(mainWidget);

    auto *vLayout = new QVBoxLayout(this);
    vLayout->setContentsMargins(0, 0, 0, 0);
    vLayout->addWidget(scroll);
}

void TransferListFiltersWidget::setDownloadTrackerFavicon(bool value)
{
    m_trackersFilterWidget->setDownloadTrackerFavicon(value);
}

void TransferListFiltersWidget::addTrackers(const BitTorrent::Torrent *torrent, const QList<BitTorrent::TrackerEntry> &trackers)
{
    m_trackersFilterWidget->addTrackers(torrent, trackers);
}

void TransferListFiltersWidget::removeTrackers(const BitTorrent::Torrent *torrent, const QStringList &trackers)
{
    m_trackersFilterWidget->removeTrackers(torrent, trackers);
}

void TransferListFiltersWidget::refreshTrackers(const BitTorrent::Torrent *torrent)
{
    m_trackersFilterWidget->refreshTrackers(torrent);
}

void TransferListFiltersWidget::trackerEntryStatusesUpdated(const BitTorrent::Torrent *torrent
        , const QHash<QString, BitTorrent::TrackerEntryStatus> &updatedTrackers)
{
    m_trackersFilterWidget->handleTrackerStatusesUpdated(torrent, updatedTrackers);
}
