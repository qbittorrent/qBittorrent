/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2024  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2024  Radu Carpa <radu.carpa@cern.ch>
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
#include "base/bittorrent/torrentdescriptor.h"
#include "base/global.h"
#include "base/utils/fs.h"
#include "base/utils/misc.h"
#include "ui_torrentcreatordialog.h"
#include "utils.h"

#define SETTINGS_KEY(name) u"TorrentCreator/" name

namespace
{
    // When the root file/directory of the created torrent is a symlink, we want to keep the symlink name in the torrent.
    // On Windows, however, QFileDialog::DontResolveSymlinks disables shortcuts (.lnk files) expansion, making it impossible to pick a file if its path contains a shortcut.
    // As of NTFS symlinks, they don't seem to be resolved anyways.
#ifndef Q_OS_WIN
    const QFileDialog::Options FILE_DIALOG_OPTIONS {QFileDialog::DontResolveSymlinks};
#else
    const QFileDialog::Options FILE_DIALOG_OPTIONS {};
#endif
}

TorrentCreatorDialog::TorrentCreatorDialog(QWidget *parent, const Path &defaultPath)
    : QDialog(parent)
    , m_ui(new Ui::TorrentCreatorDialog)
    , m_threadPool(this)
    , m_storeDialogSize(SETTINGS_KEY(u"Size"_s))
    , m_storePieceSize(SETTINGS_KEY(u"PieceSize"_s))
    , m_storePrivateTorrent(SETTINGS_KEY(u"PrivateTorrent"_s))
    , m_storeStartSeeding(SETTINGS_KEY(u"StartSeeding"_s))
    , m_storeIgnoreRatio(SETTINGS_KEY(u"IgnoreRatio"_s))
#ifdef QBT_USES_LIBTORRENT2
    , m_storeTorrentFormat(SETTINGS_KEY(u"TorrentFormat"_s))
#else
    , m_storeOptimizeAlignment(SETTINGS_KEY(u"OptimizeAlignment"_s))
    , m_paddedFileSizeLimit(SETTINGS_KEY(u"PaddedFileSizeLimit"_s))
#endif
    , m_storeLastAddPath(SETTINGS_KEY(u"LastAddPath"_s))
    , m_storeTrackerList(SETTINGS_KEY(u"TrackerList"_s))
    , m_storeWebSeedList(SETTINGS_KEY(u"WebSeedList"_s))
    , m_storeComments(SETTINGS_KEY(u"Comments"_s))
    , m_storeLastSavePath(SETTINGS_KEY(u"LastSavePath"_s))
    , m_storeSource(SETTINGS_KEY(u"Source"_s))
{
    m_ui->setupUi(this);

    m_ui->comboPieceSize->addItem(tr("Auto"), 0);
    for (int i = 4; i <= 17; ++i)
    {
        const int size = 1024 << i;
        const QString displaySize = Utils::Misc::friendlyUnit(size, false, 0);
        m_ui->comboPieceSize->addItem(displaySize, size);
    }

    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Create Torrent"));
    m_ui->textInputPath->setMode(FileSystemPathEdit::Mode::ReadOnly);

    connect(m_ui->addFileButton, &QPushButton::clicked, this, &TorrentCreatorDialog::onAddFileButtonClicked);
    connect(m_ui->addFolderButton, &QPushButton::clicked, this, &TorrentCreatorDialog::onAddFolderButtonClicked);
    connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &TorrentCreatorDialog::onCreateButtonClicked);
    connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_ui->buttonCalcTotalPieces, &QPushButton::clicked, this, &TorrentCreatorDialog::updatePiecesCount);
    connect(m_ui->checkStartSeeding, &QCheckBox::clicked, m_ui->checkIgnoreShareLimits, &QWidget::setEnabled);

    loadSettings();
    updateInputPath(defaultPath);

    m_threadPool.setMaxThreadCount(1);
    m_threadPool.setObjectName("TorrentCreatorDialog m_threadPool");

#ifdef QBT_USES_LIBTORRENT2
    m_ui->checkOptimizeAlignment->hide();
#else
    m_ui->widgetTorrentFormat->hide();
#endif
}

TorrentCreatorDialog::~TorrentCreatorDialog()
{
    saveSettings();

    delete m_ui;
}

void TorrentCreatorDialog::updateInputPath(const Path &path)
{
    if (path.isEmpty()) return;
    m_ui->textInputPath->setSelectedPath(path);
    updateProgressBar(0);
}

void TorrentCreatorDialog::onAddFolderButtonClicked()
{
    const QString oldPath = m_ui->textInputPath->selectedPath().data();
    const Path path {QFileDialog::getExistingDirectory(this, tr("Select folder")
            , oldPath, (QFileDialog::ShowDirsOnly | FILE_DIALOG_OPTIONS))};
    updateInputPath(path);
}

void TorrentCreatorDialog::onAddFileButtonClicked()
{
    const QString oldPath = m_ui->textInputPath->selectedPath().data();
    const Path path {QFileDialog::getOpenFileName(this, tr("Select file"), oldPath, QString(), nullptr, FILE_DIALOG_OPTIONS)};
    updateInputPath(path);
}

int TorrentCreatorDialog::getPieceSize() const
{
    return m_ui->comboPieceSize->currentData().toInt();
}

#ifdef QBT_USES_LIBTORRENT2
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
        const QUrl firstItem = event->mimeData()->urls().first();
        const Path path {
            (firstItem.scheme().compare(u"file", Qt::CaseInsensitive) == 0)
                    ? firstItem.toLocalFile() : firstItem.toString()
        };
        updateInputPath(path);
    }
}

void TorrentCreatorDialog::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat(u"text/plain"_s) || event->mimeData()->hasFormat(u"text/uri-list"_s))
        event->acceptProposedAction();
}

// Main function that create a .torrent file
void TorrentCreatorDialog::onCreateButtonClicked()
{
#ifdef Q_OS_WIN
    // Resolve the path in case it contains a shortcut (otherwise, the following usages will consider it invalid)
    const Path inputPath = Utils::Fs::toCanonicalPath(m_ui->textInputPath->selectedPath());
#else
    const Path inputPath = m_ui->textInputPath->selectedPath();
#endif

    // test if readable
    if (!Utils::Fs::isReadable(inputPath))
    {
        QMessageBox::critical(this, tr("Torrent creation failed"), tr("Reason: Path to file/folder is not readable."));
        return;
    }

    // get save path
    const Path lastSavePath = (m_storeLastSavePath.get(Utils::Fs::homePath()) / Path(inputPath.filename() + u".torrent"));
    Path destPath {QFileDialog::getSaveFileName(this, tr("Select where to save the new torrent"), lastSavePath.data(), tr("Torrent Files (*.torrent)"))};
    if (destPath.isEmpty())
        return;
    if (!destPath.hasExtension(TORRENT_FILE_EXTENSION))
        destPath += TORRENT_FILE_EXTENSION;
    m_storeLastSavePath = destPath.parentPath();

    // Disable dialog & set busy cursor
    setInteractionEnabled(false);
    setCursor(QCursor(Qt::WaitCursor));

    const QStringList trackers = m_ui->trackersList->toPlainText().trimmed()
        .replace(QRegularExpression(u"\n\n[\n]+"_s), u"\n\n"_s).split(u'\n');
    const BitTorrent::TorrentCreatorParams params
    {
        .isPrivate = m_ui->checkPrivate->isChecked(),
#ifdef QBT_USES_LIBTORRENT2
        .torrentFormat = getTorrentFormat(),
#else
        .isAlignmentOptimized = m_ui->checkOptimizeAlignment->isChecked(),
        .paddedFileSizeLimit = getPaddedFileSizeLimit(),
#endif
        .pieceSize = getPieceSize(),
        .sourcePath = inputPath,
        .torrentFilePath = destPath,
        .comment = m_ui->txtComment->toPlainText(),
        .source = m_ui->lineEditSource->text(),
        .trackers = trackers,
        .urlSeeds = m_ui->URLSeedsList->toPlainText().split(u'\n', Qt::SkipEmptyParts)
    };

    auto *torrentCreator = new BitTorrent::TorrentCreator(params);
    connect(this, &QDialog::rejected, torrentCreator, &BitTorrent::TorrentCreator::requestInterruption);
    connect(torrentCreator, &BitTorrent::TorrentCreator::creationSuccess, this, &TorrentCreatorDialog::handleCreationSuccess);
    connect(torrentCreator, &BitTorrent::TorrentCreator::creationFailure, this, &TorrentCreatorDialog::handleCreationFailure);
    connect(torrentCreator, &BitTorrent::TorrentCreator::progressUpdated, this, &TorrentCreatorDialog::updateProgressBar);

    // run the torrentCreator in a thread
    m_threadPool.start(torrentCreator);
}

void TorrentCreatorDialog::handleCreationFailure(const QString &msg)
{
    // Remove busy cursor
    setCursor(QCursor(Qt::ArrowCursor));
    QMessageBox::information(this, tr("Torrent creation failed"), msg);
    setInteractionEnabled(true);
}

void TorrentCreatorDialog::handleCreationSuccess(const BitTorrent::TorrentCreatorResult &result)
{
    setCursor(QCursor(Qt::ArrowCursor));
    setInteractionEnabled(true);

    QMessageBox::information(this, tr("Torrent creator")
        , u"%1\n%2"_s.arg(tr("Torrent created:"), result.torrentFilePath.toString()));

    if (m_ui->checkStartSeeding->isChecked())
    {
        if (const auto loadResult = BitTorrent::TorrentDescriptor::loadFromFile(result.torrentFilePath))
        {
            BitTorrent::AddTorrentParams params;
            params.savePath = result.savePath;
            params.skipChecking = true;
            if (m_ui->checkIgnoreShareLimits->isChecked())
            {
                params.ratioLimit = BitTorrent::Torrent::NO_RATIO_LIMIT;
                params.seedingTimeLimit = BitTorrent::Torrent::NO_SEEDING_TIME_LIMIT;
                params.inactiveSeedingTimeLimit = BitTorrent::Torrent::NO_INACTIVE_SEEDING_TIME_LIMIT;
            }
            params.useAutoTMM = false;  // otherwise if it is on by default, it will overwrite `savePath` to the default save path

            BitTorrent::Session::instance()->addTorrent(loadResult.value(), params);
        }
        else
        {
            const QString message = tr("Add torrent to transfer list failed.") + u'\n' + tr("Reason: \"%1\"").arg(loadResult.error());
            QMessageBox::critical(this, tr("Add torrent failed"), message);
        }
    }
}

void TorrentCreatorDialog::updateProgressBar(int progress)
{
    m_ui->progressBar->setValue(progress);
}

void TorrentCreatorDialog::updatePiecesCount()
{
    const Path path = m_ui->textInputPath->selectedPath();
#ifdef QBT_USES_LIBTORRENT2
    const int count = BitTorrent::TorrentCreator::calculateTotalPieces(
        path, getPieceSize(), getTorrentFormat());
#else
    const bool isAlignmentOptimized = m_ui->checkOptimizeAlignment->isChecked();
    const int count = BitTorrent::TorrentCreator::calculateTotalPieces(path
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
    m_ui->lineEditSource->setEnabled(enabled);
    m_ui->comboPieceSize->setEnabled(enabled);
    m_ui->buttonCalcTotalPieces->setEnabled(enabled);
    m_ui->checkPrivate->setEnabled(enabled);
    m_ui->checkStartSeeding->setEnabled(enabled);
    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(enabled);
    m_ui->checkIgnoreShareLimits->setEnabled(enabled && m_ui->checkStartSeeding->isChecked());
#ifdef QBT_USES_LIBTORRENT2
    m_ui->widgetTorrentFormat->setEnabled(enabled);
#else
    m_ui->checkOptimizeAlignment->setEnabled(enabled);
    m_ui->spinPaddedFileSizeLimit->setEnabled(enabled);
#endif
}

void TorrentCreatorDialog::saveSettings()
{
    m_storeLastAddPath = m_ui->textInputPath->selectedPath();

    m_storePieceSize = m_ui->comboPieceSize->currentIndex();
    m_storePrivateTorrent = m_ui->checkPrivate->isChecked();
    m_storeStartSeeding = m_ui->checkStartSeeding->isChecked();
    m_storeIgnoreRatio = m_ui->checkIgnoreShareLimits->isChecked();
#ifdef QBT_USES_LIBTORRENT2
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
    m_ui->textInputPath->setSelectedPath(m_storeLastAddPath.get(Utils::Fs::homePath()));

    m_ui->comboPieceSize->setCurrentIndex(m_storePieceSize);
    m_ui->checkPrivate->setChecked(m_storePrivateTorrent);
    m_ui->checkStartSeeding->setChecked(m_storeStartSeeding);
    m_ui->checkIgnoreShareLimits->setChecked(m_storeIgnoreRatio);
    m_ui->checkIgnoreShareLimits->setEnabled(m_ui->checkStartSeeding->isChecked());
#ifdef QBT_USES_LIBTORRENT2
    m_ui->comboTorrentFormat->setCurrentIndex(m_storeTorrentFormat.get(1));
#else
    m_ui->checkOptimizeAlignment->setChecked(m_storeOptimizeAlignment.get(true));
    m_ui->spinPaddedFileSizeLimit->setValue(m_paddedFileSizeLimit.get(-1));
#endif

    m_ui->trackersList->setPlainText(m_storeTrackerList);
    m_ui->URLSeedsList->setPlainText(m_storeWebSeedList);
    m_ui->txtComment->setPlainText(m_storeComments);
    m_ui->lineEditSource->setText(m_storeSource);

    if (const QSize dialogSize = m_storeDialogSize; dialogSize.isValid())
        resize(dialogSize);
}
