/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2010  Christophe Dumez
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
 *
 * Contact : chris@qbittorrent.org
 */

#include "torrentcreatordlg.h"

#include <QCloseEvent>
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QMimeData>
#include <QUrl>

#include "base/bittorrent/session.h"
#include "base/bittorrent/torrentinfo.h"
#include "base/bittorrent/torrentcreatorthread.h"
#include "base/settingsstorage.h"
#include "base/utils/fs.h"
#include "guiiconprovider.h"

#include "ui_torrentcreatordlg.h"

namespace
{
#define SETTINGS_KEY(name) "TorrentCreator/" name
    CachedSettingValue<QSize> storeDialogSize(SETTINGS_KEY("Dimension"));

    CachedSettingValue<int> storePieceSize(SETTINGS_KEY("PieceSize"));
    CachedSettingValue<bool> storePrivateTorrent(SETTINGS_KEY("PrivateTorrent"));
    CachedSettingValue<bool> storeStartSeeding(SETTINGS_KEY("StartSeeding"));
    CachedSettingValue<bool> storeIgnoreRatio(SETTINGS_KEY("IgnoreRatio"));

    CachedSettingValue<QString> storeLastAddPath(SETTINGS_KEY("LastAddPath"), QDir::homePath());
    CachedSettingValue<QString> storeTrackerList(SETTINGS_KEY("TrackerList"));
    CachedSettingValue<QString> storeWebSeedList(SETTINGS_KEY("WebSeedList"));
    CachedSettingValue<QString> storeComments(SETTINGS_KEY("Comments"));
    CachedSettingValue<QString> storeLastSavePath(SETTINGS_KEY("LastSavePath"), QDir::homePath());
}

TorrentCreatorDlg::TorrentCreatorDlg(QWidget *parent, const QString &defaultPath)
    : QDialog(parent)
    , m_ui(new Ui::TorrentCreatorDlg)
    , m_creatorThread(nullptr)
    , m_defaultPath(Utils::Fs::toNativePath(defaultPath))
{
    m_ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    setModal(true);

    showProgressBar(false);
    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Create Torrent"));

    connect(m_ui->addFile_button, SIGNAL(clicked(bool)), SLOT(onAddFileButtonClicked()));
    connect(m_ui->addFolder_button, SIGNAL(clicked(bool)), SLOT(onAddFolderButtonClicked()));
    connect(m_ui->buttonBox, SIGNAL(accepted()), SLOT(onCreateButtonClicked()));

    loadSettings();
    show();
}

TorrentCreatorDlg::~TorrentCreatorDlg()
{
    saveSettings();

    // End torrent creation thread
    if (m_creatorThread) {
        if (m_creatorThread->isRunning()) {
            m_creatorThread->abortCreation();
            m_creatorThread->terminate();
            // Wait for termination
            m_creatorThread->wait();
        }
        delete m_creatorThread;
    }

    delete m_ui;
}

void TorrentCreatorDlg::onAddFolderButtonClicked()
{
    QString oldPath = m_ui->textInputPath->text();
    QString path = QFileDialog::getExistingDirectory(this, tr("Select folder"), oldPath);
    if (!path.isEmpty())
        m_ui->textInputPath->setText(Utils::Fs::toNativePath(path));
}

void TorrentCreatorDlg::onAddFileButtonClicked()
{
    QString oldPath = m_ui->textInputPath->text();
    QString path = QFileDialog::getOpenFileName(this, tr("Select file"), oldPath);
    if (!path.isEmpty())
        m_ui->textInputPath->setText(Utils::Fs::toNativePath(path));
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
        m_ui->textInputPath->setText(Utils::Fs::toNativePath(path));
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
    QFileInfo fi(input);
    if (!fi.isReadable()) {
        QMessageBox::critical(this, tr("Torrent creator failed"), tr("Reason: Path to file/folder is not readable."));
        return;
    }
    input = fi.canonicalFilePath();

    // get save path
    QString lastPath = storeLastSavePath;
    QString destination = QFileDialog::getSaveFileName(this, tr("Select where to save the new torrent"), lastPath, tr("Torrent Files (*.torrent)"));
    if (destination.isEmpty())
        return;
    if (!destination.endsWith(".torrent", Qt::CaseInsensitive))
        destination += ".torrent";
    storeLastSavePath = Utils::Fs::branchPath(destination);

    // Disable dialog & set busy cursor
    setInteractionEnabled(false);
    showProgressBar(true);
    setCursor(QCursor(Qt::WaitCursor));

    QStringList trackers = m_ui->trackers_list->toPlainText().split("\n");
    QStringList urlSeeds = m_ui->URLSeeds_list->toPlainText().split("\n");
    QString comment = m_ui->txt_comment->toPlainText();

    // Create the creator thread
    m_creatorThread = new BitTorrent::TorrentCreatorThread(this);
    connect(m_creatorThread, SIGNAL(creationSuccess(QString, QString)), this, SLOT(handleCreationSuccess(QString, QString)));
    connect(m_creatorThread, SIGNAL(creationFailure(QString)), this, SLOT(handleCreationFailure(QString)));
    connect(m_creatorThread, SIGNAL(updateProgress(int)), this, SLOT(updateProgressBar(int)));
    m_creatorThread->create(input, destination, trackers, urlSeeds, comment, m_ui->check_private->isChecked(), getPieceSize());
}

void TorrentCreatorDlg::handleCreationFailure(const QString &msg)
{
    // Remove busy cursor
    setCursor(QCursor(Qt::ArrowCursor));
    QMessageBox::information(this, tr("Torrent creator failed"), tr("Reason: %1").arg(msg));
    setInteractionEnabled(true);
    showProgressBar(false);
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

    close();
}

void TorrentCreatorDlg::updateProgressBar(int progress)
{
    m_ui->progressBar->setValue(progress);
}

void TorrentCreatorDlg::setInteractionEnabled(bool enabled)
{
    m_ui->textInputPath->setEnabled(enabled);
    m_ui->addFile_button->setEnabled(enabled);
    m_ui->addFolder_button->setEnabled(enabled);
    m_ui->trackers_list->setEnabled(enabled);
    m_ui->URLSeeds_list->setEnabled(enabled);
    m_ui->txt_comment->setEnabled(enabled);
    m_ui->comboPieceSize->setEnabled(enabled);
    m_ui->check_private->setEnabled(enabled);
    m_ui->checkStartSeeding->setEnabled(enabled);
    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(enabled);
    m_ui->checkIgnoreShareLimits->setEnabled(enabled && m_ui->checkStartSeeding->isChecked());
}

void TorrentCreatorDlg::showProgressBar(bool show)
{
    m_ui->progressLbl->setVisible(show);
    m_ui->progressBar->setVisible(show);
}

void TorrentCreatorDlg::saveSettings()
{
    storeLastAddPath = m_ui->textInputPath->text().trimmed();

    storePieceSize = m_ui->comboPieceSize->currentIndex();
    storePrivateTorrent = m_ui->check_private->isChecked();
    storeStartSeeding = m_ui->checkStartSeeding->isChecked();
    storeIgnoreRatio = m_ui->checkIgnoreShareLimits->isChecked();

    storeTrackerList = m_ui->trackers_list->toPlainText();
    storeWebSeedList = m_ui->URLSeeds_list->toPlainText();
    storeComments = m_ui->txt_comment->toPlainText();

    storeDialogSize = size();
}

void TorrentCreatorDlg::loadSettings()
{
    m_ui->textInputPath->setText(!m_defaultPath.isEmpty() ? m_defaultPath : storeLastAddPath);

    m_ui->comboPieceSize->setCurrentIndex(storePieceSize);
    m_ui->check_private->setChecked(storePrivateTorrent);
    m_ui->checkStartSeeding->setChecked(storeStartSeeding);
    m_ui->checkIgnoreShareLimits->setChecked(storeIgnoreRatio);
    m_ui->checkIgnoreShareLimits->setEnabled(m_ui->checkStartSeeding->isChecked());

    m_ui->trackers_list->setPlainText(storeTrackerList);
    m_ui->URLSeeds_list->setPlainText(storeWebSeedList);
    m_ui->txt_comment->setPlainText(storeComments);

    if (storeDialogSize.value().isValid())
        resize(storeDialogSize);
}
