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

#pragma once

#include <QList>
#include <QPointer>
#include <QWidget>

#include "gui/guiapplicationcomponent.h"

class QEvent;
class QObject;
class QTabWidget;

class MainWindow;
class SearchJobWidget;

namespace Ui
{
    class SearchWidget;
}

class SearchWidget : public GUIApplicationComponent<QWidget>
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SearchWidget)

public:
    explicit SearchWidget(IGUIApplication *app, MainWindow *mainWindow);
    ~SearchWidget() override;

    void giveFocusToSearchInput();

private slots:
    void on_searchButton_clicked();
    void on_pluginsButton_clicked();

private:
    bool eventFilter(QObject *object, QEvent *event) override;
    void tabChanged(int index);
    void tabMoved(int from, int to);
    void closeTab(int index);
    void closeAllTabs();
    void tabStatusChanged(QWidget *tab);
    void selectMultipleBox(int index);
    void toggleFocusBetweenLineEdits();

    void fillCatCombobox();
    void fillPluginComboBox();
    void selectActivePage();
    void searchTextEdited(const QString &);

    QString selectedCategory() const;
    QString selectedPlugin() const;

    Ui::SearchWidget *m_ui = nullptr;
    QPointer<SearchJobWidget> m_currentSearchTab; // Selected tab
    QPointer<SearchJobWidget> m_activeSearchTab; // Tab with running search
    QList<SearchJobWidget *> m_allTabs; // To store all tabs
    MainWindow *m_mainWindow = nullptr;
    bool m_isNewQueryString = false;
};
