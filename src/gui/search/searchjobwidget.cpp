/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2018-2025  Vladimir Golovnev <glassez@yandex.ru>
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
#include <QUrl>

#include "base/preferences.h"
#include "base/search/searchdownloadhandler.h"
#include "base/search/searchhandler.h"
#include "base/search/searchpluginmanager.h"
#include "base/utils/misc.h"
#include "gui/interfaces/iguiapplication.h"
#include "gui/lineedit.h"
#include "gui/uithememanager.h"
#include "searchsortmodel.h"
#include "ui_searchjobwidget.h"

namespace
{
    enum DataRole
    {
        LinkVisitedRole = Qt::UserRole + 100
    };

    QColor visitedRowColor()
    {
        return QApplication::palette().color(QPalette::Disabled, QPalette::WindowText);
    }

    QString statusText(SearchJobWidget::Status st)
    {
        switch (st)
        {
        case SearchJobWidget::Status::Ongoing:
            return SearchJobWidget::tr("Searching...");
        case SearchJobWidget::Status::Finished:
            return SearchJobWidget::tr("Search has finished");
        case SearchJobWidget::Status::Aborted:
            return SearchJobWidget::tr("Search aborted");
        case SearchJobWidget::Status::Error:
            return SearchJobWidget::tr("An error occurred during search...");
        case SearchJobWidget::Status::NoResults:
            return SearchJobWidget::tr("Search returned no results");
        default:
            return {};
        }
    }
}

SearchJobWidget::SearchJobWidget(const QString &id, IGUIApplication *app, QWidget *parent)
    : GUIApplicationComponent(app, parent)
    , m_nameFilteringMode {u"Search/FilteringMode"_s}
    , m_id {id}
    , m_ui {new Ui::SearchJobWidget}
{
    m_ui->setupUi(this);

    loadSettings();

    header()->setFirstSectionMovable(true);
    header()->setStretchLastSection(false);
    header()->setTextElideMode(Qt::ElideRight);

    // Set Search results list model
    m_searchListModel = new QStandardItemModel(0, SearchSortModel::NB_SEARCH_COLUMNS, this);
    m_searchListModel->setHeaderData(SearchSortModel::NAME, Qt::Horizontal, tr("Name", "i.e: file name"));
    m_searchListModel->setHeaderData(SearchSortModel::SIZE, Qt::Horizontal, tr("Size", "i.e: file size"));
    m_searchListModel->setHeaderData(SearchSortModel::SEEDS, Qt::Horizontal, tr("Seeders", "i.e: Number of full sources"));
    m_searchListModel->setHeaderData(SearchSortModel::LEECHES, Qt::Horizontal, tr("Leechers", "i.e: Number of partial sources"));
    m_searchListModel->setHeaderData(SearchSortModel::ENGINE_NAME, Qt::Horizontal, tr("Engine"));
    m_searchListModel->setHeaderData(SearchSortModel::ENGINE_URL, Qt::Horizontal, tr("Engine URL"));
    m_searchListModel->setHeaderData(SearchSortModel::PUB_DATE, Qt::Horizontal, tr("Published On"));
    // Set columns text alignment
    m_searchListModel->setHeaderData(SearchSortModel::SIZE, Qt::Horizontal, QVariant(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    m_searchListModel->setHeaderData(SearchSortModel::SEEDS, Qt::Horizontal, QVariant(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);
    m_searchListModel->setHeaderData(SearchSortModel::LEECHES, Qt::Horizontal, QVariant(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);

    m_proxyModel = new SearchSortModel(this);
    m_proxyModel->setDynamicSortFilter(true);
    m_proxyModel->setSourceModel(m_searchListModel);
    m_ui->resultsBrowser->setModel(m_proxyModel);

    m_ui->resultsBrowser->hideColumn(SearchSortModel::DL_LINK); // Hide url column
    m_ui->resultsBrowser->hideColumn(SearchSortModel::DESC_LINK);

    m_ui->resultsBrowser->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_ui->resultsBrowser->setRootIsDecorated(false);
    m_ui->resultsBrowser->setAllColumnsShowFocus(true);
    m_ui->resultsBrowser->setSortingEnabled(true);
    m_ui->resultsBrowser->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Ensure that at least one column is visible at all times
    bool atLeastOne = false;
    for (int i = 0; i < SearchSortModel::DL_LINK; ++i)
    {
        if (!m_ui->resultsBrowser->isColumnHidden(i))
        {
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
    {
        if ((m_ui->resultsBrowser->columnWidth(i) <= 0) && !m_ui->resultsBrowser->isColumnHidden(i))
            m_ui->resultsBrowser->resizeColumnToContents(i);
    }

    header()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(header(), &QWidget::customContextMenuRequested, this, &SearchJobWidget::displayColumnHeaderMenu);
    connect(header(), &QHeaderView::sectionResized, this, &SearchJobWidget::saveSettings);
    connect(header(), &QHeaderView::sectionMoved, this, &SearchJobWidget::saveSettings);
    connect(header(), &QHeaderView::sortIndicatorChanged, this, &SearchJobWidget::saveSettings);

    fillFilterComboBoxes();

    m_lineEditSearchResultsFilter = new LineEdit(this);
    m_lineEditSearchResultsFilter->setPlaceholderText(tr("Filter search results..."));
    m_lineEditSearchResultsFilter->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_lineEditSearchResultsFilter, &QWidget::customContextMenuRequested, this, &SearchJobWidget::showFilterContextMenu);
    connect(m_lineEditSearchResultsFilter, &LineEdit::textChanged, this, &SearchJobWidget::filterSearchResults);
    m_ui->horizontalLayout->insertWidget(0, m_lineEditSearchResultsFilter);

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

    connect(UIThemeManager::instance(), &UIThemeManager::themeChanged, this, &SearchJobWidget::onUIThemeChanged);
}

SearchJobWidget::SearchJobWidget(const QString &id, const QString &searchPattern
        , const QList<SearchResult> &searchResults, IGUIApplication *app, QWidget *parent)
    : SearchJobWidget(id, app, parent)
{
    m_searchPattern = searchPattern;
    m_proxyModel->setNameFilter(m_searchPattern);
    updateFilter();

    appendSearchResults(searchResults);
}

SearchJobWidget::SearchJobWidget(const QString &id, SearchHandler *searchHandler, IGUIApplication *app, QWidget *parent)
    : SearchJobWidget(id, app, parent)
{
    assignSearchHandler(searchHandler);
}

SearchJobWidget::~SearchJobWidget()
{
    saveSettings();
    delete m_ui;
}

QString SearchJobWidget::id() const
{
    return m_id;
}

QString SearchJobWidget::searchPattern() const
{
    return m_searchPattern;
}

QList<SearchResult> SearchJobWidget::searchResults() const
{
    return m_searchResults;
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
    for (int i = 0; i < m_proxyModel->columnCount(); ++i)
        m_proxyModel->setData(m_proxyModel->index(row, i), color, Qt::ForegroundRole);
}

void SearchJobWidget::setRowVisited(const int row)
{
    m_proxyModel->setDynamicSortFilter(false);

    m_proxyModel->setData(m_proxyModel->index(row, 0), true, LinkVisitedRole);
    setRowColor(row, visitedRowColor());

    m_proxyModel->setDynamicSortFilter(true);
}

void SearchJobWidget::onUIThemeChanged()
{
    m_proxyModel->setDynamicSortFilter(false);

    for (int row = 0; row < m_proxyModel->rowCount(); ++row)
    {
        const QVariant userData = m_proxyModel->data(m_proxyModel->index(row, 0), LinkVisitedRole);
        const bool isVisited = userData.toBool();
        if (isVisited)
            setRowColor(row, visitedRowColor());
    }

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

void SearchJobWidget::assignSearchHandler(SearchHandler *searchHandler)
{
    Q_ASSERT(searchHandler);
    if (!searchHandler) [[unlikely]]
        return;

    m_searchResults.clear();
    m_searchListModel->removeRows(0, m_searchListModel->rowCount());
    delete m_searchHandler;

    m_searchHandler = searchHandler;
    m_searchHandler->setParent(this);
    connect(m_searchHandler, &SearchHandler::newSearchResults, this, &SearchJobWidget::appendSearchResults);
    connect(m_searchHandler, &SearchHandler::searchFinished, this, &SearchJobWidget::searchFinished);
    connect(m_searchHandler, &SearchHandler::searchFailed, this, &SearchJobWidget::searchFailed);

    m_searchPattern = m_searchHandler->pattern();

    m_proxyModel->setNameFilter(m_searchPattern);
    updateFilter();

    setStatus(Status::Ongoing);
}

void SearchJobWidget::cancelSearch()
{
    if (!m_searchHandler)
        return;

    m_searchHandler->cancelSearch();
    setStatus(Status::Aborted);
}

void SearchJobWidget::downloadTorrents(const AddTorrentOption option)
{
    const QModelIndexList rows = m_ui->resultsBrowser->selectionModel()->selectedRows();
    for (const QModelIndex &rowIndex : rows)
        downloadTorrent(rowIndex, option);
}

void SearchJobWidget::openTorrentPages() const
{
    const QModelIndexList rows {m_ui->resultsBrowser->selectionModel()->selectedRows()};
    for (const QModelIndex &rowIndex : rows)
    {
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

    for (const QModelIndex &rowIndex : rows)
    {
        const QString field = m_proxyModel->data(
            m_proxyModel->index(rowIndex.row(), column)).toString();
        if (!field.isEmpty())
            list << field;
    }

    if (!list.empty())
        QApplication::clipboard()->setText(list.join(u'\n'));
}

void SearchJobWidget::setStatus(Status value)
{
    if (m_status == value)
        return;

    m_status = value;
    setStatusTip(statusText(value));
    emit statusChanged();
}

void SearchJobWidget::downloadTorrent(const QModelIndex &rowIndex, const AddTorrentOption option)
{
    const QString torrentUrl = m_proxyModel->data(
                m_proxyModel->index(rowIndex.row(), SearchSortModel::DL_LINK)).toString();
    const QString engineName = m_proxyModel->data(
                m_proxyModel->index(rowIndex.row(), SearchSortModel::ENGINE_NAME)).toString();

    if (torrentUrl.startsWith(u"magnet:", Qt::CaseInsensitive))
    {
        addTorrentToSession(torrentUrl, option);
    }
    else
    {
        SearchDownloadHandler *downloadHandler = SearchPluginManager::instance()->downloadTorrent(engineName, torrentUrl);
        connect(downloadHandler, &SearchDownloadHandler::downloadFinished
            , this, [this, option](const QString &source) { addTorrentToSession(source, option); });
        connect(downloadHandler, &SearchDownloadHandler::downloadFinished, downloadHandler, &SearchDownloadHandler::deleteLater);
    }

    setRowVisited(rowIndex.row());
}

void SearchJobWidget::addTorrentToSession(const QString &source, const AddTorrentOption option)
{
    app()->addTorrentManager()->addTorrent(source, {}, option);
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

    m_nameFilteringMode = filteringMode();

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

    const QVariant selectedMode = static_cast<int>(m_nameFilteringMode.get(NameFilteringMode::OnlyNames));
    const int index = m_ui->filterMode->findData(selectedMode);
    m_ui->filterMode->setCurrentIndex((index == -1) ? 0 : index);
}

void SearchJobWidget::filterSearchResults(const QString &name)
{
    const QString pattern = (Preferences::instance()->getRegexAsFilteringPatternForSearchJob()
                    ? name : Utils::String::wildcardToRegexPattern(name));
    m_proxyModel->setFilterRegularExpression(QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption));
    updateResultsCount();
}

void SearchJobWidget::showFilterContextMenu()
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

    menu->addAction(UIThemeManager::instance()->getIcon(u"download"_s)
        , tr("Open download window"), this, [this]() { downloadTorrents(AddTorrentOption::ShowDialog); });
    menu->addAction(UIThemeManager::instance()->getIcon(u"downloading"_s, u"download"_s)
        , tr("Download"), this, [this]() { downloadTorrents(AddTorrentOption::SkipDialog); });
    menu->addSeparator();
    menu->addAction(UIThemeManager::instance()->getIcon(u"application-url"_s), tr("Open description page")
        , this, &SearchJobWidget::openTorrentPages);

    QMenu *copySubMenu = menu->addMenu(
        UIThemeManager::instance()->getIcon(u"edit-copy"_s), tr("Copy"));

    copySubMenu->addAction(UIThemeManager::instance()->getIcon(u"name"_s, u"edit-copy"_s), tr("Name")
        , this, &SearchJobWidget::copyTorrentNames);
    copySubMenu->addAction(UIThemeManager::instance()->getIcon(u"insert-link"_s, u"edit-copy"_s), tr("Download link")
        , this, &SearchJobWidget::copyTorrentDownloadLinks);
    copySubMenu->addAction(UIThemeManager::instance()->getIcon(u"application-url"_s, u"edit-copy"_s), tr("Description page URL")
        , this, &SearchJobWidget::copyTorrentURLs);

    menu->popup(event->globalPos());
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

int SearchJobWidget::visibleColumnsCount() const
{
    int count = 0;
    for (int i = 0, iMax = m_ui->resultsBrowser->header()->count(); i < iMax; ++i)
    {
        if (!m_ui->resultsBrowser->isColumnHidden(i))
            ++count;
    }

    return count;
}

void SearchJobWidget::displayColumnHeaderMenu()
{
    auto *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setTitle(tr("Column visibility"));
    menu->setToolTipsVisible(true);

    for (int i = 0; i < SearchSortModel::DL_LINK; ++i)
    {
        const auto columnName = m_searchListModel->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString();
        QAction *action = menu->addAction(columnName, this, [this, i](const bool checked)
        {
            if (!checked && (visibleColumnsCount() <= 1))
                return;

            m_ui->resultsBrowser->setColumnHidden(i, !checked);

            if (checked && (m_ui->resultsBrowser->columnWidth(i) <= 5))
                m_ui->resultsBrowser->resizeColumnToContents(i);

            saveSettings();
        });
        action->setCheckable(true);
        action->setChecked(!m_ui->resultsBrowser->isColumnHidden(i));
    }

    menu->addSeparator();
    QAction *resizeAction = menu->addAction(tr("Resize columns"), this, [this]()
    {
        for (int i = 0, count = m_ui->resultsBrowser->header()->count(); i < count; ++i)
        {
            if (!m_ui->resultsBrowser->isColumnHidden(i))
                m_ui->resultsBrowser->resizeColumnToContents(i);
        }
        saveSettings();
    });
    resizeAction->setToolTip(tr("Resize all non-hidden columns to the size of their contents"));

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

void SearchJobWidget::appendSearchResults(const QList<SearchResult> &results)
{
    for (const SearchResult &result : results)
    {
        // Add item to search result list
        int row = m_searchListModel->rowCount();
        m_searchListModel->insertRow(row);

        const auto setModelData = [this, row] (const int column, const QString &displayData
                , const QVariant &underlyingData, const Qt::Alignment textAlignmentData = {})
        {
            const QMap<int, QVariant> data =
            {
                {Qt::DisplayRole, displayData},
                {SearchSortModel::UnderlyingDataRole, underlyingData},
                {Qt::TextAlignmentRole, QVariant {textAlignmentData}}
            };
            m_searchListModel->setItemData(m_searchListModel->index(row, column), data);
        };

        setModelData(SearchSortModel::NAME, result.fileName, result.fileName);
        setModelData(SearchSortModel::DL_LINK, result.fileUrl, result.fileUrl);
        setModelData(SearchSortModel::ENGINE_NAME, result.engineName, result.engineName);
        setModelData(SearchSortModel::ENGINE_URL, result.siteUrl, result.siteUrl);
        setModelData(SearchSortModel::DESC_LINK, result.descrLink, result.descrLink);
        setModelData(SearchSortModel::SIZE, Utils::Misc::friendlyUnit(result.fileSize), result.fileSize, (Qt::AlignRight | Qt::AlignVCenter));
        setModelData(SearchSortModel::SEEDS, QString::number(result.nbSeeders), result.nbSeeders, (Qt::AlignRight | Qt::AlignVCenter));
        setModelData(SearchSortModel::LEECHES, QString::number(result.nbLeechers), result.nbLeechers, (Qt::AlignRight | Qt::AlignVCenter));
        setModelData(SearchSortModel::PUB_DATE, QLocale().toString(result.pubDate.toLocalTime(), QLocale::ShortFormat), result.pubDate);
    }

    m_searchResults.append(results);
    updateResultsCount();
}

void SearchJobWidget::keyPressEvent(QKeyEvent *event)
{
    switch (event->key())
    {
    case Qt::Key_Enter:
    case Qt::Key_Return:
        downloadTorrents();
        break;
    default:
        QWidget::keyPressEvent(event);
    }
}
