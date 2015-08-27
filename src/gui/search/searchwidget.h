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
#include <QPair>
#include <QPointer>
#include <QStringListModel>
#include "ui_searchwidget.h"
#include "pluginselectdlg.h"
#include "searchtab.h"

class SearchWidget;
class MainWindow;
class LineEdit;
class SearchEngine;
struct SearchResult;

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

class SearchWidget : public QWidget, private Ui::SearchWidget{
    Q_OBJECT
    Q_DISABLE_COPY(SearchWidget)

public:
    SearchWidget(MainWindow *mp_mainWindow);
    ~SearchWidget();
    QString selectedCategory() const;
    QString selectedEngine() const;

public slots:
    void downloadTorrent(QString url);
    void giveFocusToSearchInput();

private slots:
    // Search slots
    void tab_changed(int);//to prevent the use of the download button when the tab is empty
    void on_search_button_clicked();
    void on_download_button_clicked();
    void closeTab(int index);
    void appendSearchResults(const QList<SearchResult> &results);
    void searchStarted();
    void searchFinished(bool cancelled);
    void searchFailed();
    void selectMultipleBox(const QString &text);
    void on_pluginsButton_clicked();
    void saveResultsColumnsWidth();
    void fillCatCombobox();
    void fillPluginComboBox();
    void searchTextEdited(QString);
    void on_goToDescBtn_clicked();
    void on_copyURLBtn_clicked();

private:
    // Search related
    LineEdit* search_pattern;

    bool no_search_results;
    QByteArray search_result_line_truncated;
    unsigned long nb_search_results;
    SearchEngine *m_searchEngine;
    QPointer<SearchTab> currentSearchTab; // Selected tab
    QPointer<SearchTab> activeSearchTab; // Tab with running search
    QList<QPointer<SearchTab> > all_tab; // To store all tabs
    MainWindow *mp_mainWindow;
    bool newQueryString;
};

#endif // SEARCHWIDGET_H
