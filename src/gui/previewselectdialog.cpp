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

#include <QDir>
#include <QFile>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QShowEvent>
#include <QStandardItemModel>
#include <QTableView>

#include "base/bittorrent/torrent.h"
#include "base/preferences.h"
#include "base/utils/fs.h"
#include "base/utils/misc.h"
#include "previewlistdelegate.h"
#include "ui_previewselectdialog.h"
#include "utils.h"

#define SETTINGS_KEY(name) "PreviewSelectDialog/" name

PreviewSelectDialog::PreviewSelectDialog(QWidget *parent, const BitTorrent::Torrent *torrent)
    : QDialog(parent)
    , m_ui(new Ui::PreviewSelectDialog)
    , m_torrent(torrent)
    , m_storeDialogSize(SETTINGS_KEY("Size"))
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    , m_storeTreeHeaderState("GUI/Qt6/" SETTINGS_KEY("HeaderState"))
#else
    , m_storeTreeHeaderState(SETTINGS_KEY("HeaderState"))
#endif
{
    m_ui->setupUi(this);

    m_ui->label->setText(tr("The following files from torrent \"%1\" support previewing, please select one of them:")
        .arg(m_torrent->name()));

    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Preview"));
    connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &PreviewSelectDialog::previewButtonClicked);
    connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_ui->previewList, &QAbstractItemView::doubleClicked, this, &PreviewSelectDialog::previewButtonClicked);

    const Preferences *pref = Preferences::instance();
    // Preview list
    m_previewListModel = new QStandardItemModel(0, NB_COLUMNS, this);
    m_previewListModel->setHeaderData(NAME, Qt::Horizontal, tr("Name"));
    m_previewListModel->setHeaderData(SIZE, Qt::Horizontal, tr("Size"));
    m_previewListModel->setHeaderData(PROGRESS, Qt::Horizontal, tr("Progress"));

    // This hack fixes reordering of first column with Qt5.
    // https://github.com/qtproject/qtbase/commit/e0fc088c0c8bc61dbcaf5928b24986cd61a22777
    QTableView unused;
    unused.setVerticalHeader(m_ui->previewList->header());
    m_ui->previewList->header()->setParent(m_ui->previewList);
    unused.setVerticalHeader(new QHeaderView(Qt::Horizontal));

    m_ui->previewList->setModel(m_previewListModel);
    m_ui->previewList->hideColumn(FILE_INDEX);
    m_listDelegate = new PreviewListDelegate(this);
    m_ui->previewList->setItemDelegate(m_listDelegate);
    m_ui->previewList->setAlternatingRowColors(pref->useAlternatingRowColors());
    // Fill list in
    const QVector<qreal> fp = torrent->filesProgress();
    for (int i = 0; i < torrent->filesCount(); ++i)
    {
        const QString fileName = Utils::Fs::fileName(torrent->filePath(i));
        if (Utils::Misc::isPreviewable(fileName))
        {
            int row = m_previewListModel->rowCount();
            m_previewListModel->insertRow(row);
            m_previewListModel->setData(m_previewListModel->index(row, NAME), fileName);
            m_previewListModel->setData(m_previewListModel->index(row, SIZE), torrent->fileSize(i));
            m_previewListModel->setData(m_previewListModel->index(row, PROGRESS), fp[i]);
            m_previewListModel->setData(m_previewListModel->index(row, FILE_INDEX), i);
        }
    }

    m_previewListModel->sort(NAME);
    m_ui->previewList->header()->setSortIndicator(0, Qt::AscendingOrder);
    m_ui->previewList->selectionModel()->select(m_previewListModel->index(0, NAME), QItemSelectionModel::Select | QItemSelectionModel::Rows);

    // Restore dialog state
    loadWindowState();
}

PreviewSelectDialog::~PreviewSelectDialog()
{
    saveWindowState();

    delete m_ui;
}

void PreviewSelectDialog::previewButtonClicked()
{
    const QModelIndexList selectedIndexes = m_ui->previewList->selectionModel()->selectedRows(FILE_INDEX);
    if (selectedIndexes.isEmpty()) return;

    // Flush data
    m_torrent->flushCache();

    // Only one file should be selected
    const int fileIndex = selectedIndexes.at(0).data().toInt();
    const QString path = QDir(m_torrent->actualStorageLocation()).absoluteFilePath(m_torrent->actualFilePath(fileIndex));
    // File
    if (!QFile::exists(path))
    {
        const bool isSingleFile = (m_previewListModel->rowCount() == 1);
        QWidget *parent = isSingleFile ? this->parentWidget() : this;
        QMessageBox::critical(parent, tr("Preview impossible")
            , tr("Sorry, we can't preview this file: \"%1\".").arg(Utils::Fs::toNativePath(path)));
        if (isSingleFile)
            reject();
        return;
    }

    emit readyToPreviewFile(path);
    accept();
}

void PreviewSelectDialog::saveWindowState()
{
    // Persist dialog size
    m_storeDialogSize = size();
    // Persist TreeView Header state
    m_storeTreeHeaderState = m_ui->previewList->header()->saveState();
}

void PreviewSelectDialog::loadWindowState()
{
    // Restore dialog size
    Utils::Gui::resize(this, m_storeDialogSize);

    // Restore TreeView Header state
    if (!m_storeTreeHeaderState.get().isEmpty())
    {
        m_headerStateInitialized = m_ui->previewList->header()->restoreState(m_storeTreeHeaderState);
    }
}

void PreviewSelectDialog::showEvent(QShowEvent *event)
{
    // event originated from system
    if (event->spontaneous())
    {
        QDialog::showEvent(event);
        return;
    }

    // Default size, have to be called after show(), because width is needed
    // Set Name column width to 60% of TreeView
    if (!m_headerStateInitialized)
    {
        const int nameSize = (m_ui->previewList->size().width() * 0.6);
        m_ui->previewList->header()->resizeSection(0, nameSize);
        m_headerStateInitialized = true;
    }

    // Only one file, no choice
    if (m_previewListModel->rowCount() <= 1)
        previewButtonClicked();
}
