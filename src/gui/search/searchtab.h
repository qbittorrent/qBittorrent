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

#include <QWidget>

class QLabel;
class QTreeView;
class QHeaderView;
class QStandardItemModel;
class QSortFilterProxyModel;
class QModelIndex;
class QVBoxLayout;

class SearchSortModel;
class SearchListDelegate;
class SearchWidget;

class SearchTab: public QWidget
{
    Q_OBJECT

public:
    explicit SearchTab(SearchWidget *m_parent);

    QLabel* getCurrentLabel() const;
    QStandardItemModel* getCurrentSearchListModel() const;
    QSortFilterProxyModel* getCurrentSearchListProxy() const;
    QTreeView* getCurrentTreeView() const;
    QHeaderView* header() const;
    QString status() const;

    bool loadColWidthResultsList();
    void setRowColor(int row, QString color);
    void setStatus(const QString &value);

private slots:
    void downloadSelectedItem(const QModelIndex &index);

private:
    QVBoxLayout *m_box;
    QLabel *m_resultsLbl;
    QTreeView *m_resultsBrowser;
    QStandardItemModel *m_searchListModel;
    SearchSortModel *m_proxyModel;
    SearchListDelegate *m_searchDelegate;
    SearchWidget *m_parent;
    QString m_status;
};

#endif // SEARCHTAB_H

