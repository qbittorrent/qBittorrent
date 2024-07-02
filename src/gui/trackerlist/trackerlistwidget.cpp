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

#include "trackerlistwidget.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QDebug>
#include <QHeaderView>
#include <QList>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QShortcut>
#include <QStringList>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QWheelEvent>

#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/bittorrent/trackerentrystatus.h"
#include "base/global.h"
#include "base/preferences.h"
#include "gui/autoexpandabledialog.h"
#include "gui/trackersadditiondialog.h"
#include "gui/uithememanager.h"
#include "trackerlistitemdelegate.h"
#include "trackerlistmodel.h"
#include "trackerlistsortmodel.h"

TrackerListWidget::TrackerListWidget(QWidget *parent)
    : QTreeView(parent)
{
#ifdef QBT_USES_LIBTORRENT2
    setColumnHidden(TrackerListModel::COL_PROTOCOL, true); // Must be set before calling loadSettings()
#endif

    setExpandsOnDoubleClick(false);
    setAllColumnsShowFocus(true);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSortingEnabled(true);
    setUniformRowHeights(true);
    setContextMenuPolicy(Qt::CustomContextMenu);

    header()->setSortIndicator(0, Qt::AscendingOrder);
    header()->setFirstSectionMovable(true);
    header()->setStretchLastSection(false); // Must be set after loadSettings() in order to work
    header()->setTextElideMode(Qt::ElideRight);
    header()->setContextMenuPolicy(Qt::CustomContextMenu);

    m_model = new TrackerListModel(BitTorrent::Session::instance(), this);
    auto *sortModel = new TrackerListSortModel(m_model, this);
    QTreeView::setModel(sortModel);

    setItemDelegate(new TrackerListItemDelegate(this));

    loadSettings();

    // Ensure that at least one column is visible at all times
    if (visibleColumnsCount() == 0)
        setColumnHidden(TrackerListModel::COL_URL, false);
    // To also mitigate the above issue, we have to resize each column when
    // its size is 0, because explicitly 'showing' the column isn't enough
    // in the above scenario.
    for (int i = 0; i < TrackerListModel::COL_COUNT; ++i)
    {
        if ((columnWidth(i) <= 0) && !isColumnHidden(i))
            resizeColumnToContents(i);
    }

    connect(this, &QWidget::customContextMenuRequested, this, &TrackerListWidget::showTrackerListMenu);
    connect(header(), &QWidget::customContextMenuRequested, this, &TrackerListWidget::displayColumnHeaderMenu);
    connect(header(), &QHeaderView::sectionMoved, this, &TrackerListWidget::saveSettings);
    connect(header(), &QHeaderView::sectionResized, this, &TrackerListWidget::saveSettings);
    connect(header(), &QHeaderView::sortIndicatorChanged, this, &TrackerListWidget::saveSettings);

    // Set hotkeys
    const auto *editHotkey = new QShortcut(Qt::Key_F2, this, nullptr, nullptr, Qt::WidgetShortcut);
    connect(editHotkey, &QShortcut::activated, this, &TrackerListWidget::editSelectedTracker);
    const auto *deleteHotkey = new QShortcut(QKeySequence::Delete, this, nullptr, nullptr, Qt::WidgetShortcut);
    connect(deleteHotkey, &QShortcut::activated, this, &TrackerListWidget::deleteSelectedTrackers);
    const auto *copyHotkey = new QShortcut(QKeySequence::Copy, this, nullptr, nullptr, Qt::WidgetShortcut);
    connect(copyHotkey, &QShortcut::activated, this, &TrackerListWidget::copyTrackerUrl);

    connect(this, &QAbstractItemView::doubleClicked, this, &TrackerListWidget::editSelectedTracker);
}

TrackerListWidget::~TrackerListWidget()
{
    saveSettings();
}

void TrackerListWidget::setTorrent(BitTorrent::Torrent *torrent)
{
    m_model->setTorrent(torrent);
}

BitTorrent::Torrent *TrackerListWidget::torrent() const
{
    return m_model->torrent();
}

QModelIndexList TrackerListWidget::getSelectedTrackerRows() const
{
    QModelIndexList selectedItemIndexes = selectionModel()->selectedRows();
    selectedItemIndexes.removeIf([](const QModelIndex &index)
    {
        return (index.parent().isValid() || (index.row() < TrackerListModel::STICKY_ROW_COUNT));
    });

    return selectedItemIndexes;
}

void TrackerListWidget::decreaseSelectedTrackerTiers()
{
    const QModelIndexList trackerIndexes = getSelectedTrackerRows();
    if (trackerIndexes.isEmpty())
        return;

    QSet<QString> trackerURLs;
    trackerURLs.reserve(trackerIndexes.size());
    for (const QModelIndex &index : trackerIndexes)
        trackerURLs.insert(index.siblingAtColumn(TrackerListModel::COL_URL).data().toString());

    const QList<BitTorrent::TrackerEntryStatus> trackers = m_model->torrent()->trackers();
    QList<BitTorrent::TrackerEntry> adjustedTrackers;
    adjustedTrackers.reserve(trackers.size());

    for (const BitTorrent::TrackerEntryStatus &status : trackers)
    {
        BitTorrent::TrackerEntry entry
        {
            .url = status.url,
            .tier = status.tier
        };
        if (trackerURLs.contains(entry.url))
        {
            if (entry.tier > 0)
                --entry.tier;
        }
        adjustedTrackers.append(entry);
    }

    m_model->torrent()->replaceTrackers(adjustedTrackers);
}

void TrackerListWidget::increaseSelectedTrackerTiers()
{
    const QModelIndexList trackerIndexes = getSelectedTrackerRows();
    if (trackerIndexes.isEmpty())
        return;

    QSet<QString> trackerURLs;
    trackerURLs.reserve(trackerIndexes.size());
    for (const QModelIndex &index : trackerIndexes)
        trackerURLs.insert(index.siblingAtColumn(TrackerListModel::COL_URL).data().toString());

    const QList<BitTorrent::TrackerEntryStatus> trackers = m_model->torrent()->trackers();
    QList<BitTorrent::TrackerEntry> adjustedTrackers;
    adjustedTrackers.reserve(trackers.size());

    for (const BitTorrent::TrackerEntryStatus &status : trackers)
    {
        BitTorrent::TrackerEntry entry
        {
            .url = status.url,
            .tier = status.tier
        };
        if (trackerURLs.contains(entry.url))
        {
            if (entry.tier < std::numeric_limits<decltype(entry.tier)>::max())
                ++entry.tier;
        }
        adjustedTrackers.append(entry);
    }

    m_model->torrent()->replaceTrackers(adjustedTrackers);
}

void TrackerListWidget::openAddTrackersDialog()
{
    if (!torrent())
        return;

    auto *dialog = new TrackersAdditionDialog(this, torrent());
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->open();
}

void TrackerListWidget::copyTrackerUrl()
{
    if (!torrent())
        return;

    const QModelIndexList selectedTrackerIndexes = getSelectedTrackerRows();
    if (selectedTrackerIndexes.isEmpty())
        return;

    QStringList urlsToCopy;
    for (const QModelIndex &index : selectedTrackerIndexes)
    {
        const QString &trackerURL = index.siblingAtColumn(TrackerListModel::COL_URL).data().toString();
        qDebug() << "Copy:" << qUtf8Printable(trackerURL);
        urlsToCopy.append(trackerURL);
    }

    QApplication::clipboard()->setText(urlsToCopy.join(u'\n'));
}


void TrackerListWidget::deleteSelectedTrackers()
{
    if (!torrent())
        return;

    const QModelIndexList selectedTrackerIndexes = getSelectedTrackerRows();
    if (selectedTrackerIndexes.isEmpty())
        return;

    QStringList urlsToRemove;
    for (const QModelIndex &index : selectedTrackerIndexes)
    {
        const QString trackerURL = index.siblingAtColumn(TrackerListModel::COL_URL).data().toString();
        urlsToRemove.append(trackerURL);
    }

    torrent()->removeTrackers(urlsToRemove);
}

void TrackerListWidget::editSelectedTracker()
{
    if (!torrent())
        return;

    const QModelIndexList selectedTrackerIndexes = getSelectedTrackerRows();
    if (selectedTrackerIndexes.isEmpty())
        return;

    // During multi-select only process item selected last
    const QUrl trackerURL = selectedTrackerIndexes.last().siblingAtColumn(TrackerListModel::COL_URL).data().toString();

    bool ok = false;
    const QUrl newTrackerURL = AutoExpandableDialog::getText(this
            , tr("Tracker editing"), tr("Tracker URL:")
            , QLineEdit::Normal, trackerURL.toString(), &ok).trimmed();
    if (!ok)
        return;

    if (!newTrackerURL.isValid())
    {
        QMessageBox::warning(this, tr("Tracker editing failed"), tr("The tracker URL entered is invalid."));
        return;
    }

    if (newTrackerURL == trackerURL)
        return;

    const QList<BitTorrent::TrackerEntryStatus> trackers = torrent()->trackers();
    QList<BitTorrent::TrackerEntry> entries;
    entries.reserve(trackers.size());

    bool match = false;
    for (const BitTorrent::TrackerEntryStatus &status : trackers)
    {
        const QUrl url {status.url};

        if (newTrackerURL == url)
        {
            QMessageBox::warning(this, tr("Tracker editing failed"), tr("The tracker URL already exists."));
            return;
        }

        BitTorrent::TrackerEntry entry
        {
            .url = status.url,
            .tier = status.tier
        };

        if (!match && (trackerURL == url))
        {
            match = true;
            entry.url = newTrackerURL.toString();
        }
        entries.append(entry);
    }

    torrent()->replaceTrackers(entries);
}

void TrackerListWidget::reannounceSelected()
{
    if (!torrent())
        return;

    const auto &selectedItemIndexes = selectedIndexes();
    if (selectedItemIndexes.isEmpty())
        return;

    QSet<QString> trackerURLs;
    for (const QModelIndex &index : selectedItemIndexes)
    {
        if (index.parent().isValid())
            continue;

        if ((index.row() < TrackerListModel::STICKY_ROW_COUNT))
        {
            // DHT case
            if (index.row() == TrackerListModel::ROW_DHT)
                torrent()->forceDHTAnnounce();

            continue;
        }

        trackerURLs.insert(index.siblingAtColumn(TrackerListModel::COL_URL).data().toString());
    }

    const QList<BitTorrent::TrackerEntryStatus> &trackers = m_model->torrent()->trackers();
    for (qsizetype i = 0; i < trackers.size(); ++i)
    {
        const BitTorrent::TrackerEntryStatus &status = trackers.at(i);
        if (trackerURLs.contains(status.url))
            torrent()->forceReannounce(i);
    }
}

void TrackerListWidget::showTrackerListMenu()
{
    if (!torrent())
        return;

    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    // Add actions
    menu->addAction(UIThemeManager::instance()->getIcon(u"list-add"_s), tr("Add trackers...")
            , this, &TrackerListWidget::openAddTrackersDialog);

    if (!getSelectedTrackerRows().isEmpty())
    {
        menu->addAction(UIThemeManager::instance()->getIcon(u"edit-rename"_s),tr("Edit tracker URL...")
                , this, &TrackerListWidget::editSelectedTracker);
        menu->addAction(UIThemeManager::instance()->getIcon(u"edit-clear"_s, u"list-remove"_s), tr("Remove tracker")
                , this, &TrackerListWidget::deleteSelectedTrackers);
        menu->addAction(UIThemeManager::instance()->getIcon(u"edit-copy"_s), tr("Copy tracker URL")
                , this, &TrackerListWidget::copyTrackerUrl);
        if (!torrent()->isStopped())
        {
            menu->addAction(UIThemeManager::instance()->getIcon(u"reannounce"_s, u"view-refresh"_s), tr("Force reannounce to selected trackers")
                    , this, &TrackerListWidget::reannounceSelected);
        }
    }

    if (!torrent()->isStopped())
    {
        menu->addSeparator();
        menu->addAction(UIThemeManager::instance()->getIcon(u"reannounce"_s, u"view-refresh"_s), tr("Force reannounce to all trackers")
                , this, [this]()
        {
            torrent()->forceReannounce();
            torrent()->forceDHTAnnounce();
        });
    }

    menu->popup(QCursor::pos());
}

void TrackerListWidget::setModel([[maybe_unused]] QAbstractItemModel *model)
{
    Q_ASSERT_X(false, Q_FUNC_INFO, "Changing the model of TrackerListWidget is not allowed.");
}

void TrackerListWidget::loadSettings()
{
    header()->restoreState(Preferences::instance()->getTrackerListState());
}

void TrackerListWidget::saveSettings() const
{
    Preferences::instance()->setTrackerListState(header()->saveState());
}

int TrackerListWidget::visibleColumnsCount() const
{
    int count = 0;
    for (int i = 0, iMax = header()->count(); i < iMax; ++i)
    {
        if (!isColumnHidden(i))
            ++count;
    }

    return count;
}

void TrackerListWidget::displayColumnHeaderMenu()
{
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setTitle(tr("Column visibility"));
    menu->setToolTipsVisible(true);

    for (int i = 0; i < TrackerListModel::COL_COUNT; ++i)
    {
        QAction *action = menu->addAction(model()->headerData(i, Qt::Horizontal).toString(), this
                , [this, i](const bool checked)
        {
            if (!checked && (visibleColumnsCount() <= 1))
                return;

            setColumnHidden(i, !checked);

            if (checked && (columnWidth(i) <= 5))
                resizeColumnToContents(i);

            saveSettings();
        });
        action->setCheckable(true);
        action->setChecked(!isColumnHidden(i));
    }

    menu->addSeparator();
    QAction *resizeAction = menu->addAction(tr("Resize columns"), this, [this]()
    {
        for (int i = 0, count = header()->count(); i < count; ++i)
        {
            if (!isColumnHidden(i))
                resizeColumnToContents(i);
        }
        saveSettings();
    });
    resizeAction->setToolTip(tr("Resize all non-hidden columns to the size of their contents"));

    menu->popup(QCursor::pos());
}

void TrackerListWidget::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ShiftModifier)
    {
        // Shift + scroll = horizontal scroll
        event->accept();
        QWheelEvent scrollHEvent {event->position(), event->globalPosition()
                , event->pixelDelta(), event->angleDelta().transposed(), event->buttons()
                , event->modifiers(), event->phase(), event->inverted(), event->source()};
        QTreeView::wheelEvent(&scrollHEvent);
        return;
    }

    QTreeView::wheelEvent(event);  // event delegated to base class
}
