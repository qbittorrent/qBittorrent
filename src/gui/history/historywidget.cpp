/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  qBittorrent contributors
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

#include "historywidget.h"

#include <QAction>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeView>
#include <QVBoxLayout>

#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/preferences.h"
#include "base/utils/fs.h"
#include "gui/deletionconfirmationdialog.h"
#include "gui/history/historymodel.h"
#include "gui/uithememanager.h"

using namespace Qt::Literals::StringLiterals;

HistoryWidget::HistoryWidget(QWidget *parent)
    : QWidget(parent)
    , m_model {new HistoryModel(this)}
    , m_view {new QTreeView(this)}
{
    m_view->setModel(m_model);
    m_view->setRootIsDecorated(false);
    m_view->setAlternatingRowColors(true);
    m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    m_view->header()->setStretchLastSection(true);
    m_view->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

    m_restoreButton = new QPushButton(UIThemeManager::instance()->getIcon(u"torrent-start"_s, u"media-playback-start"_s)
            , tr("&Restore"), this);
    m_exportButton = new QPushButton(UIThemeManager::instance()->getIcon(u"edit-copy"_s), tr("E&xport .torrent..."), this);
    m_deleteButton = new QPushButton(UIThemeManager::instance()->getIcon(u"list-remove"_s), tr("&Delete"), this);

    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(m_restoreButton);
    buttonLayout->addWidget(m_exportButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_deleteButton);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_view);
    layout->addLayout(buttonLayout);

    connect(m_view, &QWidget::customContextMenuRequested, this, &HistoryWidget::showContextMenu);
    connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged, this, &HistoryWidget::updateButtons);
    connect(m_restoreButton, &QPushButton::clicked, this, &HistoryWidget::restoreSelectedTorrents);
    connect(m_exportButton, &QPushButton::clicked, this, &HistoryWidget::exportSelectedTorrents);
    connect(m_deleteButton, &QPushButton::clicked, this, &HistoryWidget::deleteSelectedTorrents);

    const auto *const session = BitTorrent::Session::instance();
    connect(session, &BitTorrent::Session::archivedTorrentsLoaded, this, &HistoryWidget::reload);
    connect(session, &BitTorrent::Session::torrentArchived, this, &HistoryWidget::reload);
    connect(session, &BitTorrent::Session::torrentUnarchived, this, &HistoryWidget::reload);
    connect(session, &BitTorrent::Session::archivedTorrentRemoved, this, &HistoryWidget::reload);
    connect(session, &BitTorrent::Session::restored, this, &HistoryWidget::reload);
    connect(session, &BitTorrent::Session::torrentArchiveFailed, this, [this](const BitTorrent::TorrentID &, const QString &reason)
    {
        QMessageBox::warning(this, tr("Archive operation failed"), reason);
    });

    updateButtons();
}

HistoryWidget::~HistoryWidget() = default;

void HistoryWidget::reload()
{
    m_model->reload();
    updateButtons();
}

void HistoryWidget::updateButtons()
{
    const QList<BitTorrent::ArchivedTorrent> torrents = selectedTorrents();
    m_restoreButton->setEnabled(!torrents.isEmpty());
    m_exportButton->setEnabled(torrents.size() == 1);
    m_deleteButton->setEnabled(!torrents.isEmpty());
}

void HistoryWidget::restoreSelectedTorrents()
{
    const QList<BitTorrent::ArchivedTorrent> torrents = selectedTorrents();
    for (const BitTorrent::ArchivedTorrent &torrent : torrents)
        BitTorrent::Session::instance()->unarchiveTorrent(torrent.id);
}

void HistoryWidget::deleteSelectedTorrents()
{
    const QList<BitTorrent::ArchivedTorrent> torrents = selectedTorrents();
    if (torrents.isEmpty())
        return;

    const auto removeTorrents = [torrents](const BitTorrent::TorrentRemoveOption deleteOption)
    {
        for (const BitTorrent::ArchivedTorrent &torrent : torrents)
            BitTorrent::Session::instance()->removeArchivedTorrent(torrent.id, deleteOption);
    };

    if (Preferences::instance()->confirmTorrentDeletion())
    {
        auto *dialog = new DeletionConfirmationDialog(this, torrents.size(), torrents.first().name, false);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(dialog, &DeletionConfirmationDialog::accepted, this, [dialog, removeTorrents]
        {
            removeTorrents(dialog->isRemoveContentSelected()
                    ? BitTorrent::TorrentRemoveOption::RemoveContent
                    : BitTorrent::TorrentRemoveOption::KeepContent);
        });
        dialog->open();
    }
    else
    {
        removeTorrents(BitTorrent::TorrentRemoveOption::KeepContent);
    }
}

void HistoryWidget::exportSelectedTorrents()
{
    const QList<BitTorrent::ArchivedTorrent> torrents = selectedTorrents();
    if (torrents.size() != 1)
        return;

    const BitTorrent::ArchivedTorrent &torrent = torrents.first();
    const QByteArray torrentData = BitTorrent::Session::instance()->exportArchivedTorrent(torrent.id);
    if (torrentData.isEmpty())
    {
        QMessageBox::warning(this, tr("Export .torrent file error")
                , tr("Unable to export torrent file. Error: %1").arg(tr("Missing metadata")));
        return;
    }

    QString filename = Utils::Fs::toValidFileName(torrent.name, {});
    if (filename.isEmpty())
        filename = torrent.id.toString();
    filename += u".torrent"_s;

    const QString filePath = QFileDialog::getSaveFileName(this, tr("Export .torrent file"), filename
            , tr("Torrent files (*.torrent)"));
    if (filePath.isEmpty())
        return;

    QFile file {filePath};
    if (!file.open(QIODevice::WriteOnly) || (file.write(torrentData) != torrentData.size()))
    {
        QMessageBox::warning(this, tr("Export .torrent file error")
                , tr("Unable to write file: \"%1\"").arg(filePath));
    }
}

void HistoryWidget::showContextMenu(const QPoint &pos)
{
    const QModelIndex index = m_view->indexAt(pos);
    if (index.isValid())
        m_view->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);

    const QList<BitTorrent::ArchivedTorrent> torrents = selectedTorrents();
    if (torrents.isEmpty())
        return;

    auto *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->addAction(UIThemeManager::instance()->getIcon(u"torrent-start"_s, u"media-playback-start"_s)
            , tr("&Restore"), this, &HistoryWidget::restoreSelectedTorrents);
    if (torrents.size() == 1)
    {
        menu->addAction(UIThemeManager::instance()->getIcon(u"edit-copy"_s)
                , tr("E&xport .torrent..."), this, &HistoryWidget::exportSelectedTorrents);
    }
    menu->addSeparator();
    menu->addAction(UIThemeManager::instance()->getIcon(u"list-remove"_s)
            , tr("&Delete"), this, &HistoryWidget::deleteSelectedTorrents);
    menu->popup(m_view->viewport()->mapToGlobal(pos));
}

QList<BitTorrent::ArchivedTorrent> HistoryWidget::selectedTorrents() const
{
    QList<BitTorrent::ArchivedTorrent> torrents;
    const QModelIndexList selectedIndexes = m_view->selectionModel()->selectedRows();
    torrents.reserve(selectedIndexes.size());
    for (const QModelIndex &index : selectedIndexes)
    {
        if (const std::optional<BitTorrent::ArchivedTorrent> torrent = m_model->archivedTorrent(index))
            torrents.append(*torrent);
    }

    return torrents;
}
