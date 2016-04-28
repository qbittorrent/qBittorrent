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
    const QString KEY_DIALOG_SIZE = SETTINGS_KEY("Dimension");

    const QString KEY_PIECE_SIZE = SETTINGS_KEY("PieceSize");
    const QString KEY_PRIVATE_TORRENT = SETTINGS_KEY("PrivateTorrent");
    const QString KEY_START_SEEDING = SETTINGS_KEY("StartSeeding");
    const QString KEY_SETTING_IGNORE_RATIO = SETTINGS_KEY("IgnoreRatio");

    const QString KEY_LAST_ADD_PATH = SETTINGS_KEY("LastAddPath");
    const QString KEY_TRACKER_LIST = SETTINGS_KEY("TrackerList");
    const QString KEY_WEB_SEED_LIST = SETTINGS_KEY("WebSeedList");
    const QString KEY_COMMENTS = SETTINGS_KEY("Comments");
    const QString KEY_LAST_SAVE_PATH = SETTINGS_KEY("LastSavePath");

    SettingsStorage *settings() { return  SettingsStorage::instance(); }
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
    QString lastPath = getLastSavePath();
    QString destination = QFileDialog::getSaveFileName(this, tr("Select where to save the new torrent"), lastPath, tr("Torrent Files (*.torrent)"));
    if (destination.isEmpty())
        return;
    if (!destination.endsWith(".torrent", Qt::CaseInsensitive))
        destination += ".torrent";
    setLastSavePath(Utils::Fs::branchPath(destination));

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
    setLastAddPath(m_ui->textInputPath->text().trimmed());

    setSettingPieceSize(m_ui->comboPieceSize->currentIndex());
    setSettingPrivateTorrent(m_ui->check_private->isChecked());
    setSettingStartSeeding(m_ui->checkStartSeeding->isChecked());
    setSettingIgnoreRatio(m_ui->checkIgnoreShareLimits->isChecked());

    setTrackerList(m_ui->trackers_list->toPlainText());
    setWebSeedList(m_ui->URLSeeds_list->toPlainText());
    setComments(m_ui->txt_comment->toPlainText());

    setDialogSize(size());
}

void TorrentCreatorDlg::loadSettings()
{
    m_ui->textInputPath->setText(!m_defaultPath.isEmpty() ? m_defaultPath : getLastAddPath());

    m_ui->comboPieceSize->setCurrentIndex(getSettingPieceSize());
    m_ui->check_private->setChecked(getSettingPrivateTorrent());
    m_ui->checkStartSeeding->setChecked(getSettingStartSeeding());
    m_ui->checkIgnoreShareLimits->setChecked(getSettingIgnoreRatio());
    m_ui->checkIgnoreShareLimits->setEnabled(m_ui->checkStartSeeding->isChecked());

    m_ui->trackers_list->setPlainText(getTrackerList());
    m_ui->URLSeeds_list->setPlainText(getWebSeedList());
    m_ui->txt_comment->setPlainText(getComments());

    resize(getDialogSize());
}

QSize TorrentCreatorDlg::getDialogSize() const
{
    return settings()->loadValue(KEY_DIALOG_SIZE, size()).toSize();
}

void TorrentCreatorDlg::setDialogSize(const QSize &size)
{
    settings()->storeValue(KEY_DIALOG_SIZE, size);
}

int TorrentCreatorDlg::getSettingPieceSize() const
{
    return settings()->loadValue(KEY_PIECE_SIZE).toInt();
}

void TorrentCreatorDlg::setSettingPieceSize(const int size)
{
    settings()->storeValue(KEY_PIECE_SIZE, size);
}

bool TorrentCreatorDlg::getSettingPrivateTorrent() const
{
    return settings()->loadValue(KEY_PRIVATE_TORRENT).toBool();
}

void TorrentCreatorDlg::setSettingPrivateTorrent(const bool b)
{
    settings()->storeValue(KEY_PRIVATE_TORRENT, b);
}

bool TorrentCreatorDlg::getSettingStartSeeding() const
{
    return settings()->loadValue(KEY_START_SEEDING).toBool();
}

void TorrentCreatorDlg::setSettingStartSeeding(const bool b)
{
    settings()->storeValue(KEY_START_SEEDING, b);
}

bool TorrentCreatorDlg::getSettingIgnoreRatio() const
{
    return settings()->loadValue(KEY_SETTING_IGNORE_RATIO).toBool();
}

void TorrentCreatorDlg::setSettingIgnoreRatio(const bool ignore)
{
    settings()->storeValue(KEY_SETTING_IGNORE_RATIO, ignore);
}

QString TorrentCreatorDlg::getLastAddPath() const
{
    return settings()->loadValue(KEY_LAST_ADD_PATH, QDir::homePath()).toString();
}

void TorrentCreatorDlg::setLastAddPath(const QString &path)
{
    settings()->storeValue(KEY_LAST_ADD_PATH, path);
}

QString TorrentCreatorDlg::getTrackerList() const
{
    return settings()->loadValue(KEY_TRACKER_LIST).toString();
}

void TorrentCreatorDlg::setTrackerList(const QString &list)
{
    settings()->storeValue(KEY_TRACKER_LIST, list);
}

QString TorrentCreatorDlg::getWebSeedList() const
{
    return settings()->loadValue(KEY_WEB_SEED_LIST).toString();
}

void TorrentCreatorDlg::setWebSeedList(const QString &list)
{
    settings()->storeValue(KEY_WEB_SEED_LIST, list);
}

QString TorrentCreatorDlg::getComments() const
{
    return settings()->loadValue(KEY_COMMENTS).toString();
}

void TorrentCreatorDlg::setComments(const QString &str)
{
    settings()->storeValue(KEY_COMMENTS, str);
}

QString TorrentCreatorDlg::getLastSavePath() const
{
    return settings()->loadValue(KEY_LAST_SAVE_PATH, QDir::homePath()).toString();
}

void TorrentCreatorDlg::setLastSavePath(const QString &path)
{
    settings()->storeValue(KEY_LAST_SAVE_PATH, path);
}
