/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016  Michael Ziminsky <mgziminsky@gmail.com>
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

#include "utorrentrssdata.h"

#include <libtorrent/lazy_entry.hpp>

#include <QDebug>
#include <QFile>
#include <QStringList>

#ifdef QBT_USES_QT5
#include <QRegularExpression>
#else
#include <QRegExp>
#endif

#include "rssfeed.h"
#include "rssdownloadrule.h"
#include "rssdownloadrulelist.h"
#include "base/utils/string.h"

using namespace Rss;
using namespace libtorrent;
using namespace Utils::String;

namespace
{
    int POSTPONE_OPTS[] = {0, 0, 0, 1, 2, 3, 4, 7, 14, 21, 30};

    enum EnabledFlags
    {
        ENABLED      = 1,
        RAW_NAME     = 2,
        MAX_PRIORITY = 4,
        SMART_FILTER = 8,
        PAUSED       = 16
    };

    enum Quality
    {
        QUALITY_ALL   = -1,
        QUALITY_480p  = 0x10000,
        QUALITY_720p  = 0x00800,
        QUALITY_1080i = 0x01000,
        QUALITY_1080p = 0x02000
    };

    void applyFilter(Rss::DownloadRule *rule, const libtorrent::lazy_entry &filter)
    {
#ifdef QBT_USES_QT5
        // Leading/Trailing *'s and *'s before/after '|'
        const QRegularExpression STRIP_PATTERN(R"((?<=^|\|)\*+|\*+(?=$|\|))");
#else
        // Leading/Trailing *'s and *'s before '|'
        // No lookbehind, so as close as it'll get without complicating things
        const QRegExp STRIP_PATTERN(R"(^\*+|\*+(?=$|\|))");
#endif

        rule->setUseRegex(true);
        rule->setMustNotContain(fromStdString(filter.dict_find_string_value("not_filter")).remove(STRIP_PATTERN).replace('*', ".*").replace('?', "."));

        QString mustContain = fromStdString(filter.dict_find_string_value("filter")).remove(STRIP_PATTERN).replace('*', ".*").replace('?', ".");

        QStringList qualityList;
        int quality = filter.dict_find_int_value("quality");
        if (quality != QUALITY_ALL) {
            if (quality & QUALITY_480p)
                qualityList << "480p";
            if (quality & QUALITY_720p)
                qualityList << "720p";
            if (quality & QUALITY_1080i)
                qualityList << "1080i";
            if (quality & QUALITY_1080p)
                qualityList << "1080p";
        }

        if (qualityList.empty())
            rule->setMustContain(mustContain);
        else
            rule->setMustContain("(" % mustContain % ").*(" % qualityList.join("|") % ")");
    }
}

UTorrentRssData::UTorrentRssData()
    : m_rules(new DownloadRuleList)
{
}

UTorrentRssData::~UTorrentRssData()
{
    delete m_rules;
}

const QList<FeedPtr>& UTorrentRssData::getFeeds() const
{
    return m_feeds;
}

const DownloadRuleList& UTorrentRssData::getRules() const
{
    return *m_rules;
}

bool UTorrentRssData::load(const QString &path)
{
    bool success = false;

    QFile file(path);
    if (file.open(QFile::ReadOnly)) {
        QByteArray contents = file.readAll();
        file.close();

        lazy_entry e;
        boost::system::error_code ec;
        lazy_bdecode(contents.constData(), contents.constData() + contents.size(), e, ec);

        if (!ec) {
            const lazy_entry *feeds = e.dict_find_list("feeds");
            const lazy_entry *filters = e.dict_find_list("filters");
            if (success = (feeds && filters))
                loadRules(*filters, loadFeeds(*feeds));
        }
    }

    return success;
}

QHash<int, QString> UTorrentRssData::loadFeeds(const libtorrent::lazy_entry &feeds)
{
    QHash<int, QString> feedUrls;

    for (int i = 0; i < feeds.list_size(); ++i) {
        const lazy_entry *feed = feeds.list_at(i);

        // Don't import disabled feeds
        if (feed->dict_find_int_value("enabled") != 1)
            continue;

        Rss::FeedPtr newFeed;
        QString url = fromStdString(feed->dict_find_string_value("url"));
        qDebug() << "Importing uTorrent Feed: " << url;

        if (url.contains('|')) {
            newFeed = Rss::FeedPtr(new Rss::Feed(url.section('|', 1)));
            if (feed->dict_find_int_value("usefeedtitle") != 1)
                newFeed->rename(url.section('|', 0, 0));
        }
        else {
            newFeed = Rss::FeedPtr(new Rss::Feed(url));
        }

        m_feeds.append(newFeed);
        feedUrls.insert(feed->dict_find_int_value("ident"), newFeed->url());
    }

    return feedUrls;
}

void UTorrentRssData::loadRules(const libtorrent::lazy_entry &filters, const QHash<int, QString> &feedUrls)
{
    for (int i = 0; i < filters.list_size(); ++i) {
        const lazy_entry *filter = filters.list_at(i);
        Rss::DownloadRule *rule = new Rss::DownloadRule();

        QString name = fromStdString(filter->dict_find_string_value("name")).trimmed();
        if (name.isEmpty())
            continue;

        rule->setName(name);
        qDebug() << "Importing uTorrent Rule: " << rule->name();

        rule->setSavePath(fromStdString(filter->dict_find_string_value("directory")));
        rule->setLastMatch(QDateTime::fromTime_t(filter->dict_find_int_value("last_match")));
        rule->setIgnoreDays(POSTPONE_OPTS[filter->dict_find_int_value("postpone_mode")]);
        rule->setCategory(fromStdString(filter->dict_find_string_value("label")));

        if (filter->dict_find_int_value("episode_filter") == 1)
            rule->setEpisodeFilter(fromStdString(filter->dict_find_string_value("episode_filter2")));

        int enabledState = filter->dict_find_int_value("enabled");
        rule->setEnabled(enabledState & EnabledFlags::ENABLED);
        if (enabledState & EnabledFlags::PAUSED)
            rule->setAddPaused(Rss::DownloadRule::AddPausedState::ALWAYS_PAUSED);

        int feedId = filter->dict_find_int_value("feed");
        if (feedId == -1)
            rule->setRssFeeds(QStringList(feedUrls.values()));
        else
            rule->setRssFeeds(QStringList(feedUrls[feedId]));

        applyFilter(rule, *filter);
        m_rules->saveRule(Rss::DownloadRulePtr(rule));
    }
}
