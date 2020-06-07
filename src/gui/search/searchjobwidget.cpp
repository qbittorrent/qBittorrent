/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2018  Vladimir Golovnev <glassez@yandex.ru>
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

#include "searchjobwidget.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QPalette>
#include <QStandardItemModel>
#include <QTableView>
#include <QUrl>

#include "base/bittorrent/session.h"
#include "base/preferences.h"
#include "base/search/searchdownloadhandler.h"
#include "base/search/searchhandler.h"
#include "base/search/searchpluginmanager.h"
#include "base/settingvalue.h"
#include "base/utils/misc.h"
#include "gui/addnewtorrentdialog.h"
#include "gui/lineedit.h"
#include "gui/uithememanager.h"
#include "gui/utils.h"
#include "searchlistdelegate.h"
#include "searchsortmodel.h"
#include "ui_searchjobwidget.h"

SearchJobWidget::SearchJobWidget(SearchHandler *searchHandler, QWidget *parent)
    : QWidget(parent)
    , m_ui(new Ui::SearchJobWidget)
    , m_searchHandler(searchHandler)
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
    m_proxyModel->setNameFilter(searchHandler->pattern());
    m_ui->resultsBrowser->setModel(m_proxyModel);

    m_searchDelegate = new SearchListDelegate(this);
    m_ui->resultsBrowser->setItemDelegate(m_searchDelegate);

    m_ui->resultsBrowser->hideColumn(SearchSortModel::DL_LINK); // Hide url column
    m_ui->resultsBrowser->hideColumn(SearchSortModel::DESC_LINK);

    m_ui->resultsBrowser->setRootIsDecorated(false);
    m_ui->resultsBrowser->setAllColumnsShowFocus(true);
    m_ui->resultsBrowser->setSortingEnabled(true);

    // Ensure that at least one column is visible at all times
    bool atLeastOne = false;
    for (int i = 0; i < SearchSortModel::DL_LINK; ++i) {
        if (!m_ui->resultsBrowser->isColumnHidden(i)) {
            atLeastOne = true;
            break;
        }
    }
    if (!atLeastOne)
        m_ui->resultsBrowser->setColumnHidden(SearchSortModel::NAME, false);
    // To also mitigate the above issue, we have to resize each column when
    // its size is 0, because explicitly 'showing' the column isn't enough
    // in the above scenario.
    for (int i = 0; i < SearchSortModel::DL_LINK; ++i)
        if ((m_ui->resultsBrowser->columnWidth(i) <= 0) && !m_ui->resultsBrowser->isColumnHidden(i))
            m_ui->resultsBrowser->resizeColumnToContents(i);

    header()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(header(), &QWidget::customContextMenuRequested, this, &SearchJobWidget::displayToggleColumnsMenu);
    connect(header(), &QHeaderView::sectionResized, this, &SearchJobWidget::saveSettings);
    connect(header(), &QHeaderView::sectionMoved, this, &SearchJobWidget::saveSettings);
    connect(header(), &QHeaderView::sortIndicatorChanged, this, &SearchJobWidget::saveSettings);

    fillFilterComboBoxes();

    updateFilter();

    m_lineEditSearchResultsFilter = new LineEdit(this);
    m_lineEditSearchResultsFilter->setPlaceholderText(tr("Filter search results..."));
    m_lineEditSearchResultsFilter->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_lineEditSearchResultsFilter, &QWidget::customContextMenuRequested, this, &SearchJobWidget::showFilterContextMenu);
    m_ui->horizontalLayout->insertWidget(0, m_lineEditSearchResultsFilter);

    connect(m_lineEditSearchResultsFilter, &LineEdit::textChanged, this, &SearchJobWidget::filterSearchResults);
    connect(m_ui->filterMode, qOverload<int>(&QComboBox::currentIndexChanged)
            , this, &SearchJobWidget::updateFilter);
    connect(m_ui->minSeeds, &QAbstractSpinBox::editingFinished, this, &SearchJobWidget::updateFilter);
    connect(m_ui->minSeeds, qOverload<int>(&QSpinBox::valueChanged)
            , this, &SearchJobWidget::updateFilter);
    connect(m_ui->maxSeeds, &QAbstractSpinBox::editingFinished, this, &SearchJobWidget::updateFilter);
    connect(m_ui->maxSeeds, qOverload<int>(&QSpinBox::valueChanged)
            , this, &SearchJobWidget::updateFilter);
    connect(m_ui->minSize, &QAbstractSpinBox::editingFinished, this, &SearchJobWidget::updateFilter);
    connect(m_ui->minSize, qOverload<double>(&QDoubleSpinBox::valueChanged)
            , this, &SearchJobWidget::updateFilter);
    connect(m_ui->maxSize, &QAbstractSpinBox::editingFinished, this, &SearchJobWidget::updateFilter);
    connect(m_ui->maxSize, qOverload<double>(&QDoubleSpinBox::valueChanged)
            , this, &SearchJobWidget::updateFilter);
    connect(m_ui->minSizeUnit, qOverload<int>(&QComboBox::currentIndexChanged)
            , this, &SearchJobWidget::updateFilter);
    connect(m_ui->maxSizeUnit, qOverload<int>(&QComboBox::currentIndexChanged)
            , this, &SearchJobWidget::updateFilter);

    connect(m_ui->resultsBrowser, &QAbstractItemView::doubleClicked, this, &SearchJobWidget::onItemDoubleClicked);

    connect(searchHandler, &SearchHandler::newSearchResults, this, &SearchJobWidget::appendSearchResults);
    connect(searchHandler, &SearchHandler::searchFinished, this, &SearchJobWidget::searchFinished);
    connect(searchHandler, &SearchHandler::searchFailed, this, &SearchJobWidget::searchFailed);
    connect(this, &QObject::destroyed, searchHandler, &QObject::deleteLater);

    setStatusTip(statusText(m_status));
}

SearchJobWidget::~SearchJobWidget()
{
    saveSettings();
    delete m_ui;
}

void SearchJobWidget::onItemDoubleClicked(const QModelIndex &index)
{
    downloadTorrent(index);
}

QHeaderView *SearchJobWidget::header() const
{
    return m_ui->resultsBrowser->header();
}

// Set the color of a row in data model
void SearchJobWidget::setRowColor(int row, const QColor &color)
{
    m_proxyModel->setDynamicSortFilter(false);
    for (int i = 0; i < m_proxyModel->columnCount(); ++i)
        m_proxyModel->setData(m_proxyModel->index(row, i), color, Qt::ForegroundRole);

    m_proxyModel->setDynamicSortFilter(true);
}

SearchJobWidget::Status SearchJobWidget::status() const
{
    return m_status;
}

int SearchJobWidget::visibleResultsCount() const
{
    return m_proxyModel->rowCount();
}

LineEdit *SearchJobWidget::lineEditSearchResultsFilter() const
{
    return m_lineEditSearchResultsFilter;
}

void SearchJobWidget::cancelSearch()
{
    m_searchHandler->cancelSearch();
}

void SearchJobWidget::downloadTorrents()
{
    const QModelIndexList rows {m_ui->resultsBrowser->selectionModel()->selectedRows()};
    for (const QModelIndex &rowIndex : rows)
        downloadTorrent(rowIndex);
}

void SearchJobWidget::openTorrentPages() const
{
    const QModelIndexList rows {m_ui->resultsBrowser->selectionModel()->selectedRows()};
    for (const QModelIndex &rowIndex : rows) {
        const QString descrLink = m_proxyModel->data(
                    m_proxyModel->index(rowIndex.row(), SearchSortModel::DESC_LINK)).toString();
        if (!descrLink.isEmpty())
            QDesktopServices::openUrl(QUrl::fromEncoded(descrLink.toUtf8()));
    }
}

void SearchJobWidget::copyTorrentURLs() const
{
    copyField(SearchSortModel::DESC_LINK);
}

void SearchJobWidget::copyTorrentDownloadLinks() const
{
    copyField(SearchSortModel::DL_LINK);
}

void SearchJobWidget::copyTorrentNames() const
{
    copyField(SearchSortModel::NAME);
}

void SearchJobWidget::copyField(const int column) const
{
    const QModelIndexList rows {m_ui->resultsBrowser->selectionModel()->selectedRows()};
    QStringList list;

    for (const QModelIndex &rowIndex : rows) {
        const QString field = m_proxyModel->data(
            m_proxyModel->index(rowIndex.row(), column)).toString();
        if (!field.isEmpty())
            list << field;
    }

    if (!list.empty())
        QApplication::clipboard()->setText(list.join('\n'));
}

void SearchJobWidget::setStatus(Status value)
{
    if (m_status == value) return;

    m_status = value;
    setStatusTip(statusText(value));
    emit statusChanged();
}

void SearchJobWidget::downloadTorrent(const QModelIndex &rowIndex)
{
    const QString torrentUrl = m_proxyModel->data(
                m_proxyModel->index(rowIndex.row(), SearchSortModel::DL_LINK)).toString();
    const QString siteUrl = m_proxyModel->data(
                m_proxyModel->index(rowIndex.row(), SearchSortModel::ENGINE_URL)).toString();

    if (torrentUrl.startsWith("magnet:", Qt::CaseInsensitive)) {
        addTorrentToSession(torrentUrl);
    }
    else {
        SearchDownloadHandler *downloadHandler = m_searchHandler->manager()->downloadTorrent(siteUrl, torrentUrl);
        connect(downloadHandler, &SearchDownloadHandler::downloadFinished, this, &SearchJobWidget::addTorrentToSession);
        connect(downloadHandler, &SearchDownloadHandler::downloadFinished, downloadHandler, &SearchDownloadHandler::deleteLater);
    }
    setRowColor(rowIndex.row(), QApplication::palette().color(QPalette::LinkVisited));
}

void SearchJobWidget::addTorrentToSession(const QString &source)
{
    if (source.isEmpty()) return;

    if (AddNewTorrentDialog::isEnabled())
        AddNewTorrentDialog::show(source, this);
    else
        BitTorrent::Session::instance()->addTorrent(source);
}

void SearchJobWidget::updateResultsCount()
{
    const int totalResults = m_searchListModel->rowCount();
    const int filteredResults = m_proxyModel->rowCount();
    m_ui->resultsLbl->setText(tr("Results (showing <i>%1</i> out of <i>%2</i>):", "i.e: Search results")
                              .arg(filteredResults).arg(totalResults));

    m_noSearchResults = (totalResults == 0);
    emit resultsCountUpdated();
}

void SearchJobWidget::updateFilter()
{
    using Utils::Misc::SizeUnit;

    m_proxyModel->enableNameFilter(filteringMode() == NameFilteringMode::OnlyNames);
    // we update size and seeds filter parameters in the model even if they are disabled
    m_proxyModel->setSeedsFilter(m_ui->minSeeds->value(), m_ui->maxSeeds->value());
    m_proxyModel->setSizeFilter(
        sizeInBytes(m_ui->minSize->value(), static_cast<SizeUnit>(m_ui->minSizeUnit->currentIndex())),
        sizeInBytes(m_ui->maxSize->value(), static_cast<SizeUnit>(m_ui->maxSizeUnit->currentIndex())));

    nameFilteringModeSetting() = filteringMode();

    m_proxyModel->invalidate();
    updateResultsCount();
}

void SearchJobWidget::fillFilterComboBoxes()
{
    using Utils::Misc::SizeUnit;
    using Utils::Misc::unitString;

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

    m_ui->filterMode->addItem(tr("Torrent names only"), static_cast<int>(NameFilteringMode::OnlyNames));
    m_ui->filterMode->addItem(tr("Everywhere"), static_cast<int>(NameFilteringMode::Everywhere));

    QVariant selectedMode = static_cast<int>(nameFilteringModeSetting().value());
    int index = m_ui->filterMode->findData(selectedMode);
    m_ui->filterMode->setCurrentIndex((index == -1) ? 0 : index);
}

void SearchJobWidget::filterSearchResults(const QString &name)
{
    const QRegExp::PatternSyntax patternSyntax = Preferences::instance()->getRegexAsFilteringPatternForSearchJob()
                    ? QRegExp::RegExp : QRegExp::WildcardUnix;
    m_proxyModel->setFilterRegExp(QRegExp(name, Qt::CaseInsensitive, patternSyntax));
    updateResultsCount();
}

void SearchJobWidget::showFilterContextMenu(const QPoint &)
{
    const Preferences *pref = Preferences::instance();

    QMenu *menu = m_lineEditSearchResultsFilter->createStandardContextMenu();
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->addSeparator();

    QAction *useRegexAct = menu->addAction(tr("Use regular expressions"));
    useRegexAct->setCheckable(true);
    useRegexAct->setChecked(pref->getRegexAsFilteringPatternForSearchJob());
    connect(useRegexAct, &QAction::toggled, pref, &Preferences::setRegexAsFilteringPatternForSearchJob);
    connect(useRegexAct, &QAction::toggled, this, [this]() { filterSearchResults(m_lineEditSearchResultsFilter->text()); });

    menu->popup(QCursor::pos());
}

void SearchJobWidget::contextMenuEvent(QContextMenuEvent *event)
{
    auto *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    const QAction *downloadAction = menu->addAction(
        UIThemeManager::instance()->getIcon("download"), tr("Download"));
    connect(downloadAction, &QAction::triggered, this, &SearchJobWidget::downloadTorrents);

    menu->addSeparator();

    const QAction *openDescriptionAction = menu->addAction(
        UIThemeManager::instance()->getIcon("application-x-mswinurl"), tr("Open description page"));
    connect(openDescriptionAction, &QAction::triggered, this, &SearchJobWidget::openTorrentPages);

    QMenu *copySubMenu = menu->addMenu(
        UIThemeManager::instance()->getIcon("edit-copy"), tr("Copy"));

    const QAction *copyNamesAction = copySubMenu->addAction(
        UIThemeManager::instance()->getIcon("edit-copy"), tr("Name"));
    connect(copyNamesAction, &QAction::triggered, this, &SearchJobWidget::copyTorrentNames);

    const QAction *copyDownloadLinkAction = copySubMenu->addAction(
        UIThemeManager::instance()->getIcon("edit-copy"), tr("Download link"));
    connect(copyDownloadLinkAction, &QAction::triggered
        , this, &SearchJobWidget::copyTorrentDownloadLinks);

    const QAction *copyDescriptionAction = copySubMenu->addAction(
        UIThemeManager::instance()->getIcon("edit-copy"), tr("Description page URL"));
    connect(copyDescriptionAction, &QAction::triggered, this, &SearchJobWidget::copyTorrentURLs);

    menu->popup(event->globalPos());
}

QString SearchJobWidget::statusText(SearchJobWidget::Status st)
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
        return {};
    }
}

SearchJobWidget::NameFilteringMode SearchJobWidget::filteringMode() const
{
    return static_cast<NameFilteringMode>(m_ui->filterMode->itemData(m_ui->filterMode->currentIndex()).toInt());
}

void SearchJobWidget::loadSettings()
{
    header()->restoreState(Preferences::instance()->getSearchTabHeaderState());
}

void SearchJobWidget::saveSettings() const
{
    Preferences::instance()->setSearchTabHeaderState(header()->saveState());
}

void SearchJobWidget::displayToggleColumnsMenu(const QPoint &)
{
    auto menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setTitle(tr("Column visibility"));

    for (int i = 0; i < SearchSortModel::DL_LINK; ++i) {
        QAction *myAct = menu->addAction(m_searchListModel->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString());
        myAct->setCheckable(true);
        myAct->setChecked(!m_ui->resultsBrowser->isColumnHidden(i));
        myAct->setData(i);
    }

    connect(menu, &QMenu::triggered, this, [this](const QAction *action)
    {
        int visibleCols = 0;
        for (int i = 0; i < SearchSortModel::DL_LINK; ++i) {
            if (!m_ui->resultsBrowser->isColumnHidden(i))
                ++visibleCols;

            if (visibleCols > 1)
                break;
        }

        const int col = action->data().toInt();

        if ((!m_ui->resultsBrowser->isColumnHidden(col)) && (visibleCols == 1))
            return;

        m_ui->resultsBrowser->setColumnHidden(col, !m_ui->resultsBrowser->isColumnHidden(col));

        if ((!m_ui->resultsBrowser->isColumnHidden(col)) && (m_ui->resultsBrowser->columnWidth(col) <= 5))
            m_ui->resultsBrowser->resizeColumnToContents(col);

        saveSettings();
    });

    menu->popup(QCursor::pos());
}

void SearchJobWidget::searchFinished(bool cancelled)
{
    if (cancelled)
        setStatus(Status::Aborted);
    else if (m_noSearchResults)
        setStatus(Status::NoResults);
    else
        setStatus(Status::Finished);
}

void SearchJobWidget::searchFailed()
{
    setStatus(Status::Error);
}

void SearchJobWidget::appendSearchResults(const QVector<SearchResult> &results)
{
    for (const SearchResult &result : results) {
        // Add item to search result list
        int row = m_searchListModel->rowCount();
        m_searchListModel->insertRow(row);

        m_searchListModel->setData(m_searchListModel->index(row, SearchSortModel::NAME), result.fileName); // Name
        m_searchListModel->setData(m_searchListModel->index(row, SearchSortModel::DL_LINK), result.fileUrl); // download URL
        m_searchListModel->setData(m_searchListModel->index(row, SearchSortModel::SIZE), result.fileSize); // Size
        m_searchListModel->setData(m_searchListModel->index(row, SearchSortModel::SEEDS), result.nbSeeders); // Seeders
        m_searchListModel->setData(m_searchListModel->index(row, SearchSortModel::LEECHES), result.nbLeechers); // Leechers
        m_searchListModel->setData(m_searchListModel->index(row, SearchSortModel::ENGINE_URL), result.siteUrl); // Search site URL
        m_searchListModel->setData(m_searchListModel->index(row, SearchSortModel::DESC_LINK), result.descrLink); // Description Link
    }

    updateResultsCount();
}

CachedSettingValue<SearchJobWidget::NameFilteringMode> &SearchJobWidget::nameFilteringModeSetting()
{
    static CachedSettingValue<NameFilteringMode> setting("Search/FilteringMode", NameFilteringMode::OnlyNames);
    return setting;
}

void SearchJobWidget::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Enter:
    case Qt::Key_Return:
        downloadTorrents();
        break;
    default:
        QWidget::keyPressEvent(event);
    }
}
