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

#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>

#include "torrentimportdlg.h"
#include "ui_torrentimportdlg.h"
#include "core/preferences.h"
#include "core/bittorrent/infohash.h"
#include "core/bittorrent/session.h"
#include "guiiconprovider.h"
#include "core/utils/fs.h"

TorrentImportDlg::TorrentImportDlg(QWidget *parent):
    QDialog(parent),
    ui(new Ui::TorrentImportDlg)
{
    ui->setupUi(this);
    // Icons
    ui->lbl_info->setPixmap(GuiIconProvider::instance()->getIcon("dialog-information").pixmap(ui->lbl_info->height()));
    ui->lbl_info->setFixedWidth(ui->lbl_info->height());
    ui->importBtn->setIcon(GuiIconProvider::instance()->getIcon("document-import"));
    // Libtorrent < 0.15 does not support skipping file checking
    loadSettings();
}

TorrentImportDlg::~TorrentImportDlg()
{
    delete ui;
}

void TorrentImportDlg::on_browseTorrentBtn_clicked()
{
    const QString default_dir = Preferences::instance()->getMainLastDir();
    // Ask for a torrent file
    m_torrentPath = QFileDialog::getOpenFileName(this, tr("Torrent file to import"), default_dir, tr("Torrent files") + QString(" (*.torrent)"));
    if (!m_torrentPath.isEmpty())
        loadTorrent(m_torrentPath);
    else
        ui->lineTorrent->clear();
}

void TorrentImportDlg::on_browseContentBtn_clicked()
{
    const QString default_dir = Preferences::instance()->getTorImportLastContentDir();
    bool multifile = (m_torrentInfo.filesCount() > 1);
    QString filePath = Utils::Fs::fromNativePath(m_torrentInfo.filePath(0));
    if (!multifile && (filePath.indexOf('/') != -1))
        multifile = true;

    if (!multifile) {
        // Single file torrent
        const QString file_name = Utils::Fs::fileName(filePath);
        qDebug("Torrent has only one file: %s", qPrintable(file_name));
        QString extension = Utils::Fs::fileExtension(file_name);
        qDebug("File extension is : %s", qPrintable(extension));
        QString filter;
        if (!extension.isEmpty()) {
            extension = extension.toUpper();
            filter = tr("%1 Files", "%1 is a file extension (e.g. PDF)").arg(extension) + " (*." + extension + ")";
        }
        m_contentPath = QFileDialog::getOpenFileName(this, tr("Please provide the location of %1", "%1 is a file name").arg(file_name), default_dir, filter);
        if (m_contentPath.isEmpty() || !QFile(m_contentPath).exists()) {
            m_contentPath = QString::null;
            ui->importBtn->setEnabled(false);
            ui->checkSkipCheck->setEnabled(false);
            return;
        }
        // Update display
        ui->lineContent->setText(Utils::Fs::toNativePath(m_contentPath));
        // Check file size
        const qint64 file_size = QFile(m_contentPath).size();
        if (m_torrentInfo.fileSize(0) == file_size) {
            qDebug("The file size matches, allowing fast seeding...");
            ui->checkSkipCheck->setEnabled(true);
        }
        else {
            qDebug("The file size does not match, forbidding fast seeding...");
            ui->checkSkipCheck->setChecked(false);
            ui->checkSkipCheck->setEnabled(false);
        }
        // Handle file renaming
        QStringList parts = m_contentPath.split("/");
        QString new_file_name = parts.takeLast();
        if (new_file_name != file_name) {
            qDebug("The file has been renamed");
            QStringList new_parts = m_filesPath.first().split("/");
            new_parts.replace(new_parts.count() - 1, new_file_name);
            m_filesPath.replace(0, new_parts.join("/"));
        }
        m_contentPath = parts.join("/");
    }
    else {
        // multiple files torrent
        m_contentPath = QFileDialog::getExistingDirectory(this, tr("Please point to the location of the torrent: %1").arg(m_torrentInfo.name()),
                                                          default_dir);
        if (m_contentPath.isEmpty() || !QDir(m_contentPath).exists()) {
            m_contentPath = QString::null;
            ui->importBtn->setEnabled(false);
            ui->checkSkipCheck->setEnabled(false);
            return;
        }
        // Update the display
        ui->lineContent->setText(Utils::Fs::toNativePath(m_contentPath));
        bool size_mismatch = false;
        QDir content_dir(m_contentPath);
        content_dir.cdUp();
        // Check file sizes
        for (int i = 0; i < m_torrentInfo.filesCount(); ++i) {
            const QString rel_path = m_torrentInfo.filePath(i);
            if (QFile(Utils::Fs::expandPath(content_dir.absoluteFilePath(rel_path))).size() != m_torrentInfo.fileSize(i)) {
                qDebug("%s is %lld",
                       qPrintable(Utils::Fs::expandPath(content_dir.absoluteFilePath(rel_path))), (long long int) QFile(Utils::Fs::expandPath(content_dir.absoluteFilePath(rel_path))).size());
                qDebug("%s is %lld",
                       qPrintable(rel_path), (long long int)m_torrentInfo.fileSize(i));
                size_mismatch = true;
                break;
            }
        }
        if (size_mismatch) {
            qDebug("The file size does not match, forbidding fast seeding...");
            ui->checkSkipCheck->setChecked(false);
            ui->checkSkipCheck->setEnabled(false);
        }
        else {
            qDebug("The file size matches, allowing fast seeding...");
            ui->checkSkipCheck->setEnabled(true);
        }
    }
    // Enable the import button
    ui->importBtn->setEnabled(true);
}

void TorrentImportDlg::on_importBtn_clicked()
{
    saveSettings();
    // Has to be accept() and not close()
    // or the torrent won't be imported
    accept();
}

QString TorrentImportDlg::getTorrentPath() const
{
    return m_torrentPath;
}

QString TorrentImportDlg::getContentPath() const
{
    return m_contentPath;
}

void TorrentImportDlg::importTorrent()
{
    qDebug() << Q_FUNC_INFO << "ENTER";
    TorrentImportDlg dlg;
    if (dlg.exec()) {
        BitTorrent::AddTorrentParams params;
        qDebug() << "Loading the torrent file...";
        BitTorrent::TorrentInfo torrentInfo = dlg.torrent();
        if (!torrentInfo.isValid()) return;

        QString torrentPath = dlg.getTorrentPath();
        QString contentPath = dlg.getContentPath();
        if (torrentPath.isEmpty() || contentPath.isEmpty() || !QFile(torrentPath).exists()) {
            qWarning() << "Incorrect input, aborting." << torrentPath << contentPath;
            return;
        }

        const QString hash = torrentInfo.hash();
        qDebug() << "Torrent hash is" << hash;
        params.savePath = contentPath;
        params.skipChecking = dlg.skipFileChecking();
        params.disableTempPath = true;
        qDebug("Adding the torrent to the session...");
        BitTorrent::Session::instance()->addTorrent(torrentInfo, params);
        // Remember the last opened folder
        Preferences* const pref = Preferences::instance();
        pref->setMainLastDir(Utils::Fs::fromNativePath(torrentPath));
        pref->setTorImportLastContentDir(Utils::Fs::fromNativePath(contentPath));
        return;
    }
    qDebug() << Q_FUNC_INFO << "EXIT";
    return;
}

void TorrentImportDlg::loadTorrent(const QString &torrentPath)
{
    // Load the torrent file
    m_torrentInfo = BitTorrent::TorrentInfo::loadFromFile(torrentPath);
    if (!m_torrentInfo.isValid()) {
        ui->browseContentBtn->setEnabled(false);
        ui->lineTorrent->clear();
        QMessageBox::warning(this, tr("Invalid torrent file"), tr("This is not a valid torrent file."));
    }
    else {
        // Update display
        ui->lineTorrent->setText(Utils::Fs::toNativePath(torrentPath));
        ui->browseContentBtn->setEnabled(true);
        // Load the file names
        initializeFilesPath();
    }
}

void TorrentImportDlg::initializeFilesPath()
{
    // Loads files path in the torrent
    m_filesPath = m_torrentInfo.filePaths();
}

bool TorrentImportDlg::fileRenamed() const
{
    return m_fileRenamed;
}


BitTorrent::TorrentInfo TorrentImportDlg::torrent() const
{
    return m_torrentInfo;
}

bool TorrentImportDlg::skipFileChecking() const
{
    return ui->checkSkipCheck->isChecked();
}

void TorrentImportDlg::loadSettings()
{
    restoreGeometry(Preferences::instance()->getTorImportGeometry());
}

void TorrentImportDlg::saveSettings()
{
    Preferences::instance()->setTorImportGeometry(saveGeometry());
}

void TorrentImportDlg::closeEvent(QCloseEvent *event)
{
    qDebug() << Q_FUNC_INFO;
    saveSettings();
    QDialog::closeEvent(event);
}
