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

#include "torrentcreatordialog.h"

#include <QCloseEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QMimeData>
#include <QUrl>

#include "base/bittorrent/session.h"
#include "base/bittorrent/torrentinfo.h"
#include "base/global.h"
#include "base/utils/fs.h"
#include "ui_torrentcreatordialog.h"
#include "utils.h"

#define SETTINGS_KEY(name) "TorrentCreator/" name

TorrentCreatorDialog::TorrentCreatorDialog(QWidget *parent, const QString &defaultPath)
    : QDialog(parent)
    , m_ui(new Ui::TorrentCreatorDialog)
    , m_creatorThread(new BitTorrent::TorrentCreatorThread(this))
    , m_storeDialogSize(SETTINGS_KEY("Size"))
    , m_storePieceSize(SETTINGS_KEY("PieceSize"))
    , m_storePrivateTorrent(SETTINGS_KEY("PrivateTorrent"))
    , m_storeStartSeeding(SETTINGS_KEY("StartSeeding"))
    , m_storeIgnoreRatio(SETTINGS_KEY("IgnoreRatio"))
#if (LIBTORRENT_VERSION_NUM >= 20000)
    , m_storeTorrentFormat(SETTINGS_KEY("TorrentFormat"))
#else
    , m_storeOptimizeAlignment(SETTINGS_KEY("OptimizeAlignment"))
    , m_paddedFileSizeLimit(SETTINGS_KEY("PaddedFileSizeLimit"))
#endif
    , m_storeLastAddPath(SETTINGS_KEY("LastAddPath"))
    , m_storeTrackerList(SETTINGS_KEY("TrackerList"))
    , m_storeWebSeedList(SETTINGS_KEY("WebSeedList"))
    , m_storeComments(SETTINGS_KEY("Comments"))
    , m_storeLastSavePath(SETTINGS_KEY("LastSavePath"))
    , m_storeSource(SETTINGS_KEY("Source"))
{
    m_ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    setModal(false);

    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Create Torrent"));

    connect(m_ui->addFileButton, &QPushButton::clicked, this, &TorrentCreatorDialog::onAddFileButtonClicked);
    connect(m_ui->addFolderButton, &QPushButton::clicked, this, &TorrentCreatorDialog::onAddFolderButtonClicked);
    connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &TorrentCreatorDialog::onCreateButtonClicked);
    connect(m_ui->buttonCalcTotalPieces, &QPushButton::clicked, this, &TorrentCreatorDialog::updatePiecesCount);

    connect(m_creatorThread, &BitTorrent::TorrentCreatorThread::creationSuccess, this, &TorrentCreatorDialog::handleCreationSuccess);
    connect(m_creatorThread, &BitTorrent::TorrentCreatorThread::creationFailure, this, &TorrentCreatorDialog::handleCreationFailure);
    connect(m_creatorThread, &BitTorrent::TorrentCreatorThread::updateProgress, this, &TorrentCreatorDialog::updateProgressBar);

    loadSettings();
    updateInputPath(defaultPath);

#if (LIBTORRENT_VERSION_NUM >= 20000)
    m_ui->checkOptimizeAlignment->hide();
#else
    m_ui->widgetTorrentFormat->hide();
#endif

    show();
}

TorrentCreatorDialog::~TorrentCreatorDialog()
{
    saveSettings();

    delete m_ui;
}

void TorrentCreatorDialog::updateInputPath(const QString &path)
{
    if (path.isEmpty()) return;
    m_ui->textInputPath->setText(Utils::Fs::toNativePath(path));
    updateProgressBar(0);
}

void TorrentCreatorDialog::onAddFolderButtonClicked()
{
    QString oldPath = m_ui->textInputPath->text();
    QString path = QFileDialog::getExistingDirectory(this, tr("Select folder"), oldPath);
    updateInputPath(path);
}

void TorrentCreatorDialog::onAddFileButtonClicked()
{
    QString oldPath = m_ui->textInputPath->text();
    QString path = QFileDialog::getOpenFileName(this, tr("Select file"), oldPath);
    updateInputPath(path);
}

int TorrentCreatorDialog::getPieceSize() const
{
    const int pieceSizes[] = {0, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768};  // base unit in KiB
    return pieceSizes[m_ui->comboPieceSize->currentIndex()] * 1024;
}

#if (LIBTORRENT_VERSION_NUM >= 20000)
BitTorrent::TorrentFormat TorrentCreatorDialog::getTorrentFormat() const
{
    switch (m_ui->comboTorrentFormat->currentIndex())
    {
    case 0:
        return BitTorrent::TorrentFormat::V2;
    case 1:
        return BitTorrent::TorrentFormat::Hybrid;
    case 2:
        return BitTorrent::TorrentFormat::V1;
    }
    return BitTorrent::TorrentFormat::Hybrid;
}
#else
int TorrentCreatorDialog::getPaddedFileSizeLimit() const
{
    const int value = m_ui->spinPaddedFileSizeLimit->value();
    return ((value >= 0) ? (value * 1024) : -1);
}
#endif

void TorrentCreatorDialog::dropEvent(QDropEvent *event)
{
    event->acceptProposedAction();

    if (event->mimeData()->hasUrls())
    {
        // only take the first one
        QUrl firstItem = event->mimeData()->urls().first();
        QString path = (firstItem.scheme().compare("file", Qt::CaseInsensitive) == 0)
                       ? firstItem.toLocalFile() : firstItem.toString();
        updateInputPath(path);
    }
}

void TorrentCreatorDialog::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat("text/plain") || event->mimeData()->hasFormat("text/uri-list"))
        event->acceptProposedAction();
}

// Main function that create a .torrent file
void TorrentCreatorDialog::onCreateButtonClicked()
{
    QString input = Utils::Fs::toUniformPath(m_ui->textInputPath->text()).trimmed();

    // test if readable
    const QFileInfo fi(input);
    if (!fi.isReadable())
    {
        QMessageBox::critical(this, tr("Torrent creation failed"), tr("Reason: Path to file/folder is not readable."));
        return;
    }
    input = fi.canonicalFilePath();

    // get save path
    const QString savePath = m_storeLastSavePath.get(QDir::homePath()) + QLatin1Char('/') + fi.fileName() + QLatin1String(".torrent");
    QString destination = QFileDialog::getSaveFileName(this, tr("Select where to save the new torrent"), savePath, tr("Torrent Files (*.torrent)"));
    if (destination.isEmpty())
        return;
    if (!destination.endsWith(C_TORRENT_FILE_EXTENSION, Qt::CaseInsensitive))
        destination += C_TORRENT_FILE_EXTENSION;
    m_storeLastSavePath = Utils::Fs::branchPath(destination);

    // Disable dialog & set busy cursor
    setInteractionEnabled(false);
    setCursor(QCursor(Qt::WaitCursor));

    const QStringList trackers = m_ui->trackersList->toPlainText().trimmed()
        .replace(QRegularExpression("\n\n[\n]+"), "\n\n").split('\n');
    const BitTorrent::TorrentCreatorParams params
    {
        m_ui->checkPrivate->isChecked()
#if (LIBTORRENT_VERSION_NUM >= 20000)
        , getTorrentFormat()
#else
        , m_ui->checkOptimizeAlignment->isChecked()
        , getPaddedFileSizeLimit()
#endif
        , getPieceSize()
        , input, destination
        , m_ui->txtComment->toPlainText()
        , m_ui->lineEditSource->text()
        , trackers
        , m_ui->URLSeedsList->toPlainText().split('\n', QString::SkipEmptyParts)
    };

    // run the creator thread
    m_creatorThread->create(params);
}

void TorrentCreatorDialog::handleCreationFailure(const QString &msg)
{
    // Remove busy cursor
    setCursor(QCursor(Qt::ArrowCursor));
    QMessageBox::information(this, tr("Torrent creation failed"), tr("Reason: %1").arg(msg));
    setInteractionEnabled(true);
}

void TorrentCreatorDialog::handleCreationSuccess(const QString &path, const QString &branchPath)
{
    // Remove busy cursor
    setCursor(QCursor(Qt::ArrowCursor));
    if (m_ui->checkStartSeeding->isChecked())
    {
        // Create save path temp data
        const BitTorrent::TorrentInfo info = BitTorrent::TorrentInfo::loadFromFile(Utils::Fs::toNativePath(path));
        if (!info.isValid())
        {
            QMessageBox::critical(this, tr("Torrent creation failed"), tr("Reason: Created torrent is invalid. It won't be added to download list."));
            return;
        }

        BitTorrent::AddTorrentParams params;
        params.savePath = branchPath;
        params.skipChecking = true;
        if (m_ui->checkIgnoreShareLimits->isChecked())
        {
            params.ratioLimit = BitTorrent::TorrentHandle::NO_RATIO_LIMIT;
            params.seedingTimeLimit = BitTorrent::TorrentHandle::NO_SEEDING_TIME_LIMIT;
        }
        params.useAutoTMM = false;  // otherwise if it is on by default, it will overwrite `savePath` to the default save path

        BitTorrent::Session::instance()->addTorrent(info, params);
    }
    QMessageBox::information(this, tr("Torrent creator")
        , QString::fromLatin1("%1\n%2").arg(tr("Torrent created:"), Utils::Fs::toNativePath(path)));
    setInteractionEnabled(true);
}

void TorrentCreatorDialog::updateProgressBar(int progress)
{
    m_ui->progressBar->setValue(progress);
}

void TorrentCreatorDialog::updatePiecesCount()
{
    const QString path = m_ui->textInputPath->text().trimmed();
#if (LIBTORRENT_VERSION_NUM >= 20000)
    const int count = BitTorrent::TorrentCreatorThread::calculateTotalPieces(
        path, getPieceSize(), getTorrentFormat());
#else
    const bool isAlignmentOptimized = m_ui->checkOptimizeAlignment->isChecked();
    const int count = BitTorrent::TorrentCreatorThread::calculateTotalPieces(path
        , getPieceSize(), isAlignmentOptimized, getPaddedFileSizeLimit());
#endif
    m_ui->labelTotalPieces->setText(QString::number(count));
}

void TorrentCreatorDialog::setInteractionEnabled(const bool enabled) const
{
    m_ui->textInputPath->setEnabled(enabled);
    m_ui->addFileButton->setEnabled(enabled);
    m_ui->addFolderButton->setEnabled(enabled);
    m_ui->trackersList->setEnabled(enabled);
    m_ui->URLSeedsList->setEnabled(enabled);
    m_ui->txtComment->setEnabled(enabled);
    m_ui->comboPieceSize->setEnabled(enabled);
    m_ui->buttonCalcTotalPieces->setEnabled(enabled);
    m_ui->checkPrivate->setEnabled(enabled);
    m_ui->checkStartSeeding->setEnabled(enabled);
    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(enabled);
    m_ui->checkIgnoreShareLimits->setEnabled(enabled && m_ui->checkStartSeeding->isChecked());
#if (LIBTORRENT_VERSION_NUM >= 20000)
    m_ui->widgetTorrentFormat->setEnabled(enabled);
#else
    m_ui->checkOptimizeAlignment->setEnabled(enabled);
    m_ui->spinPaddedFileSizeLimit->setEnabled(enabled);
#endif
}

void TorrentCreatorDialog::saveSettings()
{
    m_storeLastAddPath = m_ui->textInputPath->text().trimmed();

    m_storePieceSize = m_ui->comboPieceSize->currentIndex();
    m_storePrivateTorrent = m_ui->checkPrivate->isChecked();
    m_storeStartSeeding = m_ui->checkStartSeeding->isChecked();
    m_storeIgnoreRatio = m_ui->checkIgnoreShareLimits->isChecked();
#if (LIBTORRENT_VERSION_NUM >= 20000)
    m_storeTorrentFormat = m_ui->comboTorrentFormat->currentIndex();
#else
    m_storeOptimizeAlignment = m_ui->checkOptimizeAlignment->isChecked();
    m_paddedFileSizeLimit = m_ui->spinPaddedFileSizeLimit->value();
#endif

    m_storeTrackerList = m_ui->trackersList->toPlainText();
    m_storeWebSeedList = m_ui->URLSeedsList->toPlainText();
    m_storeComments = m_ui->txtComment->toPlainText();
    m_storeSource = m_ui->lineEditSource->text();

    m_storeDialogSize = size();
}

void TorrentCreatorDialog::loadSettings()
{
    m_ui->textInputPath->setText(m_storeLastAddPath.get(QDir::homePath()));

    m_ui->comboPieceSize->setCurrentIndex(m_storePieceSize);
    m_ui->checkPrivate->setChecked(m_storePrivateTorrent);
    m_ui->checkStartSeeding->setChecked(m_storeStartSeeding);
    m_ui->checkIgnoreShareLimits->setChecked(m_storeIgnoreRatio);
    m_ui->checkIgnoreShareLimits->setEnabled(m_ui->checkStartSeeding->isChecked());
#if (LIBTORRENT_VERSION_NUM >= 20000)
    m_ui->comboTorrentFormat->setCurrentIndex(m_storeTorrentFormat.get(1));
#else
    m_ui->checkOptimizeAlignment->setChecked(m_storeOptimizeAlignment.get(true));
    m_ui->spinPaddedFileSizeLimit->setValue(m_paddedFileSizeLimit.get(-1));
#endif

    m_ui->trackersList->setPlainText(m_storeTrackerList);
    m_ui->URLSeedsList->setPlainText(m_storeWebSeedList);
    m_ui->txtComment->setPlainText(m_storeComments);
    m_ui->lineEditSource->setText(m_storeSource);

    Utils::Gui::resize(this, m_storeDialogSize);
}
