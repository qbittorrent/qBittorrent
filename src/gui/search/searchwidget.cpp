/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015, 2018  Vladimir Golovnev <glassez@yandex.ru>
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

#include "searchwidget.h"

#include <QtGlobal>

#ifdef Q_OS_WIN
#include <cstdlib>
#endif

#include <QDebug>
#include <QHeaderView>
#include <QMessageBox>
#include <QMimeData>
#include <QProcess>
#include <QRegularExpression>
#include <QShortcut>
#include <QSignalMapper>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTextStream>
#include <QTreeView>

#include "base/bittorrent/session.h"
#include "base/global.h"
#include "base/preferences.h"
#include "base/search/searchpluginmanager.h"
#include "base/search/searchhandler.h"
#include "base/utils/foreignapps.h"
#include "base/utils/fs.h"
#include "addnewtorrentdialog.h"
#include "guiiconprovider.h"
#include "mainwindow.h"
#include "pluginselectdialog.h"
#include "searchlistdelegate.h"
#include "searchsortmodel.h"
#include "searchjobwidget.h"
#include "ui_searchwidget.h"

#define SEARCHHISTORY_MAXSIZE 50
#define URL_COLUMN 5

namespace
{
    QString statusIconName(SearchJobWidget::Status st)
    {
        switch (st) {
        case SearchJobWidget::Status::Ongoing:
            return QLatin1String("task-ongoing");
        case SearchJobWidget::Status::Finished:
            return QLatin1String("task-complete");
        case SearchJobWidget::Status::Aborted:
            return QLatin1String("task-reject");
        case SearchJobWidget::Status::Error:
            return QLatin1String("task-attention");
        case SearchJobWidget::Status::NoResults:
            return QLatin1String("task-attention");
        default:
            return QString();
        }
    }
}

SearchWidget::SearchWidget(MainWindow *mainWindow)
    : QWidget(mainWindow)
    , m_ui(new Ui::SearchWidget())
    , m_tabStatusChangedMapper(new QSignalMapper(this))
    , m_mainWindow(mainWindow)
    , m_isNewQueryString(false)
{
    m_ui->setupUi(this);

    QString searchPatternHint;
    QTextStream stream(&searchPatternHint, QIODevice::WriteOnly);
    stream << "<html><head/><body><p>"
           << tr("A phrase to search for.") << "<br>"
           << tr("Spaces in a search term may be protected by double quotes.")
           << "</p><p>"
           << tr("Example:", "Search phrase example")
           << "<br>"
           << tr("<b>foo bar</b>: search for <b>foo</b> and <b>bar</b>",
                 "Search phrase example, illustrates quotes usage, a pair of "
                 "space delimited words, individal words are highlighted")
           << "<br>"
           << tr("<b>&quot;foo bar&quot;</b>: search for <b>foo bar</b>",
                 "Search phrase example, illustrates quotes usage, double quoted"
                 "pair of space delimited words, the whole pair is highlighted")
           << "</p></body></html>" << flush;
    m_ui->lineEditSearchPattern->setToolTip(searchPatternHint);

#ifndef Q_OS_MAC
    // Icons
    m_ui->searchButton->setIcon(GuiIconProvider::instance()->getIcon("edit-find"));
    m_ui->downloadButton->setIcon(GuiIconProvider::instance()->getIcon("download"));
    m_ui->goToDescBtn->setIcon(GuiIconProvider::instance()->getIcon("application-x-mswinurl"));
    m_ui->pluginsButton->setIcon(GuiIconProvider::instance()->getIcon("preferences-system-network"));
    m_ui->copyURLBtn->setIcon(GuiIconProvider::instance()->getIcon("edit-copy"));
#else
    // On macOS the icons overlap the text otherwise
    QSize iconSize = m_ui->tabWidget->iconSize();
    iconSize.setWidth(iconSize.width() + 16);
    m_ui->tabWidget->setIconSize(iconSize);
#endif
    connect(m_ui->tabWidget, &QTabWidget::tabCloseRequested, this, &SearchWidget::closeTab);
    connect(m_ui->tabWidget, &QTabWidget::currentChanged, this, &SearchWidget::tabChanged);

    connect(m_tabStatusChangedMapper, static_cast<void (QSignalMapper::*)(QWidget *)>(&QSignalMapper::mapped)
            , this, &SearchWidget::tabStatusChanged);

    auto *searchManager = SearchPluginManager::instance();
    const auto onPluginChanged = [this]()
    {
        fillPluginComboBox();
        fillCatCombobox();
        selectActivePage();
    };
    connect(searchManager, &SearchPluginManager::pluginInstalled, this, onPluginChanged);
    connect(searchManager, &SearchPluginManager::pluginUninstalled, this, onPluginChanged);
    connect(searchManager, &SearchPluginManager::pluginUpdated, this, onPluginChanged);
    connect(searchManager, &SearchPluginManager::pluginEnabled, this, onPluginChanged);

    // Fill in category combobox
    onPluginChanged();

    connect(m_ui->lineEditSearchPattern, &LineEdit::returnPressed, m_ui->searchButton, &QPushButton::click);
    connect(m_ui->lineEditSearchPattern, &LineEdit::textEdited, this, &SearchWidget::searchTextEdited);
    connect(m_ui->selectPlugin, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged)
            , this, &SearchWidget::selectMultipleBox);
    connect(m_ui->selectPlugin, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged)
            , this, &SearchWidget::fillCatCombobox);

    m_focusSearchHotkey = new QShortcut(QKeySequence::Find, this);
    connect(m_focusSearchHotkey, &QShortcut::activated, this, &SearchWidget::toggleFocusBetweenLineEdits);
}

void SearchWidget::fillCatCombobox()
{
    m_ui->comboCategory->clear();
    m_ui->comboCategory->addItem(SearchPluginManager::categoryFullName("all"), QVariant("all"));

    using QStrPair = QPair<QString, QString>;
    QList<QStrPair> tmpList;
    for (const QString &cat : asConst(SearchPluginManager::instance()->getPluginCategories(selectedPlugin())))
        tmpList << qMakePair(SearchPluginManager::categoryFullName(cat), cat);
    std::sort(tmpList.begin(), tmpList.end(), [](const QStrPair &l, const QStrPair &r) { return (QString::localeAwareCompare(l.first, r.first) < 0); });

    for (const QStrPair &p : asConst(tmpList)) {
        qDebug("Supported category: %s", qUtf8Printable(p.second));
        m_ui->comboCategory->addItem(p.first, QVariant(p.second));
    }

    if (m_ui->comboCategory->count() > 1)
        m_ui->comboCategory->insertSeparator(1);
}

void SearchWidget::fillPluginComboBox()
{
    m_ui->selectPlugin->clear();
    m_ui->selectPlugin->addItem(tr("Only enabled"), QVariant("enabled"));
    m_ui->selectPlugin->addItem(tr("All plugins"), QVariant("all"));
    m_ui->selectPlugin->addItem(tr("Select..."), QVariant("multi"));

    using QStrPair = QPair<QString, QString>;
    QList<QStrPair> tmpList;
    for (const QString &name : asConst(SearchPluginManager::instance()->enabledPlugins()))
        tmpList << qMakePair(SearchPluginManager::instance()->pluginFullName(name), name);
    std::sort(tmpList.begin(), tmpList.end(), [](const QStrPair &l, const QStrPair &r) { return (l.first < r.first); } );

    for (const QStrPair &p : asConst(tmpList))
        m_ui->selectPlugin->addItem(p.first, QVariant(p.second));

    if (m_ui->selectPlugin->count() > 3)
        m_ui->selectPlugin->insertSeparator(3);
}

QString SearchWidget::selectedCategory() const
{
    return m_ui->comboCategory->itemData(m_ui->comboCategory->currentIndex()).toString();
}

QString SearchWidget::selectedPlugin() const
{
    return m_ui->selectPlugin->itemData(m_ui->selectPlugin->currentIndex()).toString();
}

void SearchWidget::selectActivePage()
{
    if (SearchPluginManager::instance()->allPlugins().isEmpty()) {
        m_ui->stackedPages->setCurrentWidget(m_ui->emptyPage);
        m_ui->lineEditSearchPattern->setEnabled(false);
        m_ui->comboCategory->setEnabled(false);
        m_ui->selectPlugin->setEnabled(false);
        m_ui->searchButton->setEnabled(false);
    }
    else {
        m_ui->stackedPages->setCurrentWidget(m_ui->searchPage);
        m_ui->lineEditSearchPattern->setEnabled(true);
        m_ui->comboCategory->setEnabled(true);
        m_ui->selectPlugin->setEnabled(true);
        m_ui->searchButton->setEnabled(true);
    }
}

SearchWidget::~SearchWidget()
{
    qDebug("Search destruction");
    delete m_ui;
}

void SearchWidget::updateButtons()
{
    if (m_currentSearchTab && (m_currentSearchTab->visibleResultsCount() > 0)) {
        m_ui->downloadButton->setEnabled(true);
        m_ui->goToDescBtn->setEnabled(true);
        m_ui->copyURLBtn->setEnabled(true);
    }
    else {
        m_ui->downloadButton->setEnabled(false);
        m_ui->goToDescBtn->setEnabled(false);
        m_ui->copyURLBtn->setEnabled(false);
    }
}

void SearchWidget::tabChanged(int index)
{
    // when we switch from a tab that is not empty to another that is empty
    // the download button doesn't have to be available
    m_currentSearchTab = ((index < 0) ? nullptr : m_allTabs.at(m_ui->tabWidget->currentIndex()));
    updateButtons();
}

void SearchWidget::selectMultipleBox(int index)
{
    Q_UNUSED(index);
    if (selectedPlugin() == "multi")
        on_pluginsButton_clicked();
}

void SearchWidget::toggleFocusBetweenLineEdits()
{
    if (m_ui->lineEditSearchPattern->hasFocus() && m_currentSearchTab) {
        m_currentSearchTab->lineEditSearchResultsFilter()->setFocus();
        m_currentSearchTab->lineEditSearchResultsFilter()->selectAll();
    }
    else {
        m_ui->lineEditSearchPattern->setFocus();
        m_ui->lineEditSearchPattern->selectAll();
    }
}

void SearchWidget::on_pluginsButton_clicked()
{
    new PluginSelectDialog(SearchPluginManager::instance(), this);
}

void SearchWidget::searchTextEdited(QString)
{
    // Enable search button
    m_ui->searchButton->setText(tr("Search"));
    m_isNewQueryString = true;
}

void SearchWidget::giveFocusToSearchInput()
{
    m_ui->lineEditSearchPattern->setFocus();
}

// Function called when we click on search button
void SearchWidget::on_searchButton_clicked()
{
    if (!Utils::ForeignApps::pythonInfo().isValid()) {
        m_mainWindow->showNotificationBaloon(tr("Search Engine"), tr("Please install Python to use the Search Engine."));
        return;
    }

    if (m_activeSearchTab) {
        m_activeSearchTab->cancelSearch();
        if (!m_isNewQueryString) {
            m_ui->searchButton->setText(tr("Search"));
            return;
        }
    }

    m_isNewQueryString = false;

    const QString pattern = m_ui->lineEditSearchPattern->text().trimmed();
    // No search pattern entered
    if (pattern.isEmpty()) {
        QMessageBox::critical(this, tr("Empty search pattern"), tr("Please type a search pattern first"));
        return;
    }

    QStringList plugins;
    if (selectedPlugin() == "all")
        plugins = SearchPluginManager::instance()->allPlugins();
    else if (selectedPlugin() == "enabled")
        plugins = SearchPluginManager::instance()->enabledPlugins();
    else if (selectedPlugin() == "multi")
        plugins = SearchPluginManager::instance()->enabledPlugins();
    else
        plugins << selectedPlugin();

    qDebug("Search with category: %s", qUtf8Printable(selectedCategory()));

    // Launch search
    auto *searchHandler = SearchPluginManager::instance()->startSearch(pattern, selectedCategory(), plugins);

    // Tab Addition
    auto *newTab = new SearchJobWidget(searchHandler, this);
    m_allTabs.append(newTab);

    QString tabName = pattern;
    tabName.replace(QRegularExpression("&{1}"), "&&");
    m_ui->tabWidget->addTab(newTab, tabName);
    m_ui->tabWidget->setCurrentWidget(newTab);

    connect(newTab, &SearchJobWidget::resultsCountUpdated, this, &SearchWidget::resultsCountUpdated);
    connect(newTab, &SearchJobWidget::statusChanged
            , m_tabStatusChangedMapper, static_cast<void (QSignalMapper::*)()>(&QSignalMapper::map));
    m_tabStatusChangedMapper->setMapping(newTab, newTab);

    m_ui->searchButton->setText(tr("Stop"));
    m_activeSearchTab = newTab;
    tabStatusChanged(newTab);
}

void SearchWidget::resultsCountUpdated()
{
    updateButtons();
}

void SearchWidget::tabStatusChanged(QWidget *tab)
{
    const int tabIndex = m_ui->tabWidget->indexOf(tab);
    m_ui->tabWidget->setTabToolTip(tabIndex, tab->statusTip());
    m_ui->tabWidget->setTabIcon(tabIndex, GuiIconProvider::instance()->getIcon(
                                 statusIconName(static_cast<SearchJobWidget *>(tab)->status())));

    if ((tab == m_activeSearchTab) && (m_activeSearchTab->status() != SearchJobWidget::Status::Ongoing)) {
        Q_ASSERT(m_activeSearchTab->status() != SearchJobWidget::Status::Ongoing);

        if (m_mainWindow->isNotificationsEnabled() && (m_mainWindow->currentTabWidget() != this)) {
            if (m_activeSearchTab->status() == SearchJobWidget::Status::Error)
                m_mainWindow->showNotificationBaloon(tr("Search Engine"), tr("Search has failed"));
            else
                m_mainWindow->showNotificationBaloon(tr("Search Engine"), tr("Search has finished"));
        }

        m_activeSearchTab = nullptr;
        m_ui->searchButton->setText(tr("Search"));
    }
}

void SearchWidget::closeTab(int index)
{
    SearchJobWidget *tab = m_allTabs.takeAt(index);
    if (tab == m_activeSearchTab)
        m_ui->searchButton->setText(tr("Search"));

    delete tab;
}

// Download selected items in search results list
void SearchWidget::on_downloadButton_clicked()
{
    m_allTabs.at(m_ui->tabWidget->currentIndex())->downloadTorrents();
}

void SearchWidget::on_goToDescBtn_clicked()
{
    m_allTabs.at(m_ui->tabWidget->currentIndex())->openTorrentPages();
}

void SearchWidget::on_copyURLBtn_clicked()
{
    m_allTabs.at(m_ui->tabWidget->currentIndex())->copyTorrentURLs();
}
