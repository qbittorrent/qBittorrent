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

#pragma once

#include <QWidget>

#include "base/settingvalue.h"
#include "gui/guiaddtorrentmanager.h"
#include "gui/guiapplicationcomponent.h"

#define ENGINE_URL_COLUMN 4
#define URL_COLUMN 5

class QHeaderView;
class QModelIndex;
class QStandardItemModel;

class LineEdit;
class SearchHandler;
class SearchSortModel;
struct SearchResult;

template <typename T> class SettingValue;

namespace Ui
{
    class SearchJobWidget;
}

class SearchJobWidget final : public GUIApplicationComponent<QWidget>
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SearchJobWidget)

public:
    enum class NameFilteringMode
    {
        Everywhere,
        OnlyNames
    };
    Q_ENUM(NameFilteringMode)

    enum class Status
    {
        Ready,
        Ongoing,
        Finished,
        Error,
        Aborted,
        NoResults
    };

    SearchJobWidget(const QString &id, const QString &searchPattern, const QList<SearchResult> &searchResults, IGUIApplication *app, QWidget *parent = nullptr);
    SearchJobWidget(const QString &id, SearchHandler *searchHandler, IGUIApplication *app, QWidget *parent = nullptr);
    ~SearchJobWidget() override;

    QString id() const;
    QString searchPattern() const;
    QList<SearchResult> searchResults() const;
    Status status() const;
    int visibleResultsCount() const;
    LineEdit *lineEditSearchResultsFilter() const;

    void assignSearchHandler(SearchHandler *searchHandler);
    void cancelSearch();

signals:
    void resultsCountUpdated();
    void statusChanged();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void displayColumnHeaderMenu();

private:
    SearchJobWidget(const QString &id, IGUIApplication *app, QWidget *parent);

    void loadSettings();
    void saveSettings() const;
    void updateFilter();
    void filterSearchResults(const QString &name);
    void showFilterContextMenu();
    void contextMenuEvent(QContextMenuEvent *event) override;
    void onItemDoubleClicked(const QModelIndex &index);
    void searchFinished(bool cancelled);
    void searchFailed();
    void appendSearchResults(const QList<SearchResult> &results);
    void updateResultsCount();
    void setStatus(Status value);
    void downloadTorrent(const QModelIndex &rowIndex, AddTorrentOption option = AddTorrentOption::Default);
    void addTorrentToSession(const QString &source, AddTorrentOption option = AddTorrentOption::Default);
    void fillFilterComboBoxes();
    NameFilteringMode filteringMode() const;
    QHeaderView *header() const;
    int visibleColumnsCount() const;
    void setRowColor(int row, const QColor &color);
    void setRowVisited(int row);
    void onUIThemeChanged();

    void downloadTorrents(AddTorrentOption option = AddTorrentOption::Default);
    void openTorrentPages() const;
    void copyTorrentURLs() const;
    void copyTorrentDownloadLinks() const;
    void copyTorrentNames() const;
    void copyField(int column) const;

    SettingValue<NameFilteringMode> m_nameFilteringMode;

    QString m_id;
    QString m_searchPattern;
    QList<SearchResult> m_searchResults;
    Ui::SearchJobWidget *m_ui = nullptr;
    SearchHandler *m_searchHandler = nullptr;
    QStandardItemModel *m_searchListModel = nullptr;
    SearchSortModel *m_proxyModel = nullptr;
    LineEdit *m_lineEditSearchResultsFilter = nullptr;
    Status m_status = Status::Ready;
    bool m_noSearchResults = true;
};

Q_DECLARE_METATYPE(SearchJobWidget::NameFilteringMode)
