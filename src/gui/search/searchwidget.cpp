/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
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
 *
 * Contact : chris@qbittorrent.org
 */

#include <QHeaderView>
#include <QMessageBox>
#include <QTemporaryFile>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QDir>
#include <QMenu>
#include <QMimeData>
#include <QFileDialog>
#include <QDebug>

#include <iostream>
#ifdef Q_OS_WIN
#include <stdlib.h>
#endif

#include "base/utils/misc.h"
#include "base/preferences.h"
#include "base/searchengine.h"
#include "searchlistdelegate.h"
#include "mainwindow.h"
#include "guiiconprovider.h"
#include "lineedit.h"
#include "pluginselectdlg.h"
#include "searchsortmodel.h"
#include "searchtab.h"
#include "searchwidget.h"

#define SEARCHHISTORY_MAXSIZE 50

SearchWidget::SearchWidget(MainWindow *mainWindow)
    : QWidget(mainWindow)
    , m_mainWindow(mainWindow)
    , m_isNewQueryString(false)
{
    setupUi(this);

    m_searchPattern = new LineEdit(this);
    searchBarLayout->insertWidget(0, m_searchPattern);
    connect(m_searchPattern, SIGNAL(returnPressed()), searchButton, SLOT(click()));

    // Icons
    searchButton->setIcon(GuiIconProvider::instance()->getIcon("edit-find"));
    downloadButton->setIcon(GuiIconProvider::instance()->getIcon("download"));
    goToDescBtn->setIcon(GuiIconProvider::instance()->getIcon("application-x-mswinurl"));
    pluginsButton->setIcon(GuiIconProvider::instance()->getIcon("preferences-system-network"));
    copyURLBtn->setIcon(GuiIconProvider::instance()->getIcon("edit-copy"));
    tabWidget->setTabsClosable(true);
    connect(tabWidget, SIGNAL(tabCloseRequested(int)), SLOT(closeTab(int)));
    connect(tabWidget, SIGNAL(currentChanged(int)), SLOT(currentTabChanged(int)));

    m_searchEngine = new SearchEngine;
    connect(m_searchEngine, SIGNAL(searchStarted()), SLOT(searchStarted()));
    connect(m_searchEngine, SIGNAL(searchFinished(bool)), SLOT(searchFinished(bool)));
    connect(m_searchEngine, SIGNAL(searchFailed()), SLOT(searchFailed()));

    // Fill in category combobox
    fillCatCombobox();
    fillPluginComboBox();

    connect(m_searchPattern, SIGNAL(textEdited(QString)), this, SLOT(searchTextEdited(QString)));
    connect(selectPlugin, SIGNAL(currentIndexChanged(const QString &)), this, SLOT(selectMultipleBox(const QString &)));
}

SearchWidget::~SearchWidget()
{
    qDebug("Search destruction");
    delete m_searchEngine;
}

void SearchWidget::fillCatCombobox()
{
    comboCategory->clear();
    comboCategory->addItem(SearchEngine::categoryFullName("all"), QVariant("all"));
    foreach (QString cat, m_searchEngine->supportedCategories()) {
        qDebug("Supported category: %s", qPrintable(cat));
        comboCategory->addItem(SearchEngine::categoryFullName(cat), QVariant(cat));
    }
}

void SearchWidget::fillPluginComboBox()
{
    selectPlugin->clear();
    selectPlugin->addItem(tr("All enabled"), QVariant("enabled"));
    selectPlugin->addItem(tr("All plugins"), QVariant("all"));
    foreach (QString name, m_searchEngine->enabledPlugins())
        selectPlugin->addItem(name, QVariant(name));
    selectPlugin->addItem(tr("Multiple..."), QVariant("multi"));
}

QString SearchWidget::selectedCategory() const
{
    return comboCategory->itemData(comboCategory->currentIndex()).toString();
}

QString SearchWidget::selectedPlugin() const
{
    return selectPlugin->itemData(selectPlugin->currentIndex()).toString();
}

void SearchWidget::currentTabChanged(int t)
{
    downloadButton->disconnect(SIGNAL(clicked()));
    goToDescBtn->disconnect(SIGNAL(clicked()));
    copyURLBtn->disconnect(SIGNAL(clicked()));

    //when we switch from a tab that is not empty to another that is empty the download button
    //doesn't have to be available
    if (t > -1) {
        //-1 = no more tab
        m_currentSearchTab = m_allTabs.at(tabWidget->currentIndex());

        connect(downloadButton, SIGNAL(clicked()), m_currentSearchTab.data(), SLOT(download()));
        connect(goToDescBtn, SIGNAL(clicked()), m_currentSearchTab.data(), SLOT(goToDescriptionPage()));
        connect(copyURLBtn, SIGNAL(clicked()), m_currentSearchTab.data(), SLOT(copyURLs()));

        if (m_currentSearchTab->searchResultsCount() > 0) {
            downloadButton->setEnabled(true);
            goToDescBtn->setEnabled(true);
            copyURLBtn->setEnabled(true);
        }
        else {
            downloadButton->setEnabled(false);
            goToDescBtn->setEnabled(false);
            copyURLBtn->setEnabled(false);
        }

        searchStatus->setText(m_currentSearchTab->status());
    }
}

void SearchWidget::selectMultipleBox(const QString &text)
{
    if (text == tr("Multiple..."))
        on_pluginsButton_clicked();
}

void SearchWidget::tabStatusUpdated()
{
    if (sender() == m_currentSearchTab) {
        searchStatus->setText(m_currentSearchTab->status());
        searchStatus->repaint();

        if (m_currentSearchTab->searchResultsCount() > 0) {
            downloadButton->setEnabled(true);
            goToDescBtn->setEnabled(true);
            copyURLBtn->setEnabled(true);
        }
    }
}

void SearchWidget::on_pluginsButton_clicked()
{
    PluginSelectDlg *dlg = new PluginSelectDlg(m_searchEngine, this);
    connect(dlg, SIGNAL(pluginsChanged()), this, SLOT(fillCatCombobox()));
    connect(dlg, SIGNAL(pluginsChanged()), this, SLOT(fillPluginComboBox()));
}

void SearchWidget::searchTextEdited(QString)
{
    // Enable search button
    searchButton->setText(tr("Search"));
    m_isNewQueryString = true;
}

void SearchWidget::giveFocusToSearchInput()
{
    m_searchPattern->setFocus();
}

// Function called when we click on search button
void SearchWidget::on_searchButton_clicked()
{
    if (Utils::Misc::pythonVersion() < 0) {
        m_mainWindow->showNotificationBaloon(tr("Search Engine"), tr("Please install Python to use the Search Engine."));
        return;
    }

    if (m_searchEngine->isActive()) {
        m_searchEngine->cancelSearch();

        if (!m_isNewQueryString) return;
    }

    m_isNewQueryString = false;

    const QString pattern = m_searchPattern->text().trimmed();
    // No search pattern entered
    if (pattern.isEmpty()) {
        QMessageBox::critical(0, tr("Empty search pattern"), tr("Please type a search pattern first"));
        return;
    }

    // Tab Addition
    m_currentSearchTab = new SearchTab(m_searchEngine, this);
    connect(m_currentSearchTab.data(), SIGNAL(statusUpdated()), SLOT(tabStatusUpdated()));
    connect(downloadButton, SIGNAL(clicked()), m_currentSearchTab.data(), SLOT(download()));
    connect(goToDescBtn, SIGNAL(clicked()), m_currentSearchTab.data(), SLOT(goToDescriptionPage()));
    connect(copyURLBtn, SIGNAL(clicked()), m_currentSearchTab.data(), SLOT(copyURLs()));

    m_allTabs.append(m_currentSearchTab);
    QString tabName = pattern;
    tabName.replace(QRegExp("&{1}"), "&&");
    tabWidget->addTab(m_currentSearchTab, tabName);
    tabWidget->setCurrentWidget(m_currentSearchTab);

    QStringList plugins;
    if (selectedPlugin() == "all") plugins = m_searchEngine->allPlugins();
    else if (selectedPlugin() == "enabled") plugins = m_searchEngine->enabledPlugins();
    else if (selectedPlugin() == "multi") plugins = m_searchEngine->enabledPlugins();
    else plugins << selectedPlugin();

    qDebug("Search with category: %s", qPrintable(selectedCategory()));
    // Launch search
    m_searchEngine->startSearch(pattern, selectedCategory(), plugins);
}

void SearchWidget::searchStarted()
{
    searchButton->setText(tr("Stop"));
}

void SearchWidget::searchFinished(bool)
{
    if (Preferences::instance()->useProgramNotification() && (m_mainWindow->getCurrentTabWidget() != this))
        m_mainWindow->showNotificationBaloon(tr("Search Engine"), tr("Search has finished"));

    searchButton->setText(tr("Search"));
}

void SearchWidget::searchFailed()
{
    if (Preferences::instance()->useProgramNotification() && (m_mainWindow->getCurrentTabWidget() != this))
        m_mainWindow->showNotificationBaloon(tr("Search Engine"), tr("Search has failed"));

    searchButton->setText(tr("Search"));
}

void SearchWidget::closeTab(int index)
{
    auto closedTab = m_allTabs.takeAt(index);
    if (closedTab->isActive()) {
        // Search is run for active tab so if user
        // decided to close it, then stop search
        m_searchEngine->cancelSearch();
    }

    delete closedTab;

    if (m_allTabs.size() == 0) {
        downloadButton->setEnabled(false);
        goToDescBtn->setEnabled(false);
        searchStatus->setText(tr("Stopped"));
        copyURLBtn->setEnabled(false);
    }
}
