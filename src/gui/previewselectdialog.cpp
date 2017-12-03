/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2011  Christophe Dumez <chris@qbittorrent.org>
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

#include "previewselectdialog.h"

#include <QFile>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTableView>

#include "base/preferences.h"
#include "base/utils/fs.h"
#include "base/utils/misc.h"
#include "previewlistdelegate.h"
#include "utils.h"

#define SETTINGS_KEY(name) "PreviewSelectDialog/" name

PreviewSelectDialog::PreviewSelectDialog(QWidget *parent, BitTorrent::TorrentHandle *const torrent)
    : QDialog(parent)
    , m_torrent(torrent)
    , m_storeDialogSize(SETTINGS_KEY("Dimension"))
    , m_storeTreeHeaderState(SETTINGS_KEY("HeaderState"))
{
    setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Preview"));
    connect(buttonBox, &QDialogButtonBox::accepted, this, &PreviewSelectDialog::previewButtonClicked);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    Preferences *const pref = Preferences::instance();
    // Preview list
    m_previewListModel = new QStandardItemModel(0, NB_COLUMNS);
    m_previewListModel->setHeaderData(NAME, Qt::Horizontal, tr("Name"));
    m_previewListModel->setHeaderData(SIZE, Qt::Horizontal, tr("Size"));
    m_previewListModel->setHeaderData(PROGRESS, Qt::Horizontal, tr("Progress"));

    // This hack fixes reordering of first column with Qt5.
    // https://github.com/qtproject/qtbase/commit/e0fc088c0c8bc61dbcaf5928b24986cd61a22777
    QTableView unused;
    unused.setVerticalHeader(previewList->header());
    previewList->header()->setParent(previewList);
    unused.setVerticalHeader(new QHeaderView(Qt::Horizontal));

    previewList->setModel(m_previewListModel);
    previewList->hideColumn(FILE_INDEX);
    m_listDelegate = new PreviewListDelegate(this);
    previewList->setItemDelegate(m_listDelegate);
    previewList->setAlternatingRowColors(pref->useAlternatingRowColors());
    // Fill list in
    QVector<qreal> fp = torrent->filesProgress();
    int nbFiles = torrent->filesCount();
    for (int i = 0; i < nbFiles; ++i) {
        QString fileName = torrent->fileName(i);
        if (fileName.endsWith(QB_EXT))
            fileName.chop(4);
        QString extension = Utils::Fs::fileExtension(fileName).toUpper();
        if (Utils::Misc::isPreviewable(extension)) {
            int row = m_previewListModel->rowCount();
            m_previewListModel->insertRow(row);
            m_previewListModel->setData(m_previewListModel->index(row, NAME), QVariant(fileName));
            m_previewListModel->setData(m_previewListModel->index(row, SIZE), QVariant(torrent->fileSize(i)));
            m_previewListModel->setData(m_previewListModel->index(row, PROGRESS), QVariant(fp[i]));
            m_previewListModel->setData(m_previewListModel->index(row, FILE_INDEX), QVariant(i));
        }
    }

    if (m_previewListModel->rowCount() == 0) {
        QMessageBox::critical(this->parentWidget(), tr("Preview impossible"), tr("Sorry, we can't preview this file"));
        close();
    }
    connect(this, SIGNAL(readyToPreviewFile(QString)), parent, SLOT(previewFile(QString)));
    m_previewListModel->sort(NAME);
    previewList->header()->setSortIndicator(0, Qt::AscendingOrder);
    previewList->selectionModel()->select(m_previewListModel->index(0, NAME), QItemSelectionModel::Select | QItemSelectionModel::Rows);

    // Restore dialog state
    loadWindowState();

    if (m_previewListModel->rowCount() == 1) {
        qDebug("Torrent file only contains one file, no need to display selection dialog before preview");
        // Only one file : no choice
        previewButtonClicked();
    }
    else {
        qDebug("Displaying media file selection dialog for preview");
        show();
    }
}

PreviewSelectDialog::~PreviewSelectDialog()
{
    saveWindowState();

    delete m_previewListModel;
    delete m_listDelegate;
}

void PreviewSelectDialog::previewButtonClicked()
{
    QModelIndexList selectedIndexes = previewList->selectionModel()->selectedRows(FILE_INDEX);
    if (selectedIndexes.size() == 0) return;

    // Flush data
    m_torrent->flushCache();

    QStringList absolutePaths(m_torrent->absoluteFilePaths());
    // Only one file should be selected
    QString path = absolutePaths.at(selectedIndexes.at(0).data().toInt());
    // File
    if (QFile::exists(path))
        emit readyToPreviewFile(path);
    else
        QMessageBox::critical(this->parentWidget(), tr("Preview impossible"), tr("Sorry, we can't preview this file"));

    accept();
}

void PreviewSelectDialog::saveWindowState()
{
    // Persist dialog size
    m_storeDialogSize = size();
    // Persist TreeView Header state
    m_storeTreeHeaderState = previewList->header()->saveState();
}

void PreviewSelectDialog::loadWindowState()
{
    // Restore dialog size
    Utils::Gui::resize(this, m_storeDialogSize);

    // Restore TreeView Header state
    if (!m_storeTreeHeaderState.value().isEmpty()) {
        m_headerStateInitialized = previewList->header()->restoreState(m_storeTreeHeaderState);
    }
}

void PreviewSelectDialog::showEvent(QShowEvent *event)
{
    Q_UNUSED(event);

    // Default size, have to be called after show(), because width is needed
    // Set Name column width to 60% of TreeView
    if (!m_headerStateInitialized) {
        int nameSize = (previewList->size().width() * 0.6);
        previewList->header()->resizeSection(0, nameSize);
        m_headerStateInitialized = true;
    }
}
