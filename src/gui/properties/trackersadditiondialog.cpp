/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2022  Mike Tzou (Chocobo1)
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

#include "trackersadditiondialog.h"

#include <QMessageBox>
#include <QSize>
#include <QStringView>
#include <QVector>

#include "base/bittorrent/torrent.h"
#include "base/bittorrent/trackerentry.h"
#include "base/global.h"
#include "base/net/downloadmanager.h"
#include "gui/uithememanager.h"
#include "ui_trackersadditiondialog.h"

#define SETTINGS_KEY(name) u"AddTrackersDialog/" name

TrackersAdditionDialog::TrackersAdditionDialog(QWidget *parent, BitTorrent::Torrent *const torrent)
    : QDialog(parent)
    , m_ui(new Ui::TrackersAdditionDialog)
    , m_torrent(torrent)
    , m_storeDialogSize(SETTINGS_KEY(u"Size"_qs))
    , m_storeTrackersListURL(SETTINGS_KEY(u"TrackersListURL"_qs))
{
    m_ui->setupUi(this);

    m_ui->downloadButton->setIcon(UIThemeManager::instance()->getIcon(u"downloading"_qs));
    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Add"));

    connect(m_ui->downloadButton, &QAbstractButton::clicked, this, &TrackersAdditionDialog::onDownloadButtonClicked);
    connect(this, &QDialog::accepted, this, &TrackersAdditionDialog::onAccepted);

    loadSettings();
}

TrackersAdditionDialog::~TrackersAdditionDialog()
{
    saveSettings();

    delete m_ui;
}

void TrackersAdditionDialog::onAccepted() const
{
    const QVector<BitTorrent::TrackerEntry> entries = BitTorrent::parseTrackerEntries(m_ui->textEditTrackersList->toPlainText());
    m_torrent->addTrackers(entries);
}

void TrackersAdditionDialog::onDownloadButtonClicked()
{
    const QString url = m_ui->lineEditListURL->text();
    if (url.isEmpty())
    {
        QMessageBox::warning(this, tr("Trackers list URL error"), tr("The trackers list URL cannot be empty"));
        return;
    }

    // Just to show that it takes times
    m_ui->downloadButton->setEnabled(false);
    setCursor(Qt::WaitCursor);

    Net::DownloadManager::instance()->download(url, this, &TrackersAdditionDialog::onTorrentListDownloadFinished);
}

void TrackersAdditionDialog::onTorrentListDownloadFinished(const Net::DownloadResult &result)
{
    // Restore the cursor, buttons...
    m_ui->downloadButton->setEnabled(true);
    setCursor(Qt::ArrowCursor);

    if (result.status != Net::DownloadStatus::Success)
    {
        QMessageBox::warning(this, tr("Download trackers list error")
            , tr("Error occurred when downloading the trackers list. Reason: \"%1\"").arg(result.errorString));
        return;
    }

    // Add fetched trackers to the list
    const QString existingText = m_ui->textEditTrackersList->toPlainText();
    if (!existingText.isEmpty() && !existingText.endsWith(u'\n'))
        m_ui->textEditTrackersList->insertPlainText(u"\n"_qs);

    // append the data as-is
    const auto trackers = QString::fromUtf8(result.data).trimmed();
    m_ui->textEditTrackersList->insertPlainText(trackers);
}

void TrackersAdditionDialog::saveSettings()
{
    m_storeDialogSize = size();
    m_storeTrackersListURL = m_ui->lineEditListURL->text();
}

void TrackersAdditionDialog::loadSettings()
{
    if (const QSize dialogSize = m_storeDialogSize; dialogSize.isValid())
        resize(dialogSize);
    m_ui->lineEditListURL->setText(m_storeTrackersListURL);
}
