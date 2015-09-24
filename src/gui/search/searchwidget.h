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

#ifndef SEARCHWIDGET_H
#define SEARCHWIDGET_H

#include <QList>
#include <QPointer>

#include "ui_searchwidget.h"

class MainWindow;
class LineEdit;
class SearchEngine;
struct SearchResult;
class SearchTab;

class SearchWidget: public QWidget, private Ui::SearchWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(SearchWidget)

public:
    explicit SearchWidget(MainWindow *mainWindow);
    ~SearchWidget();

    void downloadTorrent(QString url);
    void giveFocusToSearchInput();

private slots:
    // Search slots
    void tab_changed(int); //to prevent the use of the download button when the tab is empty
    void on_searchButton_clicked();
    void on_downloadButton_clicked();
    void on_goToDescBtn_clicked();
    void on_copyURLBtn_clicked();
    void on_pluginsButton_clicked();

    void closeTab(int index);
    void appendSearchResults(const QList<SearchResult> &results);
    void searchStarted();
    void searchFinished(bool cancelled);
    void searchFailed();
    void selectMultipleBox(const QString &text);

    void saveResultsColumnsWidth();
    void fillCatCombobox();
    void fillPluginComboBox();
    void searchTextEdited(QString);

private:
    QString selectedCategory() const;
    QString selectedPlugin() const;

    LineEdit *m_searchPattern;
    SearchEngine *m_searchEngine;
    QPointer<SearchTab> m_currentSearchTab; // Selected tab
    QPointer<SearchTab> m_activeSearchTab; // Tab with running search
    QList<QPointer<SearchTab> > m_allTabs; // To store all tabs
    MainWindow *m_mainWindow;
    bool m_isNewQueryString;
    bool m_noSearchResults;
    QByteArray m_searchResultLineTruncated;
    unsigned long m_nbSearchResults;
};

#endif // SEARCHWIDGET_H
