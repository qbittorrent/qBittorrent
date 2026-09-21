/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026 Tim Sylvester <t.j.sylvester@gmail.com>
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

#include "unmatchedtorrentsdialog.h"

#include <QFileDialog>
#include <QListWidget>
#include <QPushButton>

#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/path.h"
#include "ui_unmatchedtorrentsdialog.h"

UnmatchedTorrentsDialog::UnmatchedTorrentsDialog(QWidget *parent, const QList<BitTorrent::TorrentID> &torrentIDs)
    : QDialog(parent)
    , m_ui {new Ui::UnmatchedTorrentsDialog}
{
    m_ui->setupUi(this);

    for (const BitTorrent::TorrentID &id : torrentIDs)
    {
        const BitTorrent::Torrent *torrent = BitTorrent::Session::instance()->getTorrent(id);
        if (!torrent)
            continue;

        m_torrentIDs.append(id);
        m_ui->listTorrents->addItem(torrent->name());
    }

    m_ui->listTorrents->setCurrentRow(0);

    QPushButton *setLocationButton = m_ui->buttonBox->addButton(tr("Set location..."), QDialogButtonBox::ActionRole);
    connect(setLocationButton, &QAbstractButton::clicked, this, &UnmatchedTorrentsDialog::setCurrentTorrentLocation);
    connect(m_ui->listTorrents, &QListWidget::itemDoubleClicked, this, &UnmatchedTorrentsDialog::setCurrentTorrentLocation);
    connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_ui->listTorrents, &QListWidget::currentRowChanged, this
            , [setLocationButton](const int currentRow) { setLocationButton->setEnabled(currentRow >= 0); });
    setLocationButton->setEnabled(m_ui->listTorrents->currentRow() >= 0);

    connect(BitTorrent::Session::instance(), &BitTorrent::Session::torrentAboutToBeRemoved, this
            , [this](const BitTorrent::Torrent *torrent) { removeTorrent(torrent->id()); });
}

UnmatchedTorrentsDialog::~UnmatchedTorrentsDialog()
{
    delete m_ui;
}

bool UnmatchedTorrentsDialog::isEmpty() const
{
    return m_torrentIDs.isEmpty();
}

void UnmatchedTorrentsDialog::setCurrentTorrentLocation()
{
    const int row = m_ui->listTorrents->currentRow();
    if (row < 0)
        return;

    const BitTorrent::TorrentID id = m_torrentIDs.value(row);
    const BitTorrent::Torrent *torrent = BitTorrent::Session::instance()->getTorrent(id);
    if (!torrent)
    {
        removeTorrent(id);
        return;
    }

    auto *fileDialog = new QFileDialog(this, tr("Choose save path"), torrent->savePath().data());
    fileDialog->setAttribute(Qt::WA_DeleteOnClose);
    fileDialog->setFileMode(QFileDialog::Directory);
    fileDialog->setOptions(QFileDialog::DontConfirmOverwrite | QFileDialog::ShowDirsOnly | QFileDialog::HideNameFilterDetails);
    connect(fileDialog, &QDialog::accepted, this, [this, fileDialog, id]()
    {
        const Path newLocation {fileDialog->selectedFiles().constFirst()};
        if (!newLocation.exists())
            return;

        if (m_torrentIDs.contains(id))
        {
            BitTorrent::Session::instance()->assignTorrentLocation(id, newLocation);
            removeTorrent(id);
        }
    });

    fileDialog->open();
}

void UnmatchedTorrentsDialog::removeTorrent(const BitTorrent::TorrentID &id)
{
    const int row = m_torrentIDs.indexOf(id);
    if (row < 0)
        return;

    delete m_ui->listTorrents->takeItem(row);
    m_torrentIDs.removeAt(row);

    if (m_torrentIDs.isEmpty())
        accept();
}
