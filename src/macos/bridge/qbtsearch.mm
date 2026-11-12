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

#import "qbtsearch.h"

#include <QHash>
#include <QPointer>

#include "base/global.h"
#include "base/path.h"
#include "base/preferences.h"
#include "base/search/searchdownloadhandler.h"
#include "base/search/searchhandler.h"
#include "base/search/searchpluginmanager.h"
#include "base/utils/foreignapps.h"
#include "bridgeutils.h"

using namespace BridgeUtils;

namespace
{
    NSArray<NSString *> *toNS(const QStringList &strings)
    {
        NSMutableArray<NSString *> *array = [NSMutableArray arrayWithCapacity:strings.size()];
        for (const QString &string : strings)
            [array addObject:BridgeUtils::toNS(string)];
        return array;
    }

    QStringList toQt(NSArray<NSString *> *strings)
    {
        QStringList list;
        list.reserve(strings.count);
        for (NSString *string in strings)
            list.append(BridgeUtils::toQt(string));
        return list;
    }
}

@interface QBTSearchResult ()
- (instancetype)initWithResult:(const SearchResult &)result;
@end

@implementation QBTSearchResult

- (instancetype)initWithResult:(const SearchResult &)result
{
    if ((self = [super init]))
    {
        _name = toNS(result.fileName);
        _fileURL = toNS(result.fileUrl);
        _size = result.fileSize;
        _seeders = result.nbSeeders;
        _leechers = result.nbLeechers;
        _engineName = toNS(result.engineName);
        _engineFullName = toNS(result.engineFullName);
        _siteURL = toNS(result.siteUrl);
        _descriptionURL = toNS(result.descrLink);
        _publishedOn = result.pubDate.isValid() ? toNS(result.pubDate) : nil;
    }
    return self;
}

@end

@interface QBTSearchPlugin ()
- (instancetype)initWithInfo:(const SearchPluginInfo &)info;
@end

@implementation QBTSearchPlugin

- (instancetype)initWithInfo:(const SearchPluginInfo &)info
{
    if ((self = [super init]))
    {
        _name = toNS(info.name);
        _fullName = toNS(info.fullName);
        _version = toNS(info.version.toString());
        _url = toNS(info.url);
        _categories = ::toNS(info.supportedCategories);
        _iconPath = info.iconPath.isEmpty() ? nil : toNS(info.iconPath.toString());
        _isEnabled = info.enabled;
    }
    return self;
}

@end

@interface QBTSearchJob ()
- (instancetype)initWithHandler:(SearchHandler *)handler;
@end

@implementation QBTSearchJob
{
    QPointer<SearchHandler> _handler;
    BOOL _isFinished;
}

- (instancetype)initWithHandler:(SearchHandler *)handler
{
    if ((self = [super init]))
    {
        _handler = handler;
        _pattern = toNS(handler->pattern());

        __weak QBTSearchJob *weakSelf = self;
        QObject::connect(handler, &SearchHandler::newSearchResults, handler, [weakSelf](const QList<SearchResult> &results)
        {
            QBTSearchJob *job = weakSelf;
            if (!job || !job.onResults)
                return;
            NSMutableArray<QBTSearchResult *> *array = [NSMutableArray arrayWithCapacity:results.size()];
            for (const SearchResult &result : results)
                [array addObject:[[QBTSearchResult alloc] initWithResult:result]];
            job.onResults(array);
        });
        QObject::connect(handler, &SearchHandler::searchFinished, handler, [weakSelf](const bool cancelled)
        {
            [weakSelf finishCancelled:cancelled error:nil];
        });
        // Emitted with an empty message when the process just exits with an error.
        QObject::connect(handler, &SearchHandler::searchFailed, handler, [weakSelf](const QString &message)
        {
            [weakSelf finishCancelled:NO error:toNS(message)];
        });
    }
    return self;
}

- (void)dealloc
{
    if (_handler)
    {
        _handler->cancelSearch();
        _handler->deleteLater();
    }
}

- (void)finishCancelled:(BOOL)cancelled error:(nullable NSString *)error
{
    // A crashed process reports both an error and a failed exit.
    if (_isFinished)
        return;
    _isFinished = YES;
    if (self.onFinished)
        self.onFinished(cancelled, error);
}

- (BOOL)isActive
{
    return _handler && _handler->isActive();
}

- (void)cancel
{
    if (_handler)
        _handler->cancelSearch();
}

@end

@implementation QBTSearchEngine
{
    BOOL _isConnected;
}

+ (QBTSearchEngine *)shared
{
    static QBTSearchEngine *engine = [[QBTSearchEngine alloc] init];
    return engine;
}

- (QBTPythonStatus)pythonStatus
{
    const Utils::ForeignApps::PythonInfo info = Utils::ForeignApps::pythonInfo();
    if (!info.isValid())
        return QBTPythonStatusMissing;
    return info.isSupportedVersion() ? QBTPythonStatusReady : QBTPythonStatusOutdated;
}

- (NSString *)pythonVersion
{
    const Utils::ForeignApps::PythonInfo info = Utils::ForeignApps::pythonInfo();
    return info.isValid() ? toNS(info.version.toString()) : @"";
}

- (NSString *)pythonPath
{
    return toNS(Utils::ForeignApps::pythonInfo().executablePath.toString());
}

- (NSString *)minimumPythonVersion
{
    return toNS(Utils::ForeignApps::PythonInfo::MINIMUM_SUPPORTED_VERSION.toString());
}

- (NSString *)preferredPythonPath
{
    return toNS(Preferences::instance()->getPythonExecutablePath().toString());
}

- (void)setPreferredPythonPath:(NSString *)path
{
    Preferences::instance()->setPythonExecutablePath(Path(toQt(path)));
}

// The manager runs Python when it is created, so it waits until Python is ready.
- (nullable SearchPluginManager *)manager
{
    if (self.pythonStatus != QBTPythonStatusReady)
        return nullptr;

    SearchPluginManager *manager = SearchPluginManager::instance();
    if (!_isConnected)
    {
        _isConnected = YES;
        [self connectManager:manager];
    }
    return manager;
}

- (void)connectManager:(SearchPluginManager *)manager
{
    __weak QBTSearchEngine *weakSelf = self;
    const auto changed = [weakSelf]
    {
        QBTSearchEngine *engine = weakSelf;
        if (engine.onPluginsChanged)
            engine.onPluginsChanged();
    };
    const auto event = [weakSelf](const QBTSearchPluginEvent event, const QString &name, const QString &reason)
    {
        QBTSearchEngine *engine = weakSelf;
        if (engine.onPluginEvent)
            engine.onPluginEvent(event, toNS(name), toNS(reason));
    };

    QObject::connect(manager, &SearchPluginManager::pluginEnabled, manager, changed);
    QObject::connect(manager, &SearchPluginManager::pluginInstalled, manager, [changed, event](const QString &name)
    {
        changed();
        event(QBTSearchPluginEventInstalled, name, {});
    });
    QObject::connect(manager, &SearchPluginManager::pluginInstallationFailed, manager, [event](const QString &name, const QString &reason)
    {
        event(QBTSearchPluginEventInstallationFailed, name, reason);
    });
    QObject::connect(manager, &SearchPluginManager::pluginUpdated, manager, [changed, event](const QString &name)
    {
        changed();
        event(QBTSearchPluginEventUpdated, name, {});
    });
    QObject::connect(manager, &SearchPluginManager::pluginUpdateFailed, manager, [event](const QString &name, const QString &reason)
    {
        event(QBTSearchPluginEventUpdateFailed, name, reason);
    });
    QObject::connect(manager, &SearchPluginManager::pluginUninstalled, manager, [changed, event](const QString &name)
    {
        changed();
        event(QBTSearchPluginEventUninstalled, name, {});
    });
    QObject::connect(manager, &SearchPluginManager::checkForUpdatesFinished, manager
        , [manager, event](const QHash<QString, SearchPluginVersion> &updateInfo)
    {
        if (updateInfo.isEmpty())
            event(QBTSearchPluginEventAllUpToDate, {}, {});
        for (auto it = updateInfo.cbegin(); it != updateInfo.cend(); ++it)
            manager->updatePlugin(it.key());
    });
    QObject::connect(manager, &SearchPluginManager::checkForUpdatesFailed, manager, [event](const QString &reason)
    {
        event(QBTSearchPluginEventUpdateCheckFailed, {}, reason);
    });
}

- (NSArray<QBTSearchPlugin *> *)plugins
{
    SearchPluginManager *manager = self.manager;
    if (!manager)
        return @[];

    NSMutableArray<QBTSearchPlugin *> *plugins = [NSMutableArray array];
    for (const QString &name : manager->allPlugins())
    {
        if (const SearchPluginInfo *info = manager->pluginInfo(name))
            [plugins addObject:[[QBTSearchPlugin alloc] initWithInfo:*info]];
    }
    return plugins;
}

- (NSArray<NSString *> *)categories
{
    SearchPluginManager *manager = self.manager;
    if (!manager)
        return @[@"all"];

    QStringList categories = manager->supportedCategories();
    categories.removeAll(u"all"_s);
    categories.prepend(u"all"_s);
    return ::toNS(categories);
}

- (NSString *)nameOfCategory:(NSString *)categoryID
{
    return toNS(SearchPluginManager::categoryFullName(toQt(categoryID)));
}

- (nullable QBTSearchJob *)startSearch:(NSString *)pattern category:(NSString *)categoryID plugins:(NSArray<NSString *> *)pluginNames
{
    SearchPluginManager *manager = self.manager;
    if (!manager || (pluginNames.count == 0))
        return nil;

    SearchHandler *handler = manager->startSearch(toQt(pattern), toQt(categoryID), ::toQt(pluginNames));
    return [[QBTSearchJob alloc] initWithHandler:handler];
}

- (void)fetchTorrentForResult:(QBTSearchResult *)result completion:(void (^)(NSString *source, NSString *_Nullable error))completion
{
    if ([result.fileURL.lowercaseString hasPrefix:@"magnet:"])
    {
        completion(result.fileURL, nil);
        return;
    }

    SearchPluginManager *manager = self.manager;
    if (!manager)
    {
        completion(@"", @"");
        return;
    }

    SearchDownloadHandler *handler = manager->downloadTorrent(toQt(result.engineName), toQt(result.fileURL));
    QObject::connect(handler, &SearchDownloadHandler::downloadFinished, handler
        , [handler, completion](const QString &path, const QString &error)
    {
        completion(toNS(path), (path.isEmpty() ? toNS(error) : nil));
        handler->deleteLater();
    });
}

- (void)setPlugin:(NSString *)name enabled:(BOOL)enabled
{
    if (SearchPluginManager *manager = self.manager)
        manager->enablePlugin(toQt(name), enabled);
}

- (void)installPluginFromSource:(NSString *)source
{
    if (SearchPluginManager *manager = self.manager)
        manager->installPlugin(toQt(source));
}

- (BOOL)uninstallPlugin:(NSString *)name
{
    SearchPluginManager *manager = self.manager;
    return manager && manager->uninstallPlugin(toQt(name));
}

- (void)updateAllPlugins
{
    if (SearchPluginManager *manager = self.manager)
        manager->checkForUpdates();
}

@end
