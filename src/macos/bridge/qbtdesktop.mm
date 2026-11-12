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

#import "qbtdesktop.h"

#include <QCoreApplication>

#include "base/addtorrentmanager.h"
#include "base/bittorrent/session.h"
#include "base/bittorrent/torrent.h"
#include "base/global.h"
#include "base/interfaces/iapplication.h"
#include "base/preferences.h"
#include "base/settingsstorage.h"
#include "bridgeutils.h"

using namespace BridgeUtils;

namespace
{
    // The keys the Qt UI's DesktopIntegration and Application use.
    const QString NotificationsEnabledKey = u"GUI/Notifications/Enabled"_s;
    const QString TorrentAddedNotificationsKey = u"Application/GUI/Notifications/TorrentAdded"_s;
}

@interface QBTDesktopEvent ()
- (instancetype)initWithKind:(QBTDesktopEventKind)kind torrentID:(NSString *)torrentID name:(NSString *)name message:(NSString *)message;
@end

@implementation QBTDesktopEvent

- (instancetype)initWithKind:(QBTDesktopEventKind)kind torrentID:(NSString *)torrentID name:(NSString *)name message:(NSString *)message
{
    if ((self = [super init]))
    {
        _kind = kind;
        _torrentID = [torrentID copy];
        _name = [name copy];
        _message = [message copy];
    }
    return self;
}

@end

@implementation QBTDesktopEvents
{
    QObject *_connections;
}

+ (QBTDesktopEvents *)shared
{
    static QBTDesktopEvents *shared = [[QBTDesktopEvents alloc] init];
    return shared;
}

- (instancetype)init
{
    if ((self = [super init]))
    {
        _connections = new QObject;
        auto *session = BitTorrent::Session::instance();
        __weak QBTDesktopEvents *weakSelf = self;
        const auto connectAll = [weakSelf, connections = _connections, session]
        {
            const auto send = [weakSelf](const QBTDesktopEventKind kind, const BitTorrent::Torrent *torrent, const QString &name, const QString &message)
            {
                QBTDesktopEvents *strongSelf = weakSelf;
                if (!strongSelf || !strongSelf.onEvent)
                    return;
                NSString *torrentID = torrent ? toNS(torrent->id().toString()) : @"";
                strongSelf.onEvent([[QBTDesktopEvent alloc] initWithKind:kind torrentID:torrentID name:toNS(name) message:toNS(message)]);
            };

            using BitTorrent::Session;
            using BitTorrent::Torrent;
            QObject::connect(session, &Session::torrentFinished, connections, [send](const Torrent *torrent)
            {
                send(QBTDesktopEventKindDownloadFinished, torrent, torrent->name(), {});
            });
            QObject::connect(session, &Session::torrentIOError, connections, [send](const Torrent *torrent, const QString &message)
            {
                send(QBTDesktopEventKindIOError, torrent, torrent->name(), message);
            });

            // The application creates its add manager once the session is restored.
            auto *app = dynamic_cast<IApplication *>(QCoreApplication::instance());
            AddTorrentManager *manager = app ? app->addTorrentManager() : nullptr;
            if (!manager)
                return;
            QObject::connect(manager, &AddTorrentManager::torrentAdded, connections, [send]([[maybe_unused]] const QString &source, const Torrent *torrent)
            {
                send(QBTDesktopEventKindTorrentAdded, torrent, torrent->name(), {});
            });
            QObject::connect(manager, &AddTorrentManager::addTorrentFailed, connections, [send](const QString &source, const QString &reason)
            {
                send(QBTDesktopEventKindAddFailed, nullptr, source, reason);
            });
        };

        // Queued, so that the application has made its add manager by then.
        if (session->isRestored())
            QMetaObject::invokeMethod(_connections, connectAll, Qt::QueuedConnection);
        else
            QObject::connect(session, &BitTorrent::Session::restored, _connections, connectAll, Qt::QueuedConnection);
    }
    return self;
}

- (void)dealloc
{
    delete _connections;
}

@end

@implementation QBTDesktopSettings

+ (BOOL)notificationsEnabled
{
    return SettingsStorage::instance()->loadValue(NotificationsEnabledKey, true);
}

+ (void)setNotificationsEnabled:(BOOL)enabled
{
    SettingsStorage::instance()->storeValue(NotificationsEnabledKey, static_cast<bool>(enabled));
}

+ (BOOL)torrentAddedNotificationsEnabled
{
    return SettingsStorage::instance()->loadValue(TorrentAddedNotificationsKey, false);
}

+ (void)setTorrentAddedNotificationsEnabled:(BOOL)enabled
{
    SettingsStorage::instance()->storeValue(TorrentAddedNotificationsKey, static_cast<bool>(enabled));
}

+ (BOOL)speedInDockEnabled
{
    return Preferences::instance()->isSpeedInDockEnabled();
}

+ (void)setSpeedInDockEnabled:(BOOL)enabled
{
    Preferences::instance()->setSpeedInDockEnabled(enabled);
}

+ (BOOL)confirmOnExit
{
    return Preferences::instance()->confirmOnExit();
}

+ (void)setConfirmOnExit:(BOOL)confirm
{
    Preferences::instance()->setConfirmOnExit(confirm);
}

@end
