/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015, 2021  Vladimir Golovnev <glassez@yandex.ru>
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

#define SETTINGS_KEY(name) "SearchIndexersDialog/" name

const QString KEY_TREEHEADERSTATE = QStringLiteral(SETTINGS_KEY("TreeHeaderState"));
const QString KEY_DIALOGSIZE = QStringLiteral(SETTINGS_KEY("Size"));

enum IndexerColumns
{
    INDEXER_NAME,
    INDEXER_URL,
    INDEXER_STATE,
    INDEXER_ID
};

IndexersDialog::IndexersDialog(SearchEngine *searchEngine, QWidget *parent)
    : QDialog {parent}
    , m_ui {new Ui::IndexersDialog}
    , m_searchEngine {searchEngine}
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

    m_ui->actionRemove->setIcon(UIThemeManager::instance()->getIcon("list-remove"));

    connect(m_ui->actionEnable, &QAction::toggled, this, &IndexersDialog::enableSelection);
    connect(m_ui->indexersTree, &QTreeWidget::customContextMenuRequested, this, &IndexersDialog::displayContextMenu);
    connect(m_ui->indexersTree, &QTreeWidget::itemDoubleClicked, this, &IndexersDialog::toggleIndexerState);

    loadIndexers();

    connect(m_searchEngine, &SearchEngine::indexerAdded, this, &IndexersDialog::onIndexerAdded);
    connect(m_searchEngine, &SearchEngine::indexerOptionsChanged, this, &IndexersDialog::onIndexerOptionsChanged);
    connect(m_searchEngine, &SearchEngine::indexerStateChanged, this, &IndexersDialog::onIndexerStateChanged);
    connect(m_searchEngine, &SearchEngine::indexerRemoved, this, &IndexersDialog::onIndexerRemoved);

    Utils::Gui::resize(this, SettingsStorage::instance()->loadValue<QSize>(KEY_DIALOGSIZE));
    m_ui->indexersTree->header()->restoreState(SettingsStorage::instance()->loadValue<QByteArray>(KEY_TREEHEADERSTATE));
    show();
}

IndexersDialog::~IndexersDialog()
{
    SettingsStorage::instance()->storeValue(KEY_DIALOGSIZE, size());
    SettingsStorage::instance()->storeValue(KEY_TREEHEADERSTATE, m_ui->indexersTree->header()->saveState());
    delete m_ui;
}

void IndexersDialog::toggleIndexerState(QTreeWidgetItem *item, int)
{
    const QString indexerName = item->text(INDEXER_ID);
    const IndexerInfo indexerInfo = m_searchEngine->indexers().value(indexerName);
    m_searchEngine->enableIndexer(indexerName, !indexerInfo.enabled);
}

void IndexersDialog::displayContextMenu(const QPoint &)
{
    // Enable/disable pause/start action given the DL state
    const QList<QTreeWidgetItem *> items = m_ui->indexersTree->selectedItems();
    if (items.isEmpty()) return;

    QMenu *myContextMenu = new QMenu(this);
    myContextMenu->setAttribute(Qt::WA_DeleteOnClose);

    const QString firstIndexerName = items.first()->text(INDEXER_ID);
    m_ui->actionEnable->setChecked(m_searchEngine->indexers().value(firstIndexerName).enabled);
    myContextMenu->addAction(m_ui->actionEnable);
    myContextMenu->addSeparator();
    myContextMenu->addAction(m_ui->actionRemove);

    myContextMenu->popup(QCursor::pos());
}

void IndexersDialog::on_closeButton_clicked()
{
    close();
}

void IndexersDialog::on_actionRemove_triggered()
{
    for (QTreeWidgetItem *item : asConst(m_ui->indexersTree->selectedItems()))
    {
        const int index = m_ui->indexersTree->indexOfTopLevelItem(item);
        Q_ASSERT(index != -1);
        const QString indexerName = item->text(INDEXER_ID);
        m_searchEngine->removeIndexer(indexerName);
    }
}

void IndexersDialog::enableSelection(bool enable)
{
    for (QTreeWidgetItem *item : asConst(m_ui->indexersTree->selectedItems()))
    {
        int index = m_ui->indexersTree->indexOfTopLevelItem(item);
        Q_ASSERT(index != -1);
        const QString indexerName = item->text(INDEXER_ID);
        m_searchEngine->enableIndexer(indexerName, enable);
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

    if (m_searchEngine->indexers().value(indexerName).enabled)
    {
        item->setText(INDEXER_STATE, tr("Yes"));
        setItemColor(item, QLatin1String("green"));
    }
    else
    {
        item->setText(INDEXER_STATE, tr("No"));
        setItemColor(item, QLatin1String("red"));
    }
}

void IndexersDialog::onIndexerRemoved(const QString &indexerName)
{
    QTreeWidgetItem *item = findItemByName(indexerName);
    Q_ASSERT(item);

    delete item;
}

// Set the color of a row in data model
void IndexersDialog::setItemColor(QTreeWidgetItem *item, const QString &colorName)
{
    const QColor color {colorName};
    for (int i = 0; i < m_ui->indexersTree->columnCount(); ++i)
    {
        item->setData(i, Qt::ForegroundRole, color);
    }
}

QTreeWidgetItem *IndexersDialog::findItemByName(const QString &indexerName)
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
    if (indexerInfo.enabled)
    {
        item->setText(INDEXER_STATE, tr("Yes"));
        setItemColor(item, QLatin1String("green"));
    }
    else
    {
        item->setText(INDEXER_STATE, tr("No"));
        setItemColor(item, QLatin1String("red"));
    }
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
