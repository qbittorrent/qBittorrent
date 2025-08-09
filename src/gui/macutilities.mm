/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017  Brian Kendall <brian@briankendall.net>
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

#include "macutilities.h"

#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <UserNotifications/UserNotifications.h>
#include <objc/message.h>

#include <QCoreApplication>
#include <QPixmap>
#include <QSize>
#include <QString>

#include "base/logger.h"
#include "base/path.h"

QImage qt_mac_toQImage(CGImageRef image);

namespace MacUtils
{
    QPixmap pixmapForExtension(const QString &ext, const QSize &size)
    {
        @autoreleasepool
        {
            const NSImage *image = [[NSWorkspace sharedWorkspace]
                iconForContentType:[UTType typeWithFilenameExtension:ext.toNSString()]];
            if (image)
            {
                NSRect rect = NSMakeRect(0, 0, size.width(), size.height());
                CGImageRef cgImage = [image CGImageForProposedRect:&rect context:nil hints:nil];
                return QPixmap::fromImage(qt_mac_toQImage(cgImage));
            }

            return QPixmap();
        }
    }

    void overrideDockClickHandler(bool (*dockClickHandler)(id, SEL, ...))
    {
        NSApplication *appInst = [NSApplication sharedApplication];

        if (!appInst)
            return;

        Class delClass = [[appInst delegate] class];
        SEL shouldHandle = sel_registerName("applicationShouldHandleReopen:hasVisibleWindows:");

        if (class_getInstanceMethod(delClass, shouldHandle))
        {
            if (class_replaceMethod(delClass, shouldHandle, reinterpret_cast<IMP>(dockClickHandler), "B@:"))
                qDebug("Registered dock click handler (replaced original method)");
            else
                qWarning("Failed to replace method for dock click handler");
        }
        else
        {
            if (class_addMethod(delClass, shouldHandle, reinterpret_cast<IMP>(dockClickHandler), "B@:"))
                qDebug("Registered dock click handler");
            else
                qWarning("Failed to register dock click handler");
        }
    }

    void askForNotificationPermission()
    {
        @autoreleasepool
        {
            [UNUserNotificationCenter.currentNotificationCenter requestAuthorizationWithOptions:
                    (UNAuthorizationOptionAlert + UNAuthorizationOptionSound)
                            completionHandler:^([[maybe_unused]] BOOL granted, NSError * _Nullable error)
                            {
                                if (error)
                                {
                                    LogMsg(QCoreApplication::translate("MacUtils", "Permission for notifications not granted. Error: \"%1\"").arg
                                                                               (QString::fromNSString(error.localizedDescription)), Log::WARNING);
                                }
                            }];
        }
    }

    void displayNotification(const QString &title, const QString &message)
    {
        @autoreleasepool
        {
            UNMutableNotificationContent *content = [[UNMutableNotificationContent alloc] init];
            content.title = title.toNSString();
            content.body = message.toNSString();
            content.sound = [UNNotificationSound defaultSound];
            UNNotificationRequest *request = [UNNotificationRequest requestWithIdentifier:
                                              [[NSUUID UUID] UUIDString] content:content
                                                                                trigger:nil];
            [UNUserNotificationCenter.currentNotificationCenter
                addNotificationRequest:request withCompletionHandler:nil];
        }
    }

    void openFiles(const PathList &pathList)
    {
        @autoreleasepool
        {
            NSMutableArray *pathURLs = [NSMutableArray arrayWithCapacity:pathList.size()];

            for (const auto &path : pathList)
                [pathURLs addObject:[NSURL fileURLWithPath:path.toString().toNSString()]];

            // In some unknown way, the next line affects Qt's main loop causing the crash
            // in QApplication::exec() on processing next event after this call.
            // Even crash doesn't happen exactly after this call, it will happen on
            // application exit. Call stack and disassembly are the same in all cases.
            // But running it in another thread (aka in background) solves the issue.
            dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^
            {
                [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:pathURLs];
            });
        }
    }

    bool isMagnetLinkAssocSet()
    {
        @autoreleasepool
        {
            const NSURL *magnetStandardURL = [[NSWorkspace sharedWorkspace] URLForApplicationToOpenURL:[NSURL URLWithString:@"magnet:"]];
            const NSURL *qbtURL = [[NSBundle mainBundle] bundleURL];
            return [magnetStandardURL isEqual:qbtURL];
        }
    }

    void setMagnetLinkAssoc()
    {
        @autoreleasepool
        {
            [[NSWorkspace sharedWorkspace] setDefaultApplicationAtURL:[[NSBundle mainBundle] bundleURL]
                toOpenURLsWithScheme:@"magnet" completionHandler:nil];
        }
    }

    bool isTorrentFileAssocSet()
    {
        @autoreleasepool
        {
            const NSURL *torrentStandardURL = [[NSWorkspace sharedWorkspace]
                URLForApplicationToOpenContentType:[UTType typeWithFilenameExtension:@"torrent"]];
            const NSURL *qbtURL = [[NSBundle mainBundle] bundleURL];
            return [torrentStandardURL isEqual:qbtURL];
        }
    }

    void setTorrentFileAssoc()
    {
        @autoreleasepool
        {
            [[NSWorkspace sharedWorkspace] setDefaultApplicationAtURL:[[NSBundle mainBundle] bundleURL]
                toOpenContentType:[UTType typeWithFilenameExtension:@"torrent"]
                completionHandler:nil];
        }
    }

    QString badgeLabelText()
    {
        return QString::fromNSString(NSApp.dockTile.badgeLabel);
    }

    void setBadgeLabelText(const QString &text)
    {
        NSApp.dockTile.badgeLabel = text.toNSString();
    }
}
