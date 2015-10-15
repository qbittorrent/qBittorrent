/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2010  Christophe Dumez
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

#ifndef RSSDOWNLOADRULELIST_H
#define RSSDOWNLOADRULELIST_H

#include <QList>
#include <QHash>
#include <QVariantHash>

#include "rssdownloadrule.h"

namespace Rss
{
    class DownloadRuleList
    {
        Q_DISABLE_COPY(DownloadRuleList)

    public:
        DownloadRuleList();

        DownloadRulePtr findMatchingRule(const QString &feedUrl, const QString &articleTitle) const;
        // Operators
        void saveRule(const DownloadRulePtr &rule);
        void removeRule(const QString &name);
        void renameRule(const QString &oldName, const QString &newName);
        DownloadRulePtr getRule(const QString &name) const;
        QStringList ruleNames() const;
        bool isEmpty() const;
        void saveRulesToStorage();
        bool serialize(const QString &path);
        bool unserialize(const QString &path);
        void replace(DownloadRuleList *other);

    private:
        void loadRulesFromStorage();
        void loadRulesFromVariantHash(const QVariantHash &l);
        QVariantHash toVariantHash() const;

    private:
        QHash<QString, DownloadRulePtr> m_rules;
        QHash<QString, QStringList> m_feedRules;
    };
}

#endif // RSSDOWNLOADFILTERLIST_H
