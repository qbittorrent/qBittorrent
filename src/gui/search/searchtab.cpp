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

#include <QDir>
#include <QMetaEnum>
#include <QTreeView>
#include <QStandardItemModel>
#include <QHeaderView>
#include <QSortFilterProxyModel>
#include <QLabel>
#include <QVBoxLayout>
#ifdef QBT_USES_QT5
#include <QTableView>
#endif

#include "base/utils/misc.h"
#include "base/preferences.h"
#include "base/settingsstorage.h"
#include "guiiconprovider.h"
#include "searchsortmodel.h"
#include "searchlistdelegate.h"
#include "searchwidget.h"
#include "searchtab.h"

namespace
{
#define SETTINGS_KEY(name) "Search/" name

    const QString KEY_FILTER_MODE_SETTING_NAME = SETTINGS_KEY("FilteringMode");
}

SearchTab::SearchTab(SearchWidget *parent)
    : QWidget(parent)
    , m_parent(parent)
{
    setupUi(this);
    retranslateUi(this);

    m_box = static_cast<QVBoxLayout*>(this->layout());
    m_resultsLbl = new QLabel(this);
    m_resultsBrowser = new QTreeView(this);
#ifdef QBT_USES_QT5
    // This hack fixes reordering of first column with Qt5.
    // https://github.com/qtproject/qtbase/commit/e0fc088c0c8bc61dbcaf5928b24986cd61a22777
    QTableView unused;
    unused.setVerticalHeader(m_resultsBrowser->header());
    m_resultsBrowser->header()->setParent(m_resultsBrowser);
    unused.setVerticalHeader(new QHeaderView(Qt::Horizontal));
#endif
    m_resultsBrowser->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_box->addWidget(m_resultsLbl);
    m_box->addWidget(m_resultsBrowser);

    // Set Search results list model
    m_searchListModel = new QStandardItemModel(0, SearchSortModel::NB_SEARCH_COLUMNS, this);
    m_searchListModel->setHeaderData(SearchSortModel::NAME, Qt::Horizontal, tr("Name", "i.e: file name"));
    m_searchListModel->setHeaderData(SearchSortModel::SIZE, Qt::Horizontal, tr("Size", "i.e: file size"));
    m_searchListModel->setHeaderData(SearchSortModel::SEEDS, Qt::Horizontal, tr("Seeders", "i.e: Number of full sources"));
    m_searchListModel->setHeaderData(SearchSortModel::LEECHES, Qt::Horizontal, tr("Leechers", "i.e: Number of partial sources"));
    m_searchListModel->setHeaderData(SearchSortModel::ENGINE_URL, Qt::Horizontal, tr("Search engine"));

    m_proxyModel = new SearchSortModel(this);
    m_proxyModel->setDynamicSortFilter(true);
    m_proxyModel->setSourceModel(m_searchListModel);
    m_resultsBrowser->setModel(m_proxyModel);

    m_searchDelegate = new SearchListDelegate(this);
    m_resultsBrowser->setItemDelegate(m_searchDelegate);

    m_resultsBrowser->hideColumn(SearchSortModel::DL_LINK); // Hide url column
    m_resultsBrowser->hideColumn(SearchSortModel::DESC_LINK);

    m_resultsBrowser->setRootIsDecorated(false);
    m_resultsBrowser->setAllColumnsShowFocus(true);
    m_resultsBrowser->setSortingEnabled(true);

    // Connect signals to slots (search part)
    connect(m_resultsBrowser, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(downloadSelectedItem(const QModelIndex&)));

    // Load last columns width for search results list
    if (!loadColWidthResultsList())
        m_resultsBrowser->header()->resizeSection(0, 275);

    // Sort by Seeds
    m_resultsBrowser->sortByColumn(SearchSortModel::SEEDS, Qt::DescendingOrder);

    fillFilterComboBoxes();

    updateFilter();

    connect(filterMode, SIGNAL(currentIndexChanged(int)), this, SLOT(updateFilter()));
    connect(minSeeds, SIGNAL(editingFinished()), this, SLOT(updateFilter()));
    connect(minSeeds, SIGNAL(valueChanged(int)), this, SLOT(updateFilter()));
    connect(maxSeeds, SIGNAL(editingFinished()), this, SLOT(updateFilter()));
    connect(maxSeeds, SIGNAL(valueChanged(int)), this, SLOT(updateFilter()));
    connect(minSize, SIGNAL(editingFinished()), this, SLOT(updateFilter()));
    connect(minSize, SIGNAL(valueChanged(double)), this, SLOT(updateFilter()));
    connect(maxSize, SIGNAL(editingFinished()), this, SLOT(updateFilter()));
    connect(maxSize, SIGNAL(valueChanged(double)), this, SLOT(updateFilter()));
    connect(minSizeUnit, SIGNAL(currentIndexChanged(int)), this, SLOT(updateFilter()));
    connect(maxSizeUnit, SIGNAL(currentIndexChanged(int)), this, SLOT(updateFilter()));
}

void SearchTab::downloadSelectedItem(const QModelIndex &index)
{
    QString torrentUrl = m_proxyModel->data(m_proxyModel->index(index.row(), SearchSortModel::DL_LINK)).toString();
    setRowColor(index.row(), "blue");
    m_parent->downloadTorrent(torrentUrl);
}

QHeaderView* SearchTab::header() const
{
    return m_resultsBrowser->header();
}

bool SearchTab::loadColWidthResultsList()
{
    QString line = Preferences::instance()->getSearchColsWidth();
    if (line.isEmpty()) return false;

    QStringList widthList = line.split(' ');
    if (widthList.size() > m_searchListModel->columnCount())
        return false;

    unsigned int listSize = widthList.size();
    for (unsigned int i = 0; i < listSize; ++i)
        m_resultsBrowser->header()->resizeSection(i, widthList.at(i).toInt());

    return true;
}

QTreeView* SearchTab::getCurrentTreeView() const
{
    return m_resultsBrowser;
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
void SearchTab::setRowColor(int row, QString color)
{
    m_proxyModel->setDynamicSortFilter(false);
    for (int i = 0; i < m_proxyModel->columnCount(); ++i)
        m_proxyModel->setData(m_proxyModel->index(row, i), QVariant(QColor(color)), Qt::ForegroundRole);

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
    m_resultsLbl->setText(tr("Results (showing <i>%1</i> out of <i>%2</i>):", "i.e: Search results")
                          .arg(filteredResults).arg(totalResults));
}

void SearchTab::updateFilter()
{
    using Utils::Misc::SizeUnit;
    SearchSortModel* filterModel = getCurrentSearchListProxy();
    filterModel->enableNameFilter(filteringMode() == OnlyNames);
    // we update size and seeds filter parameters in the model even if they are disabled
    // because we need to read them from the model when search tabs switch
    filterModel->setSeedsFilter(minSeeds->value(), maxSeeds->value());
    filterModel->setSizeFilter(
        sizeInBytes(minSize->value(), static_cast<SizeUnit>(minSizeUnit->currentIndex())),
        sizeInBytes(maxSize->value(), static_cast<SizeUnit>(maxSizeUnit->currentIndex())));

    SettingsStorage::instance()->storeValue(KEY_FILTER_MODE_SETTING_NAME,
                                            filterMode->itemData(filterMode->currentIndex()));
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

    minSizeUnit->clear();
    maxSizeUnit->clear();
    minSizeUnit->addItems(unitStrings);
    maxSizeUnit->addItems(unitStrings);

    minSize->setValue(0);
    minSizeUnit->setCurrentIndex(static_cast<int>(SizeUnit::MebiByte));

    maxSize->setValue(-1);
    maxSizeUnit->setCurrentIndex(static_cast<int>(SizeUnit::TebiByte));

    filterMode->clear();

    QMetaEnum nameFilteringModeEnum =
        this->metaObject()->enumerator(this->metaObject()->indexOfEnumerator("NameFilteringMode"));

    filterMode->addItem(tr("Torrent names only"), nameFilteringModeEnum.valueToKey(OnlyNames));
    filterMode->addItem(tr("Everywhere"), nameFilteringModeEnum.valueToKey(Everywhere));

    QVariant selectedMode = SettingsStorage::instance()->loadValue(
                KEY_FILTER_MODE_SETTING_NAME, nameFilteringModeEnum.valueToKey(OnlyNames));
    int index = filterMode->findData(selectedMode);
    filterMode->setCurrentIndex(index == -1 ? 0 : index);
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
    return static_cast<NameFilteringMode>(metaEnum.keyToValue(filterMode->itemData(filterMode->currentIndex()).toByteArray()));
}
