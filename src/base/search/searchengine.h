/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015-2022  Vladimir Golovnev <glassez@yandex.ru>
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

#include <memory>

#include <QHash>
#include <QObject>

#include "base/exceptions.h"

class SearchHandler;

QBT_DECLARE_EXCEPTION(IndexerNotFoundError, RuntimeError)
QBT_DECLARE_EXCEPTION(IndexerAlreadyExistsError, RuntimeError)

struct IndexerOptions
{
    QString url;
    QString apiKey;
};

struct IndexerInfo
{
    IndexerOptions options;
    bool enabled = true;
};

class SearchEngine final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SearchEngine)

public:
    SearchEngine();

    static SearchEngine *instance();
    static void freeInstance();

    QHash<QString, IndexerInfo> indexers() const;
    QStringList supportedCategories() const;

    void addIndexer(const QString &name, const IndexerOptions &options);
    void setIndexerOptions(const QString &name, const IndexerOptions &options);
    void enableIndexer(const QString &name, bool enabled = true);
    void removeIndexer(const QString &name);

    bool hasEnabledIndexers() const;

    SearchHandler *startSearch(const QString &pattern, const QString &category);

    static QString categoryFullName(const QString &categoryName);

signals:
    void indexerAdded(const QString &name);
    void indexerOptionsChanged(const QString &name);
    void indexerStateChanged(const QString &name);
    void indexerRemoved(const QString &name);

private:
    void load();
    void store() const;

    static QPointer<SearchEngine> m_instance;

    QHash<QString, IndexerInfo> m_indexers;
};
