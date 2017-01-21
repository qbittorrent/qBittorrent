/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez
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

#ifndef SEARCHTAB_H
#define SEARCHTAB_H

#include <QVariant> // I don't know why <QMetaType> is not enought for Qt's 4.8.7 moc
#include <QWidget>

#define ENGINE_URL_COLUMN 4
#define URL_COLUMN 5

class QLabel;
class QModelIndex;
class QTreeView;
class QHeaderView;
class QStandardItemModel;
class QVBoxLayout;

class SearchSortModel;
class SearchListDelegate;
class SearchWidget;

namespace Ui
{
    class SearchTab;
}

class SearchTab: public QWidget
{
    Q_OBJECT

public:

    enum NameFilteringMode
    {
        Everywhere,
        OnlyNames
    };

    Q_ENUMS(NameFilteringMode)

    explicit SearchTab(SearchWidget *parent);
    ~SearchTab();

    QStandardItemModel* getCurrentSearchListModel() const;
    SearchSortModel* getCurrentSearchListProxy() const;
    QTreeView* getCurrentTreeView() const;
    QHeaderView* header() const;

    void setRowColor(int row, const QColor &color);

    enum class Status
    {
        Ongoing,
        Finished,
        Error,
        Aborted,
        NoResults
    };

    void setStatus(Status value);
    Status status() const;

    void updateResultsCount();

public slots:
    void downloadItem(const QModelIndex &index);

private slots:
    void loadSettings();
    void saveSettings() const;
    void updateFilter();
    void displayToggleColumnsMenu(const QPoint&);

private:
    void fillFilterComboBoxes();
    NameFilteringMode filteringMode() const;
    static QString statusText(Status st);
    static QString statusIconName(Status st);

    Ui::SearchTab *m_ui;
    QTreeView *m_resultsBrowser;
    QStandardItemModel *m_searchListModel;
    SearchSortModel *m_proxyModel;
    SearchListDelegate *m_searchDelegate;
    SearchWidget *m_parent;
    Status m_status;
};

Q_DECLARE_METATYPE(SearchTab::NameFilteringMode)

#endif // SEARCHTAB_H
