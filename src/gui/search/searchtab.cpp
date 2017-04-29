/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez
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

#include <QApplication>
#include <QDir>
#include <QMenu>
#include <QMetaEnum>
#include <QTreeView>
#include <QStandardItemModel>
#include <QHeaderView>
#include <QSortFilterProxyModel>
#include <QLabel>
#include <QPalette>
#include <QVBoxLayout>
#include <QTableView>

#include "base/utils/misc.h"
#include "base/preferences.h"
#include "base/settingsstorage.h"
#include "guiiconprovider.h"
#include "searchsortmodel.h"
#include "searchlistdelegate.h"
#include "searchwidget.h"
#include "searchtab.h"
#include "ui_searchtab.h"

namespace
{
#define SETTINGS_KEY(name) "Search/" name

    const QString KEY_FILTER_MODE_SETTING_NAME = SETTINGS_KEY("FilteringMode");
}

SearchTab::SearchTab(SearchWidget *parent)
    : QWidget(parent)
    , m_ui(new Ui::SearchTab())
    , m_parent(parent)
{
    m_ui->setupUi(this);

    // This hack fixes reordering of first column with Qt5.
    // https://github.com/qtproject/qtbase/commit/e0fc088c0c8bc61dbcaf5928b24986cd61a22777
    QTableView unused;
    unused.setVerticalHeader(m_ui->resultsBrowser->header());
    m_ui->resultsBrowser->header()->setParent(m_ui->resultsBrowser);
    unused.setVerticalHeader(new QHeaderView(Qt::Horizontal));

    loadSettings();
    m_ui->resultsBrowser->setSelectionMode(QAbstractItemView::ExtendedSelection);
    header()->setStretchLastSection(false);

    // Set Search results list model
    m_searchListModel = new QStandardItemModel(0, SearchSortModel::NB_SEARCH_COLUMNS, this);
    m_searchListModel->setHeaderData(SearchSortModel::NAME, Qt::Horizontal, tr("Name", "i.e: file name"));
    m_searchListModel->setHeaderData(SearchSortModel::SIZE, Qt::Horizontal, tr("Size", "i.e: file size"));
    m_searchListModel->setHeaderData(SearchSortModel::SEEDS, Qt::Horizontal, tr("Seeders", "i.e: Number of full sources"));
    m_searchListModel->setHeaderData(SearchSortModel::LEECHES, Qt::Horizontal, tr("Leechers", "i.e: Number of partial sources"));
    m_searchListModel->setHeaderData(SearchSortModel::ENGINE_URL, Qt::Horizontal, tr("Search engine"));
    // Set columns text alignment
    m_searchListModel->setHeaderData(SearchSortModel::SIZE, Qt::Horizontal, QVariant(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    m_searchListModel->setHeaderData(SearchSortModel::SEEDS, Qt::Horizontal, QVariant(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    m_searchListModel->setHeaderData(SearchSortModel::LEECHES, Qt::Horizontal, QVariant(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);

    m_proxyModel = new SearchSortModel(this);
    m_proxyModel->setDynamicSortFilter(true);
    m_proxyModel->setSourceModel(m_searchListModel);
    m_ui->resultsBrowser->setModel(m_proxyModel);

    m_searchDelegate = new SearchListDelegate(this);
    m_ui->resultsBrowser->setItemDelegate(m_searchDelegate);

    m_ui->resultsBrowser->hideColumn(SearchSortModel::DL_LINK); // Hide url column
    m_ui->resultsBrowser->hideColumn(SearchSortModel::DESC_LINK);

    m_ui->resultsBrowser->setRootIsDecorated(false);
    m_ui->resultsBrowser->setAllColumnsShowFocus(true);
    m_ui->resultsBrowser->setSortingEnabled(true);

    //Ensure that at least one column is visible at all times
    bool atLeastOne = false;
    for (unsigned int i = 0; i < SearchSortModel::DL_LINK; i++) {
        if (!m_ui->resultsBrowser->isColumnHidden(i)) {
            atLeastOne = true;
            break;
        }
    }
    if (!atLeastOne)
        m_ui->resultsBrowser->setColumnHidden(SearchSortModel::NAME, false);
    //To also mitigate the above issue, we have to resize each column when
    //its size is 0, because explicitly 'showing' the column isn't enough
    //in the above scenario.
    for (unsigned int i = 0; i < SearchSortModel::DL_LINK; i++)
        if ((m_ui->resultsBrowser->columnWidth(i) <= 0) && !m_ui->resultsBrowser->isColumnHidden(i))
            m_ui->resultsBrowser->resizeColumnToContents(i);

    // Connect signals to slots (search part)
    connect(m_ui->resultsBrowser, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(downloadItem(const QModelIndex&)));

    header()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(header(), SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(displayToggleColumnsMenu(const QPoint &)));
    connect(header(), SIGNAL(sectionResized(int, int, int)), this, SLOT(saveSettings()));
    connect(header(), SIGNAL(sectionMoved(int, int, int)), this, SLOT(saveSettings()));
    connect(header(), SIGNAL(sortIndicatorChanged(int, Qt::SortOrder)), this, SLOT(saveSettings()));

    fillFilterComboBoxes();

    updateFilter();

    connect(m_ui->filterMode, SIGNAL(currentIndexChanged(int)), this, SLOT(updateFilter()));
    connect(m_ui->minSeeds, SIGNAL(editingFinished()), this, SLOT(updateFilter()));
    connect(m_ui->minSeeds, SIGNAL(valueChanged(int)), this, SLOT(updateFilter()));
    connect(m_ui->maxSeeds, SIGNAL(editingFinished()), this, SLOT(updateFilter()));
    connect(m_ui->maxSeeds, SIGNAL(valueChanged(int)), this, SLOT(updateFilter()));
    connect(m_ui->minSize, SIGNAL(editingFinished()), this, SLOT(updateFilter()));
    connect(m_ui->minSize, SIGNAL(valueChanged(double)), this, SLOT(updateFilter()));
    connect(m_ui->maxSize, SIGNAL(editingFinished()), this, SLOT(updateFilter()));
    connect(m_ui->maxSize, SIGNAL(valueChanged(double)), this, SLOT(updateFilter()));
    connect(m_ui->minSizeUnit, SIGNAL(currentIndexChanged(int)), this, SLOT(updateFilter()));
    connect(m_ui->maxSizeUnit, SIGNAL(currentIndexChanged(int)), this, SLOT(updateFilter()));
}

SearchTab::~SearchTab()
{
    saveSettings();
    delete m_ui;
}

void SearchTab::downloadItem(const QModelIndex &index)
{
    QString torrentUrl = m_proxyModel->data(m_proxyModel->index(index.row(), SearchSortModel::DL_LINK)).toString();
    QString siteUrl = m_proxyModel->data(m_proxyModel->index(index.row(), SearchSortModel::ENGINE_URL)).toString();
    setRowColor(index.row(), QApplication::palette().color(QPalette::LinkVisited));
    m_parent->downloadTorrent(siteUrl, torrentUrl);
}

QHeaderView* SearchTab::header() const
{
    return m_ui->resultsBrowser->header();
}

QTreeView* SearchTab::getCurrentTreeView() const
{
    return m_ui->resultsBrowser;
}

SearchSortModel* SearchTab::getCurrentSearchListProxy() const
{
    return m_proxyModel;
}

QStandardItemModel* SearchTab::getCurrentSearchListModel() const
{
    return m_searchListModel;
}

// Set the color of a row in data model
void SearchTab::setRowColor(int row, const QColor &color)
{
    m_proxyModel->setDynamicSortFilter(false);
    for (int i = 0; i < m_proxyModel->columnCount(); ++i)
        m_proxyModel->setData(m_proxyModel->index(row, i), color, Qt::ForegroundRole);

    m_proxyModel->setDynamicSortFilter(true);
}

SearchTab::Status SearchTab::status() const
{
    return m_status;
}

void SearchTab::setStatus(Status value)
{
    m_status = value;
    setStatusTip(statusText(value));
    const int thisTabIndex = m_parent->searchTabs()->indexOf(this);
    m_parent->searchTabs()->setTabToolTip(thisTabIndex, statusTip());
    m_parent->searchTabs()->setTabIcon(thisTabIndex, GuiIconProvider::instance()->getIcon(statusIconName(value)));
}

void SearchTab::updateResultsCount()
{
    const int totalResults = getCurrentSearchListModel() ? getCurrentSearchListModel()->rowCount(QModelIndex()) : 0;
    const int filteredResults = getCurrentSearchListProxy() ? getCurrentSearchListProxy()->rowCount(QModelIndex()) : totalResults;
    m_ui->resultsLbl->setText(tr("Results (showing <i>%1</i> out of <i>%2</i>):", "i.e: Search results")
                              .arg(filteredResults).arg(totalResults));
}

void SearchTab::updateFilter()
{
    using Utils::Misc::SizeUnit;
    SearchSortModel* filterModel = getCurrentSearchListProxy();
    filterModel->enableNameFilter(filteringMode() == OnlyNames);
    // we update size and seeds filter parameters in the model even if they are disabled
    filterModel->setSeedsFilter(m_ui->minSeeds->value(), m_ui->maxSeeds->value());
    filterModel->setSizeFilter(
        sizeInBytes(m_ui->minSize->value(), static_cast<SizeUnit>(m_ui->minSizeUnit->currentIndex())),
        sizeInBytes(m_ui->maxSize->value(), static_cast<SizeUnit>(m_ui->maxSizeUnit->currentIndex())));

    SettingsStorage::instance()->storeValue(KEY_FILTER_MODE_SETTING_NAME,
                                            m_ui->filterMode->itemData(m_ui->filterMode->currentIndex()));

    filterModel->invalidate();
    updateResultsCount();
}

void SearchTab::fillFilterComboBoxes()
{
    using Utils::Misc::SizeUnit;
    QStringList unitStrings;
    unitStrings.append(unitString(SizeUnit::Byte));
    unitStrings.append(unitString(SizeUnit::KibiByte));
    unitStrings.append(unitString(SizeUnit::MebiByte));
    unitStrings.append(unitString(SizeUnit::GibiByte));
    unitStrings.append(unitString(SizeUnit::TebiByte));
    unitStrings.append(unitString(SizeUnit::PebiByte));
    unitStrings.append(unitString(SizeUnit::ExbiByte));

    m_ui->minSizeUnit->clear();
    m_ui->maxSizeUnit->clear();
    m_ui->minSizeUnit->addItems(unitStrings);
    m_ui->maxSizeUnit->addItems(unitStrings);

    m_ui->minSize->setValue(0);
    m_ui->minSizeUnit->setCurrentIndex(static_cast<int>(SizeUnit::MebiByte));

    m_ui->maxSize->setValue(-1);
    m_ui->maxSizeUnit->setCurrentIndex(static_cast<int>(SizeUnit::GibiByte));

    m_ui->filterMode->clear();

    QMetaEnum nameFilteringModeEnum =
        this->metaObject()->enumerator(this->metaObject()->indexOfEnumerator("NameFilteringMode"));

    m_ui->filterMode->addItem(tr("Torrent names only"), nameFilteringModeEnum.valueToKey(OnlyNames));
    m_ui->filterMode->addItem(tr("Everywhere"), nameFilteringModeEnum.valueToKey(Everywhere));

    QVariant selectedMode = SettingsStorage::instance()->loadValue(
                KEY_FILTER_MODE_SETTING_NAME, nameFilteringModeEnum.valueToKey(OnlyNames));
    int index = m_ui->filterMode->findData(selectedMode);
    m_ui->filterMode->setCurrentIndex(index == -1 ? 0 : index);
}

QString SearchTab::statusText(SearchTab::Status st)
{
    switch (st) {
    case Status::Ongoing:
        return tr("Searching...");
    case Status::Finished:
        return tr("Search has finished");
    case Status::Aborted:
        return tr("Search aborted");
    case Status::Error:
        return tr("An error occurred during search...");
    case Status::NoResults:
        return tr("Search returned no results");
    default:
        return QString();
    }
}

QString SearchTab::statusIconName(SearchTab::Status st)
{
    switch (st) {
    case Status::Ongoing:
        return QLatin1String("task-ongoing");
    case Status::Finished:
        return QLatin1String("task-complete");
    case Status::Aborted:
        return QLatin1String("task-reject");
    case Status::Error:
        return QLatin1String("task-attention");
    case Status::NoResults:
        return QLatin1String("task-attention");
    default:
        return QString();
    }
}

SearchTab::NameFilteringMode SearchTab::filteringMode() const
{
    QMetaEnum metaEnum =
        this->metaObject()->enumerator(this->metaObject()->indexOfEnumerator("NameFilteringMode"));
    return static_cast<NameFilteringMode>(metaEnum.keyToValue(m_ui->filterMode->itemData(m_ui->filterMode->currentIndex()).toByteArray()));
}

void SearchTab::loadSettings()
{
    header()->restoreState(Preferences::instance()->getSearchTabHeaderState());
}

void SearchTab::saveSettings() const
{
    Preferences::instance()->setSearchTabHeaderState(header()->saveState());
}

void SearchTab::displayToggleColumnsMenu(const QPoint&)
{
    QMenu hideshowColumn(this);
    hideshowColumn.setTitle(tr("Column visibility"));
    QList<QAction*> actions;
    for (int i = 0; i < SearchSortModel::DL_LINK; ++i) {
        QAction *myAct = hideshowColumn.addAction(m_searchListModel->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString());
        myAct->setCheckable(true);
        myAct->setChecked(!m_ui->resultsBrowser->isColumnHidden(i));
        actions.append(myAct);
    }
    int visibleCols = 0;
    for (unsigned int i = 0; i < SearchSortModel::DL_LINK; i++) {
        if (!m_ui->resultsBrowser->isColumnHidden(i))
            visibleCols++;

        if (visibleCols > 1)
            break;
    }

    // Call menu
    QAction *act = hideshowColumn.exec(QCursor::pos());
    if (act) {
        int col = actions.indexOf(act);
        Q_ASSERT(col >= 0);
        Q_ASSERT(visibleCols > 0);
        if ((!m_ui->resultsBrowser->isColumnHidden(col)) && (visibleCols == 1))
            return;
        qDebug("Toggling column %d visibility", col);
        m_ui->resultsBrowser->setColumnHidden(col, !m_ui->resultsBrowser->isColumnHidden(col));
        if ((!m_ui->resultsBrowser->isColumnHidden(col)) && (m_ui->resultsBrowser->columnWidth(col) <= 5))
            m_ui->resultsBrowser->setColumnWidth(col, 100);
        saveSettings();
    }
}
