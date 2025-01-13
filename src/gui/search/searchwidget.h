/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015-2025  Vladimir Golovnev <glassez@yandex.ru>
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

#include <QPointer>
#include <QWidget>

#include "gui/guiapplicationcomponent.h"

class QEvent;
class QObject;

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
    explicit SearchWidget(IGUIApplication *app, QWidget *parent);
    ~SearchWidget() override;

    void giveFocusToSearchInput();

signals:
    void searchFinished(bool failed);

private:
    bool eventFilter(QObject *object, QEvent *event) override;

    void pluginsButtonClicked();
    void searchButtonClicked();
    void stopButtonClicked();
    void searchTextEdited(const QString &text);
    void currentTabChanged(int index);

    void tabStatusChanged(SearchJobWidget *tab);

    void closeTab(int index);
    void closeAllTabs();
    void refreshTab(SearchJobWidget *searchJobWidget);
    void showTabMenu(int index);

    void selectMultipleBox(int index);
    void toggleFocusBetweenLineEdits();
    void adjustSearchButton();

    void fillCatCombobox();
    void fillPluginComboBox();
    void selectActivePage();

    QString selectedCategory() const;
    QStringList selectedPlugins() const;

    Ui::SearchWidget *m_ui = nullptr;
    QPointer<SearchJobWidget> m_currentSearchTab; // Selected tab
    bool m_isNewQueryString = false;
};
