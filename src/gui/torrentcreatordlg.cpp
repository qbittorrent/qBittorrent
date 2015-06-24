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

#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>

#include "torrentcreatordlg.h"
#include "core/utils/fs.h"
#include "core/utils/misc.h"
#include "core/preferences.h"
#include "guiiconprovider.h"
#include "core/bittorrent/session.h"
#include "core/bittorrent/torrentinfo.h"
#include "core/bittorrent/torrentcreatorthread.h"

const uint NB_PIECES_MIN = 1200;
const uint NB_PIECES_MAX = 2200;

TorrentCreatorDlg::TorrentCreatorDlg(QWidget *parent)
    : QDialog(parent)
    , m_creatorThread(0)
{
    setupUi(this);
    // Icons
    addFile_button->setIcon(GuiIconProvider::instance()->getIcon("document-new"));
    addFolder_button->setIcon(GuiIconProvider::instance()->getIcon("folder-new"));
    createButton->setIcon(GuiIconProvider::instance()->getIcon("document-save"));
    cancelButton->setIcon(GuiIconProvider::instance()->getIcon("dialog-cancel"));

    setAttribute(Qt::WA_DeleteOnClose);
    setModal(true);
    showProgressBar(false);
    loadTrackerList();
    // Piece sizes
    m_pieceSizes << 16 << 32 << 64 << 128 << 256 << 512 << 1024 << 2048 << 4096 << 8192 << 16384;
    loadSettings();
    show();
}

TorrentCreatorDlg::~TorrentCreatorDlg()
{
    if (m_creatorThread)
        delete m_creatorThread;
}

void TorrentCreatorDlg::on_addFolder_button_clicked()
{
    Preferences* const pref = Preferences::instance();
    QString lastPath = pref->getCreateTorLastAddPath();
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select a folder to add to the torrent"), lastPath, QFileDialog::ShowDirsOnly);
    if (!dir.isEmpty()) {
        pref->setCreateTorLastAddPath(dir);
        textInputPath->setText(Utils::Fs::toNativePath(dir));
        // Update piece size
        if (checkAutoPieceSize->isChecked())
            updateOptimalPieceSize();
    }
}

void TorrentCreatorDlg::on_addFile_button_clicked()
{
    Preferences* const pref = Preferences::instance();
    QString lastPath = pref->getCreateTorLastAddPath();
    QString file = QFileDialog::getOpenFileName(this, tr("Select a file to add to the torrent"), lastPath);
    if (!file.isEmpty()) {
        pref->setCreateTorLastAddPath(Utils::Fs::branchPath(file));
        textInputPath->setText(Utils::Fs::toNativePath(file));
        // Update piece size
        if (checkAutoPieceSize->isChecked())
            updateOptimalPieceSize();
    }
}

int TorrentCreatorDlg::getPieceSize() const
{
    return m_pieceSizes.at(comboPieceSize->currentIndex()) * 1024;
}

// Main function that create a .torrent file
void TorrentCreatorDlg::on_createButton_clicked()
{
    QString input = Utils::Fs::fromNativePath(textInputPath->text()).trimmed();
    if (input.endsWith("/"))
        input.chop(1);
    if (input.isEmpty()) {
        QMessageBox::critical(0, tr("No input path set"), tr("Please type an input path first"));
        return;
    }
    QStringList trackers = trackers_list->toPlainText().split("\n");
    if (!trackers_list->toPlainText().trimmed().isEmpty())
        saveTrackerList();

    Preferences* const pref = Preferences::instance();
    QString lastPath = pref->getCreateTorLastSavePath();

    QString destination = QFileDialog::getSaveFileName(this, tr("Select destination torrent file"), lastPath, tr("Torrent Files")+QString::fromUtf8(" (*.torrent)"));
    if (destination.isEmpty())
        return;

    pref->setCreateTorLastSavePath(Utils::Fs::branchPath(destination));
    if (!destination.toUpper().endsWith(".TORRENT"))
        destination += QString::fromUtf8(".torrent");

    // Disable dialog
    setInteractionEnabled(false);
    showProgressBar(true);
    // Set busy cursor
    setCursor(QCursor(Qt::WaitCursor));
    // Actually create the torrent
    QStringList urlSeeds = URLSeeds_list->toPlainText().split("\n");
    QString comment = txt_comment->toPlainText();
    // Create the creator thread
    m_creatorThread = new BitTorrent::TorrentCreatorThread(this);
    connect(m_creatorThread, SIGNAL(creationSuccess(QString, QString)), this, SLOT(handleCreationSuccess(QString, QString)));
    connect(m_creatorThread, SIGNAL(creationFailure(QString)), this, SLOT(handleCreationFailure(QString)));
    connect(m_creatorThread, SIGNAL(updateProgress(int)), this, SLOT(updateProgressBar(int)));
    m_creatorThread->create(input, destination, trackers, urlSeeds, comment, check_private->isChecked(), getPieceSize());
}

void TorrentCreatorDlg::handleCreationFailure(QString msg)
{
    // Remove busy cursor
    setCursor(QCursor(Qt::ArrowCursor));
    QMessageBox::information(0, tr("Torrent creation"), tr("Torrent creation was unsuccessful, reason: %1").arg(msg));
    setInteractionEnabled(true);
    showProgressBar(false);
}

void TorrentCreatorDlg::handleCreationSuccess(QString path, QString branch_path)
{
    // Remove busy cursor
    setCursor(QCursor(Qt::ArrowCursor));
    if (checkStartSeeding->isChecked()) {
        // Create save path temp data
        BitTorrent::TorrentInfo t = BitTorrent::TorrentInfo::loadFromFile(Utils::Fs::toNativePath(path));
        if (!t.isValid()) {
            QMessageBox::critical(0, tr("Torrent creation"), tr("Created torrent file is invalid. It won't be added to download list."));
            return;
        }

        BitTorrent::AddTorrentParams params;
        params.savePath = branch_path;
        params.skipChecking = true;
        params.ignoreShareRatio = checkIgnoreShareLimits->isChecked();

        BitTorrent::Session::instance()->addTorrent(t, params);
    }
    QMessageBox::information(0, tr("Torrent creation"), tr("Torrent was created successfully:")+" "+Utils::Fs::toNativePath(path));
    close();
}

void TorrentCreatorDlg::on_cancelButton_clicked()
{
    // End torrent creation thread
    if (m_creatorThread && m_creatorThread->isRunning()) {
        m_creatorThread->abortCreation();
        m_creatorThread->terminate();
        // Wait for termination
        m_creatorThread->wait();
    }
    // Close the dialog
    close();
}

void TorrentCreatorDlg::updateProgressBar(int progress)
{
    progressBar->setValue(progress);
}

void TorrentCreatorDlg::setInteractionEnabled(bool enabled)
{
    textInputPath->setEnabled(enabled);
    addFile_button->setEnabled(enabled);
    addFolder_button->setEnabled(enabled);
    trackers_list->setEnabled(enabled);
    URLSeeds_list->setEnabled(enabled);
    txt_comment->setEnabled(enabled);
    comboPieceSize->setEnabled(enabled);
    checkAutoPieceSize->setEnabled(enabled);
    check_private->setEnabled(enabled);
    checkStartSeeding->setEnabled(enabled);
    createButton->setEnabled(enabled);
    checkIgnoreShareLimits->setEnabled(enabled && checkStartSeeding->isChecked());
}

void TorrentCreatorDlg::showProgressBar(bool show)
{
    progressLbl->setVisible(show);
    progressBar->setVisible(show);
}

void TorrentCreatorDlg::on_checkAutoPieceSize_clicked(bool checked)
{
    comboPieceSize->setEnabled(!checked);
    if (checked)
        updateOptimalPieceSize();
}

void TorrentCreatorDlg::updateOptimalPieceSize()
{
    qint64 torrentSize = Utils::Fs::computePathSize(textInputPath->text());
    qDebug("Torrent size is %lld", torrentSize);
    if (torrentSize < 0)
        return;
    int i = 0;
    qulonglong nb_pieces = 0;
    do {
        nb_pieces = (double)torrentSize/(m_pieceSizes.at(i) * 1024.);
        qDebug("nb_pieces=%lld with piece_size=%s", nb_pieces, qPrintable(comboPieceSize->itemText(i)));
        if (nb_pieces <= NB_PIECES_MIN) {
            if (i > 1)
                --i;
            break;
        }
        else if (nb_pieces < NB_PIECES_MAX) {
            qDebug("Good, nb_pieces=%lld < %d", nb_pieces + 1, NB_PIECES_MAX);
            break;
        }
        ++i;
    } while (i < (m_pieceSizes.size() - 1));
    comboPieceSize->setCurrentIndex(i);
}

void TorrentCreatorDlg::saveTrackerList()
{
    Preferences::instance()->setCreateTorTrackers(trackers_list->toPlainText());
}

void TorrentCreatorDlg::loadTrackerList()
{
    trackers_list->setPlainText(Preferences::instance()->getCreateTorTrackers());
}

void TorrentCreatorDlg::saveSettings()
{
    Preferences* const pref = Preferences::instance();
    pref->setCreateTorGeometry(saveGeometry());
    pref->setCreateTorIgnoreRatio(checkIgnoreShareLimits->isChecked());
}

void TorrentCreatorDlg::loadSettings()
{
    const Preferences* const pref = Preferences::instance();
    restoreGeometry(pref->getCreateTorGeometry());
    checkIgnoreShareLimits->setChecked(pref->getCreateTorIgnoreRatio());
}

void TorrentCreatorDlg::closeEvent(QCloseEvent *event)
{
    qDebug() << Q_FUNC_INFO;
    saveSettings();
    QDialog::closeEvent(event);
}
