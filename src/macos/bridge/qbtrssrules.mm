/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  svbmwjwj <255624542+svbmwjwj@users.noreply.github.com>
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

#import "qbtrssrules.h"
#import "qbtrssprivate.h"

#include <algorithm>

#include "base/bittorrent/addtorrentparams.h"
#include "base/global.h"
#include "base/path.h"
#include "base/rss/rss_article.h"
#include "base/rss/rss_autodownloader.h"
#include "base/rss/rss_feed.h"
#include "base/rss/rss_session.h"
#include "bridgeutils.h"

using namespace BridgeUtils;

namespace
{
    NSError *makeError(const QString &message)
    {
        return [NSError errorWithDomain:@"QBTRSS" code:1 userInfo:@{NSLocalizedDescriptionKey: toNS(message)}];
    }
}

@implementation QBTRSSRule

- (instancetype)initWithName:(NSString *)name
{
    return [self initWithRule:RSS::AutoDownloadRule(toQt(name))];
}

- (instancetype)initWithRule:(const RSS::AutoDownloadRule &)rule
{
    if ((self = [super init]))
    {
        _name = toNS(rule.name());
        _enabled = rule.isEnabled();
        _priority = rule.priority();
        _mustContain = toNS(rule.mustContain());
        _mustNotContain = toNS(rule.mustNotContain());
        _useRegex = rule.useRegex();
        _useSmartFilter = rule.useSmartFilter();
        _episodeFilter = toNS(rule.episodeFilter());
        NSMutableArray<NSString *> *urls = [NSMutableArray array];
        for (const QString &url : asConst(rule.feedURLs()))
            [urls addObject:toNS(url)];
        _feedURLs = urls;
        _ignoreDays = rule.ignoreDays();
        const QDateTime lastMatch = rule.lastMatch();
        _lastMatch = lastMatch.isValid() ? toNS(lastMatch) : nil;
        const BitTorrent::AddTorrentParams params = rule.addTorrentParams();
        _category = toNS(params.category);
        _savePath = toNS(params.savePath.toString());
        _addStopped = params.addStopped ? (*params.addStopped ? 1 : 0) : -1;
    }
    return self;
}

- (RSS::AutoDownloadRule)ruleBasedOn:(RSS::AutoDownloadRule)rule
{
    rule.setName(toQt(_name));
    rule.setEnabled(_enabled);
    rule.setPriority(_priority);
    rule.setMustContain(toQt(_mustContain));
    rule.setMustNotContain(toQt(_mustNotContain));
    rule.setUseRegex(_useRegex);
    rule.setUseSmartFilter(_useSmartFilter);
    rule.setEpisodeFilter(toQt(_episodeFilter));
    QStringList urls;
    for (NSString *url in _feedURLs)
        urls.append(toQt(url));
    rule.setFeedURLs(urls);
    rule.setIgnoreDays(_ignoreDays);

    BitTorrent::AddTorrentParams params = rule.addTorrentParams();
    params.category = toQt(_category);
    params.savePath = Path(toQt(_savePath));
    // As the Qt editor: a save path of its own means manual management.
    if (!params.savePath.isEmpty())
        params.useAutoTMM = false;
    else if (params.useAutoTMM == false)
        params.useAutoTMM.reset();
    params.addStopped = (_addStopped < 0) ? std::nullopt : std::optional<bool>(_addStopped == 1);
    rule.setAddTorrentParams(params);
    return rule;
}

@end

@implementation QBTRSSRules

+ (QBTRSSRules *)shared
{
    static QBTRSSRules *shared = [[QBTRSSRules alloc] init];
    return shared;
}

// MARK: Settings

- (BOOL)autoDownloadEnabled
{
    const auto *downloader = RSS::AutoDownloader::instance();
    return downloader && downloader->isProcessingEnabled();
}

- (void)setAutoDownloadEnabled:(BOOL)enabled
{
    if (auto *downloader = RSS::AutoDownloader::instance())
        downloader->setProcessingEnabled(enabled);
}

- (NSArray<NSString *> *)smartEpisodeFilters
{
    NSMutableArray<NSString *> *result = [NSMutableArray array];
    if (const auto *downloader = RSS::AutoDownloader::instance())
    {
        for (const QString &filter : asConst(downloader->smartEpisodeFilters()))
            [result addObject:toNS(filter)];
    }
    return result;
}

- (void)setSmartEpisodeFilters:(NSArray<NSString *> *)filters
{
    auto *downloader = RSS::AutoDownloader::instance();
    if (!downloader)
        return;
    QStringList list;
    for (NSString *filter in filters)
    {
        if (const QString trimmed = toQt(filter).trimmed(); !trimmed.isEmpty())
            list.append(trimmed);
    }
    downloader->setSmartEpisodeFilters(list);
}

- (BOOL)downloadRepacks
{
    const auto *downloader = RSS::AutoDownloader::instance();
    return downloader && downloader->downloadRepacks();
}

- (void)setDownloadRepacks:(BOOL)enabled
{
    if (auto *downloader = RSS::AutoDownloader::instance())
        downloader->setDownloadRepacks(enabled);
}

// MARK: Rules

- (NSArray<QBTRSSRule *> *)rules
{
    NSMutableArray<QBTRSSRule *> *result = [NSMutableArray array];
    if (const auto *downloader = RSS::AutoDownloader::instance())
    {
        for (const RSS::AutoDownloadRule &rule : asConst(downloader->rules()))
            [result addObject:[[QBTRSSRule alloc] initWithRule:rule]];
    }
    return result;
}

- (BOOL)hasRule:(NSString *)name
{
    const auto *downloader = RSS::AutoDownloader::instance();
    return downloader && downloader->hasRule(toQt(name));
}

- (BOOL)setRule:(QBTRSSRule *)rule originalName:(nullable NSString *)originalName
{
    auto *downloader = RSS::AutoDownloader::instance();
    const QString name = toQt(rule.name).trimmed();
    if (!downloader || name.isEmpty())
        return NO;

    const QString oldName = originalName ? toQt(originalName) : name;
    if ((oldName != name) && downloader->hasRule(oldName))
    {
        if (!downloader->renameRule(oldName, name))
            return NO;
    }
    rule.name = toNS(name);
    downloader->setRule([rule ruleBasedOn:downloader->ruleByName(name)]);
    return YES;
}

- (void)removeRule:(NSString *)name
{
    if (auto *downloader = RSS::AutoDownloader::instance())
        downloader->removeRule(toQt(name));
}

- (NSArray<QBTRSSArticle *> *)articlesMatchingRule:(QBTRSSRule *)rule
{
    auto *session = RSS::Session::instance();
    auto *downloader = RSS::AutoDownloader::instance();
    if (!session || !downloader)
        return @[];
    const RSS::AutoDownloadRule coreRule = [rule ruleBasedOn:downloader->ruleByName(toQt(rule.name))];
    QList<RSS::Article *> matches;
    // As AutomatedRssDownloader::updateMatchingArticles().
    for (const QString &url : asConst(coreRule.feedURLs()))
    {
        const RSS::Feed *feed = session->feedByURL(url);
        if (!feed)
            continue;
        for (RSS::Article *article : asConst(feed->articles()))
        {
            if (coreRule.matches(article->data()))
                matches.append(article);
        }
    }
    std::stable_sort(matches.begin(), matches.end(), [](const RSS::Article *left, const RSS::Article *right)
    {
        return left->date() > right->date();
    });
    NSMutableArray<QBTRSSArticle *> *result = [NSMutableArray arrayWithCapacity:matches.size()];
    for (const RSS::Article *article : asConst(matches))
        [result addObject:[[QBTRSSArticle alloc] initWithArticle:article]];
    return result;
}

- (NSData *)exportRulesInFormat:(QBTRSSRulesFormat)format
{
    const auto *downloader = RSS::AutoDownloader::instance();
    if (!downloader)
        return [NSData data];
    return downloader->exportRules(static_cast<RSS::AutoDownloader::RulesFileFormat>(format)).toNSData();
}

- (BOOL)importRules:(NSData *)data format:(QBTRSSRulesFormat)format error:(NSError **)error
{
    auto *downloader = RSS::AutoDownloader::instance();
    if (!downloader)
        return NO;
    try
    {
        downloader->importRules(QByteArray::fromNSData(data), static_cast<RSS::AutoDownloader::RulesFileFormat>(format));
        return YES;
    }
    catch (const RSS::ParsingError &parsingError)
    {
        if (error)
            *error = makeError(parsingError.message());
        return NO;
    }
}

@end
