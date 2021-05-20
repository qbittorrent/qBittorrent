/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015-2022  Vladimir Golovnev <glassez@yandex.ru>
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

#include "indexersdialog.h"

#include <QDropEvent>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QTableView>

#include "base/global.h"
#include "gui/uithememanager.h"
#include "gui/utils.h"
#include "indexeroptionsdialog.h"
#include "ui_indexersdialog.h"

#define SETTINGS_KEY(name) u"SearchIndexersDialog/" name

enum IndexerColumns
{
    INDEXER_NAME,
    INDEXER_URL,
    INDEXER_STATE,
    INDEXER_ID
};

IndexersDialog::IndexersDialog(SearchEngine *searchEngine, QWidget *parent)
    : QDialog(parent)
    , m_ui {new Ui::IndexersDialog}
    , m_searchEngine {searchEngine}
    , m_dialogSize {SETTINGS_KEY(u"Size"_qs)}
    , m_treeHeaderState {SETTINGS_KEY(u"TreeHeaderState"_qs)}
{
    m_ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    // This hack fixes reordering of first column with Qt5.
    // https://github.com/qtproject/qtbase/commit/e0fc088c0c8bc61dbcaf5928b24986cd61a22777
    QTableView unused;
    unused.setVerticalHeader(m_ui->indexersTree->header());
    m_ui->indexersTree->header()->setParent(m_ui->indexersTree);
    unused.setVerticalHeader(new QHeaderView(Qt::Horizontal));

    m_ui->indexersTree->setRootIsDecorated(false);
    m_ui->indexersTree->hideColumn(INDEXER_ID);
    m_ui->indexersTree->header()->setSortIndicator(0, Qt::AscendingOrder);

    m_ui->actionRemove->setIcon(UIThemeManager::instance()->getIcon(u"list-remove"_qs));

    connect(m_ui->indexersTree, &QTreeWidget::customContextMenuRequested, this, &IndexersDialog::displayContextMenu);
    connect(m_ui->indexersTree, &QTreeWidget::itemDoubleClicked, this, &IndexersDialog::onItemDoubleClicked);

    loadIndexers();

    connect(m_searchEngine, &SearchEngine::indexerAdded, this, &IndexersDialog::onIndexerAdded);
    connect(m_searchEngine, &SearchEngine::indexerOptionsChanged, this, &IndexersDialog::onIndexerOptionsChanged);
    connect(m_searchEngine, &SearchEngine::indexerStateChanged, this, &IndexersDialog::onIndexerStateChanged);
    connect(m_searchEngine, &SearchEngine::indexerRemoved, this, &IndexersDialog::onIndexerRemoved);

    if (const QSize dialogSize = m_dialogSize; dialogSize.isValid())
        resize(dialogSize);
    m_ui->indexersTree->header()->restoreState(m_treeHeaderState.get());
}

IndexersDialog::~IndexersDialog()
{
    m_dialogSize = size();
    m_treeHeaderState = m_ui->indexersTree->header()->saveState();
    delete m_ui;
}

void IndexersDialog::onItemDoubleClicked(QTreeWidgetItem *item, const int column)
{
    const QString indexerName = item->text(INDEXER_ID);
    if (column == INDEXER_STATE)
    {
        const IndexerInfo indexerInfo = m_searchEngine->indexers().value(indexerName);
        m_searchEngine->enableIndexer(indexerName, !indexerInfo.enabled);
    }
    else
    {
        editIndexer(indexerName);
    }
}

void IndexersDialog::displayContextMenu(const QPoint &)
{
    // Enable/disable pause/start action given the DL state
    const QList<QTreeWidgetItem *> items = m_ui->indexersTree->selectedItems();
    if (items.isEmpty()) return;

    auto contextMenu = new QMenu(this);
    contextMenu->setAttribute(Qt::WA_DeleteOnClose);

    const QString firstIndexerName = items.first()->text(INDEXER_ID);

    m_ui->actionEnable->setChecked(m_searchEngine->indexers().value(firstIndexerName).enabled);
    contextMenu->addAction(m_ui->actionEnable);

    if (items.size() == 1)
        contextMenu->addAction(m_ui->actionEdit);

    contextMenu->addSeparator();
    contextMenu->addAction(m_ui->actionRemove);

    contextMenu->popup(QCursor::pos());
}

void IndexersDialog::on_addButton_clicked()
{
    auto dialog = new IndexerOptionsDialog(this);
    dialog->setModal(true);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &QDialog::accepted, this, [this, dialog]()
    {
        m_searchEngine->addIndexer(dialog->indexerName(), dialog->indexerOptions());
    });

    dialog->open();
}

void IndexersDialog::on_closeButton_clicked()
{
    close();
}

void IndexersDialog::on_actionEnable_toggled(bool enabled)
{
    for (QTreeWidgetItem *item : asConst(m_ui->indexersTree->selectedItems()))
    {
        Q_ASSERT(m_ui->indexersTree->indexOfTopLevelItem(item) != -1);

        const QString indexerName = item->text(INDEXER_ID);
        m_searchEngine->enableIndexer(indexerName, enabled);
    }
}

void IndexersDialog::editIndexer(const QString &indexerName)
{
    auto dialog = new IndexerOptionsDialog(this);
    dialog->setModal(true);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setIndexerName(indexerName);
    dialog->setIndexerOptions(m_searchEngine->indexers().value(indexerName).options);
    connect(dialog, &QDialog::accepted, this, [this, dialog]()
    {
        m_searchEngine->setIndexerOptions(dialog->indexerName(), dialog->indexerOptions());
    });

    dialog->open();
}

void IndexersDialog::on_actionEdit_triggered()
{
    Q_ASSERT(m_ui->indexersTree->selectedItems().size() == 1);

    QTreeWidgetItem *item = m_ui->indexersTree->selectedItems().first();
    Q_ASSERT(m_ui->indexersTree->indexOfTopLevelItem(item) != -1);

    const QString indexerName = item->text(INDEXER_ID);
    editIndexer(indexerName);
}

void IndexersDialog::on_actionRemove_triggered()
{
    for (QTreeWidgetItem *item : asConst(m_ui->indexersTree->selectedItems()))
    {
        Q_ASSERT(m_ui->indexersTree->indexOfTopLevelItem(item) != -1);

        const QString indexerName = item->text(INDEXER_ID);
        m_searchEngine->removeIndexer(indexerName);
    }
}

void IndexersDialog::onIndexerAdded(const QString &indexerName)
{
    createItem(indexerName, m_searchEngine->indexers().value(indexerName));
}

void IndexersDialog::onIndexerOptionsChanged(const QString &indexerName)
{
    QTreeWidgetItem *item = findItemByName(indexerName);
    Q_ASSERT(item);

    item->setText(INDEXER_URL, m_searchEngine->indexers().value(indexerName).options.url);
}

void IndexersDialog::onIndexerStateChanged(const QString &indexerName)
{
    QTreeWidgetItem *item = findItemByName(indexerName);
    Q_ASSERT(item);

    const bool isIndexerEnabled = m_searchEngine->indexers().value(indexerName).enabled;
    item->setText(INDEXER_STATE, (isIndexerEnabled ? tr("Yes") : tr("No")));
}

void IndexersDialog::onIndexerRemoved(const QString &indexerName)
{
    QTreeWidgetItem *item = findItemByName(indexerName);
    Q_ASSERT(item);

    delete item;
}

QTreeWidgetItem *IndexersDialog::findItemByName(const QString &indexerName) const
{
    for (int i = 0; i < m_ui->indexersTree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *item = m_ui->indexersTree->topLevelItem(i);
        if (indexerName == item->text(INDEXER_ID))
            return item;
    }

    return nullptr;
}

void IndexersDialog::loadIndexers()
{
    // Some clean up first
    m_ui->indexersTree->clear();

    const auto indexers = m_searchEngine->indexers();
    for (auto it = indexers.cbegin(); it != indexers.cend(); ++it)
        createItem(it.key(), it.value());
}

void IndexersDialog::createItem(const QString &indexerName, const IndexerInfo &indexerInfo)
{
    auto item = new QTreeWidgetItem(m_ui->indexersTree);
    item->setText(INDEXER_NAME, indexerName);
    item->setText(INDEXER_URL, indexerInfo.options.url);
    item->setText(INDEXER_ID, indexerName);
    item->setText(INDEXER_STATE, (indexerInfo.enabled ? tr("Yes") : tr("No")));
}
