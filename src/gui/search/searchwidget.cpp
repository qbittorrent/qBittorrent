/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015-2024  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2020, Will Da Silva <will@willdasilva.xyz>
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

#include <QtSystemDetection>

#include <utility>

#ifdef Q_OS_WIN
#include <cstdlib>
#endif

#include <QDebug>
#include <QEvent>
#include <QList>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QObject>
#include <QRegularExpression>
#include <QShortcut>

#include "base/global.h"
#include "base/search/searchhandler.h"
#include "base/search/searchpluginmanager.h"
#include "base/utils/foreignapps.h"
#include "gui/desktopintegration.h"
#include "gui/interfaces/iguiapplication.h"
#include "gui/uithememanager.h"
#include "pluginselectdialog.h"
#include "searchjobwidget.h"
#include "ui_searchwidget.h"

#define SEARCHHISTORY_MAXSIZE 50
#define URL_COLUMN 5

namespace
{
    QString statusIconName(const SearchJobWidget::Status st)
    {
        switch (st)
        {
        case SearchJobWidget::Status::Ongoing:
            return u"queued"_s;
        case SearchJobWidget::Status::Finished:
            return u"task-complete"_s;
        case SearchJobWidget::Status::Aborted:
            return u"task-reject"_s;
        case SearchJobWidget::Status::Error:
        case SearchJobWidget::Status::NoResults:
            return u"dialog-warning"_s;
        default:
            return {};
        }
    }
}

SearchWidget::SearchWidget(IGUIApplication *app, QWidget *parent)
    : GUIApplicationComponent(app, parent)
    , m_ui {new Ui::SearchWidget()}
{
    m_ui->setupUi(this);
    m_ui->tabWidget->tabBar()->installEventFilter(this);

    const QString searchPatternHint = u"<html><head/><body><p>"
        + tr("A phrase to search for.") + u"<br>"
        + tr("Spaces in a search term may be protected by double quotes.")
        + u"</p><p>"
        + tr("Example:", "Search phrase example")
        + u"<br>"
        + tr("<b>foo bar</b>: search for <b>foo</b> and <b>bar</b>",
                 "Search phrase example, illustrates quotes usage, a pair of "
                 "space delimited words, individual words are highlighted")
        + u"<br>"
        + tr("<b>&quot;foo bar&quot;</b>: search for <b>foo bar</b>",
                 "Search phrase example, illustrates quotes usage, double quoted"
                 "pair of space delimited words, the whole pair is highlighted")
        + u"</p></body></html>";
    m_ui->lineEditSearchPattern->setToolTip(searchPatternHint);

#ifndef Q_OS_MACOS
    // Icons
    m_ui->searchButton->setIcon(UIThemeManager::instance()->getIcon(u"edit-find"_s));
    m_ui->pluginsButton->setIcon(UIThemeManager::instance()->getIcon(u"plugins"_s, u"preferences-system-network"_s));
#else
    // On macOS the icons overlap the text otherwise
    QSize iconSize = m_ui->tabWidget->iconSize();
    iconSize.setWidth(iconSize.width() + 16);
    m_ui->tabWidget->setIconSize(iconSize);
#endif
    connect(m_ui->tabWidget, &QTabWidget::tabCloseRequested, this, &SearchWidget::closeTab);
    connect(m_ui->tabWidget, &QTabWidget::currentChanged, this, &SearchWidget::tabChanged);

    const auto *searchManager = SearchPluginManager::instance();
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
    connect(m_ui->selectPlugin, qOverload<int>(&QComboBox::currentIndexChanged)
            , this, &SearchWidget::selectMultipleBox);
    connect(m_ui->selectPlugin, qOverload<int>(&QComboBox::currentIndexChanged)
            , this, &SearchWidget::fillCatCombobox);

    const auto *focusSearchHotkey = new QShortcut(QKeySequence::Find, this);
    connect(focusSearchHotkey, &QShortcut::activated, this, &SearchWidget::toggleFocusBetweenLineEdits);
    const auto *focusSearchHotkeyAlternative = new QShortcut((Qt::CTRL | Qt::Key_E), this);
    connect(focusSearchHotkeyAlternative, &QShortcut::activated, this, &SearchWidget::toggleFocusBetweenLineEdits);
}

bool SearchWidget::eventFilter(QObject *object, QEvent *event)
{
    if (object == m_ui->tabWidget->tabBar())
    {
        // Close tabs when middle-clicked
        if (event->type() != QEvent::MouseButtonRelease)
            return false;

        const auto *mouseEvent = static_cast<QMouseEvent *>(event);
        const int tabIndex = m_ui->tabWidget->tabBar()->tabAt(mouseEvent->pos());
        if ((mouseEvent->button() == Qt::MiddleButton) && (tabIndex >= 0))
        {
            closeTab(tabIndex);
            return true;
        }
        if (mouseEvent->button() == Qt::RightButton)
        {
            QMenu *menu = new QMenu(this);
            menu->setAttribute(Qt::WA_DeleteOnClose);
            menu->addAction(tr("Close tab"), this, [this, tabIndex]() { closeTab(tabIndex); });
            menu->addAction(tr("Close all tabs"), this, &SearchWidget::closeAllTabs);
            menu->popup(QCursor::pos());
            return true;
        }
        return false;
    }

    return QWidget::eventFilter(object, event);
}

void SearchWidget::fillCatCombobox()
{
    m_ui->comboCategory->clear();
    m_ui->comboCategory->addItem(SearchPluginManager::categoryFullName(u"all"_s), u"all"_s);

    using QStrPair = std::pair<QString, QString>;
    QList<QStrPair> tmpList;
    for (const QString &cat : asConst(SearchPluginManager::instance()->getPluginCategories(selectedPlugin())))
        tmpList << std::make_pair(SearchPluginManager::categoryFullName(cat), cat);
    std::sort(tmpList.begin(), tmpList.end(), [](const QStrPair &l, const QStrPair &r) { return (QString::localeAwareCompare(l.first, r.first) < 0); });

    for (const QStrPair &p : asConst(tmpList))
    {
        qDebug("Supported category: %s", qUtf8Printable(p.second));
        m_ui->comboCategory->addItem(p.first, p.second);
    }

    if (m_ui->comboCategory->count() > 1)
        m_ui->comboCategory->insertSeparator(1);
}

void SearchWidget::fillPluginComboBox()
{
    m_ui->selectPlugin->clear();
    m_ui->selectPlugin->addItem(tr("Only enabled"), u"enabled"_s);
    m_ui->selectPlugin->addItem(tr("All plugins"), u"all"_s);
    m_ui->selectPlugin->addItem(tr("Select..."), u"multi"_s);

    using QStrPair = std::pair<QString, QString>;
    QList<QStrPair> tmpList;
    for (const QString &name : asConst(SearchPluginManager::instance()->enabledPlugins()))
        tmpList << std::make_pair(SearchPluginManager::instance()->pluginFullName(name), name);
    std::sort(tmpList.begin(), tmpList.end(), [](const QStrPair &l, const QStrPair &r) { return (l.first < r.first); } );

    for (const QStrPair &p : asConst(tmpList))
        m_ui->selectPlugin->addItem(p.first, p.second);

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
    if (SearchPluginManager::instance()->allPlugins().isEmpty())
    {
        m_ui->stackedPages->setCurrentWidget(m_ui->emptyPage);
        m_ui->lineEditSearchPattern->setEnabled(false);
        m_ui->comboCategory->setEnabled(false);
        m_ui->selectPlugin->setEnabled(false);
        m_ui->searchButton->setEnabled(false);
    }
    else
    {
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

void SearchWidget::tabChanged(const int index)
{
    // when we switch from a tab that is not empty to another that is empty
    // the download button doesn't have to be available
    m_currentSearchTab = (index >= 0)
        ? static_cast<SearchJobWidget *>(m_ui->tabWidget->widget(index))
        : nullptr;
}

void SearchWidget::selectMultipleBox([[maybe_unused]] const int index)
{
    if (selectedPlugin() == u"multi")
        on_pluginsButton_clicked();
}

void SearchWidget::toggleFocusBetweenLineEdits()
{
    if (m_ui->lineEditSearchPattern->hasFocus() && m_currentSearchTab)
    {
        m_currentSearchTab->lineEditSearchResultsFilter()->setFocus();
        m_currentSearchTab->lineEditSearchResultsFilter()->selectAll();
    }
    else
    {
        m_ui->lineEditSearchPattern->setFocus();
        m_ui->lineEditSearchPattern->selectAll();
    }
}

void SearchWidget::on_pluginsButton_clicked()
{
    auto *dlg = new PluginSelectDialog(SearchPluginManager::instance(), this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

void SearchWidget::searchTextEdited(const QString &)
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
    if (!Utils::ForeignApps::pythonInfo().isValid())
    {
        app()->desktopIntegration()->showNotification(tr("Search Engine"), tr("Please install Python to use the Search Engine."));
        return;
    }

    if (m_activeSearchTab)
    {
        m_activeSearchTab->cancelSearch();
        if (!m_isNewQueryString)
        {
            m_ui->searchButton->setText(tr("Search"));
            return;
        }
    }

    m_isNewQueryString = false;

    const QString pattern = m_ui->lineEditSearchPattern->text().trimmed();
    // No search pattern entered
    if (pattern.isEmpty())
    {
        QMessageBox::critical(this, tr("Empty search pattern"), tr("Please type a search pattern first"));
        return;
    }

    const QString plugin = selectedPlugin();

    QStringList plugins;
    if (plugin == u"all")
        plugins = SearchPluginManager::instance()->allPlugins();
    else if ((plugin == u"enabled") || (plugin == u"multi"))
        plugins = SearchPluginManager::instance()->enabledPlugins();
    else
        plugins << plugin;

    qDebug("Search with category: %s", qUtf8Printable(selectedCategory()));

    // Launch search
    auto *searchHandler = SearchPluginManager::instance()->startSearch(pattern, selectedCategory(), plugins);

    // Tab Addition
    auto *newTab = new SearchJobWidget(searchHandler, app(), this);

    QString tabName = pattern;
    tabName.replace(QRegularExpression(u"&{1}"_s), u"&&"_s);
    m_ui->tabWidget->addTab(newTab, tabName);
    m_ui->tabWidget->setCurrentWidget(newTab);

    connect(newTab, &SearchJobWidget::statusChanged, this, [this, newTab]() { tabStatusChanged(newTab); });

    m_ui->searchButton->setText(tr("Stop"));
    m_activeSearchTab = newTab;
    tabStatusChanged(newTab);
}

void SearchWidget::tabStatusChanged(QWidget *tab)
{
    const int tabIndex = m_ui->tabWidget->indexOf(tab);
    m_ui->tabWidget->setTabToolTip(tabIndex, tab->statusTip());
    m_ui->tabWidget->setTabIcon(tabIndex, UIThemeManager::instance()->getIcon(
                                 statusIconName(static_cast<SearchJobWidget *>(tab)->status())));

    if ((tab == m_activeSearchTab) && (m_activeSearchTab->status() != SearchJobWidget::Status::Ongoing))
    {
        Q_ASSERT(m_activeSearchTab->status() != SearchJobWidget::Status::Ongoing);

        emit activeSearchFinished(m_activeSearchTab->status() == SearchJobWidget::Status::Error);

        m_activeSearchTab = nullptr;
        m_ui->searchButton->setText(tr("Search"));
    }
}

void SearchWidget::closeTab(const int index)
{
    const QWidget *tab = m_ui->tabWidget->widget(index);
    if (tab == m_activeSearchTab)
        m_ui->searchButton->setText(tr("Search"));

    delete tab;
}

void SearchWidget::closeAllTabs()
{
    for (int i = (m_ui->tabWidget->count() - 1); i >= 0; --i)
        closeTab(i);
}
