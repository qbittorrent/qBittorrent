/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017  Mike Tzou (Chocobo1)
 * Copyright (C) 2010  Christophe Dumez <chris@qbittorrent.org>
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

#include "torrentcreatordlg.h"

#include <QCloseEvent>
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QMimeData>
#include <QUrl>

#include "base/bittorrent/session.h"
#include "base/bittorrent/torrentcreatorthread.h"
#include "base/bittorrent/torrentinfo.h"
#include "base/global.h"
#include "base/utils/fs.h"

#include "ui_torrentcreatordlg.h"

#define SETTINGS_KEY(name) "TorrentCreator/" name

TorrentCreatorDlg::TorrentCreatorDlg(QWidget *parent, const QString &defaultPath)
    : QDialog(parent)
    , m_ui(new Ui::TorrentCreatorDlg)
    , m_creatorThread(new BitTorrent::TorrentCreatorThread(this))
    , m_storeDialogSize(SETTINGS_KEY("Dimension"))
    , m_storePieceSize(SETTINGS_KEY("PieceSize"))
    , m_storePrivateTorrent(SETTINGS_KEY("PrivateTorrent"))
    , m_storeStartSeeding(SETTINGS_KEY("StartSeeding"))
    , m_storeIgnoreRatio(SETTINGS_KEY("IgnoreRatio"))
    , m_storeLastAddPath(SETTINGS_KEY("LastAddPath"), QDir::homePath())
    , m_storeTrackerList(SETTINGS_KEY("TrackerList"))
    , m_storeWebSeedList(SETTINGS_KEY("WebSeedList"))
    , m_storeComments(SETTINGS_KEY("Comments"))
    , m_storeLastSavePath(SETTINGS_KEY("LastSavePath"), QDir::homePath())
{
    m_ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    setModal(false);

    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Create Torrent"));

    connect(m_ui->addFileButton, SIGNAL(clicked(bool)), SLOT(onAddFileButtonClicked()));
    connect(m_ui->addFolderButton, SIGNAL(clicked(bool)), SLOT(onAddFolderButtonClicked()));
    connect(m_ui->buttonBox, SIGNAL(accepted()), SLOT(onCreateButtonClicked()));
    connect(m_ui->buttonCalcTotalPieces, &QAbstractButton::clicked, this, &TorrentCreatorDlg::updatePiecesCount);

    connect(m_creatorThread, SIGNAL(creationSuccess(QString, QString)), this, SLOT(handleCreationSuccess(QString, QString)));
    connect(m_creatorThread, SIGNAL(creationFailure(QString)), this, SLOT(handleCreationFailure(QString)));
    connect(m_creatorThread, SIGNAL(updateProgress(int)), this, SLOT(updateProgressBar(int)));

    loadSettings();
    updateInputPath(defaultPath);

    show();
}

TorrentCreatorDlg::~TorrentCreatorDlg()
{
    saveSettings();

    if (m_creatorThread)
        delete m_creatorThread;

    delete m_ui;
}

void TorrentCreatorDlg::updateInputPath(const QString &path)
{
    if (path.isEmpty()) return;
    m_ui->textInputPath->setText(Utils::Fs::toNativePath(path));
    updateProgressBar(0);
}

void TorrentCreatorDlg::onAddFolderButtonClicked()
{
    QString oldPath = m_ui->textInputPath->text();
    QString path = QFileDialog::getExistingDirectory(this, tr("Select folder"), oldPath);
    updateInputPath(path);
}

void TorrentCreatorDlg::onAddFileButtonClicked()
{
    QString oldPath = m_ui->textInputPath->text();
    QString path = QFileDialog::getOpenFileName(this, tr("Select file"), oldPath);
    updateInputPath(path);
}

int TorrentCreatorDlg::getPieceSize() const
{
    const int pieceSizes[] = {0, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384};  // base unit in KiB
    return pieceSizes[m_ui->comboPieceSize->currentIndex()] * 1024;
}

void TorrentCreatorDlg::dropEvent(QDropEvent *event)
{
    event->acceptProposedAction();

    if (event->mimeData()->hasUrls()) {
        // only take the first one
        QUrl firstItem = event->mimeData()->urls().first();
        QString path = (firstItem.scheme().compare("file", Qt::CaseInsensitive) == 0)
                       ? firstItem.toLocalFile() : firstItem.toString();
        updateInputPath(path);
    }
}

void TorrentCreatorDlg::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat("text/plain") || event->mimeData()->hasFormat("text/uri-list"))
        event->acceptProposedAction();
}

// Main function that create a .torrent file
void TorrentCreatorDlg::onCreateButtonClicked()
{
    QString input = Utils::Fs::fromNativePath(m_ui->textInputPath->text()).trimmed();

    // test if readable
    const QFileInfo fi(input);
    if (!fi.isReadable()) {
        QMessageBox::critical(this, tr("Torrent creator failed"), tr("Reason: Path to file/folder is not readable."));
        return;
    }
    input = fi.canonicalFilePath();

    // get save path
    const QString savePath = QString(m_storeLastSavePath) + QLatin1Char('/') + fi.fileName() + QLatin1String(".torrent");
    QString destination = QFileDialog::getSaveFileName(this, tr("Select where to save the new torrent"), savePath, tr("Torrent Files (*.torrent)"));
    if (destination.isEmpty())
        return;
    if (!destination.endsWith(C_TORRENT_FILE_EXTENSION, Qt::CaseInsensitive))
        destination += C_TORRENT_FILE_EXTENSION;
    m_storeLastSavePath = Utils::Fs::branchPath(destination);

    // Disable dialog & set busy cursor
    setInteractionEnabled(false);
    setCursor(QCursor(Qt::WaitCursor));

    QStringList trackers = m_ui->trackersList->toPlainText().split("\n");
    QStringList urlSeeds = m_ui->URLSeedsList->toPlainText().split("\n");
    QString comment = m_ui->txtComment->toPlainText();

    // run the creator thread
    m_creatorThread->create(input, destination, trackers, urlSeeds, comment, m_ui->checkPrivate->isChecked(), getPieceSize());
}

void TorrentCreatorDlg::handleCreationFailure(const QString &msg)
{
    // Remove busy cursor
    setCursor(QCursor(Qt::ArrowCursor));
    QMessageBox::information(this, tr("Torrent creator failed"), tr("Reason: %1").arg(msg));
    setInteractionEnabled(true);
}

void TorrentCreatorDlg::handleCreationSuccess(const QString &path, const QString &branchPath)
{
    // Remove busy cursor
    setCursor(QCursor(Qt::ArrowCursor));
    if (m_ui->checkStartSeeding->isChecked()) {
        // Create save path temp data
        BitTorrent::TorrentInfo t = BitTorrent::TorrentInfo::loadFromFile(Utils::Fs::toNativePath(path));
        if (!t.isValid()) {
            QMessageBox::critical(this, tr("Torrent creator failed"), tr("Reason: Created torrent is invalid. It won't be added to download list."));
            return;
        }

        BitTorrent::AddTorrentParams params;
        params.savePath = branchPath;
        params.skipChecking = true;
        params.ignoreShareLimits = m_ui->checkIgnoreShareLimits->isChecked();

        BitTorrent::Session::instance()->addTorrent(t, params);
    }
    QMessageBox::information(this, tr("Torrent creator"), QString("%1\n%2").arg(tr("Create torrent success:")).arg(Utils::Fs::toNativePath(path)));
    setInteractionEnabled(true);
}

void TorrentCreatorDlg::updateProgressBar(int progress)
{
    m_ui->progressBar->setValue(progress);
}

void TorrentCreatorDlg::updatePiecesCount()
{
    const QString path = m_ui->textInputPath->text().trimmed();

    const int count = BitTorrent::TorrentCreatorThread::calculateTotalPieces(path, getPieceSize());
    m_ui->labelTotalPieces->setText(QString::number(count));
}

void TorrentCreatorDlg::setInteractionEnabled(bool enabled)
{
    m_ui->textInputPath->setEnabled(enabled);
    m_ui->addFileButton->setEnabled(enabled);
    m_ui->addFolderButton->setEnabled(enabled);
    m_ui->trackersList->setEnabled(enabled);
    m_ui->URLSeedsList->setEnabled(enabled);
    m_ui->txtComment->setEnabled(enabled);
    m_ui->comboPieceSize->setEnabled(enabled);
    m_ui->checkPrivate->setEnabled(enabled);
    m_ui->checkStartSeeding->setEnabled(enabled);
    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(enabled);
    m_ui->checkIgnoreShareLimits->setEnabled(enabled && m_ui->checkStartSeeding->isChecked());
}

void TorrentCreatorDlg::saveSettings()
{
    m_storeLastAddPath = m_ui->textInputPath->text().trimmed();

    m_storePieceSize = m_ui->comboPieceSize->currentIndex();
    m_storePrivateTorrent = m_ui->checkPrivate->isChecked();
    m_storeStartSeeding = m_ui->checkStartSeeding->isChecked();
    m_storeIgnoreRatio = m_ui->checkIgnoreShareLimits->isChecked();

    m_storeTrackerList = m_ui->trackersList->toPlainText();
    m_storeWebSeedList = m_ui->URLSeedsList->toPlainText();
    m_storeComments = m_ui->txtComment->toPlainText();

    m_storeDialogSize = size();
}

void TorrentCreatorDlg::loadSettings()
{
    m_ui->textInputPath->setText(m_storeLastAddPath);

    m_ui->comboPieceSize->setCurrentIndex(m_storePieceSize);
    m_ui->checkPrivate->setChecked(m_storePrivateTorrent);
    m_ui->checkStartSeeding->setChecked(m_storeStartSeeding);
    m_ui->checkIgnoreShareLimits->setChecked(m_storeIgnoreRatio);
    m_ui->checkIgnoreShareLimits->setEnabled(m_ui->checkStartSeeding->isChecked());

    m_ui->trackersList->setPlainText(m_storeTrackerList);
    m_ui->URLSeedsList->setPlainText(m_storeWebSeedList);
    m_ui->txtComment->setPlainText(m_storeComments);

    if (m_storeDialogSize.value().isValid())
        resize(m_storeDialogSize);
}
