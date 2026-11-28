/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2025  Dru Still <drustill4@gmail.com>
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

#import "itemview.h"

#include <QString>
#include "base/utils/misc.h"

namespace
{
    static NSString * const kUploadArrow = @"\u2191";
    static NSString * const kDownloadArrow = @"\u2193";
}

@interface ItemView ()

@property(nonatomic) int64_t fDownloadRate;
@property(nonatomic) int64_t fUploadRate;

@property(strong) NSStatusItem *statusItem;
@property(nonatomic, strong) NSMenu *contextMenu;
@property(nonatomic) BOOL opensWindowOnClick;
@property(copy) void (^clickHandler)(void);
@end

@implementation ItemView

- (instancetype)init
{
    if ((self = [super init])){
        self.fDownloadRate = -1;
        self.fUploadRate = -1;

        NSString *iconPath = [NSBundle.mainBundle pathForResource:@"qbittorrent-tray-light" ofType:@"svg"];
        NSImage *icon = [[NSImage alloc] initWithContentsOfFile:iconPath];
        [icon setSize:NSMakeSize(16, 16)];
        [icon setTemplate:YES];

        self.statusItem = [[NSStatusBar systemStatusBar] statusItemWithLength:NSSquareStatusItemLength];
        // Gives the item a stable identity so macOS remembers where the user dragged it,
        // and so menu bar managers can track it by name instead of by position.
        self.statusItem.autosaveName = @"qBittorrent";
        self.statusItem.button.image = icon;
        self.statusItem.button.imagePosition = NSImageOnly;
        self.statusItem.button.imageScaling = NSImageScaleProportionallyDown;
        self.statusItem.button.title = @"";
        self.statusItem.button.target = self;
        self.statusItem.button.action = @selector(handleClick:);
        // Act on mouse *up* only: a menu opened on mouse down runs inside the button's mouse
        // tracking, which then waits for a mouse up the menu has already consumed. That
        // blocks the event loop, so choosing "Quit" from the menu never lets the process exit.
        [self.statusItem.button sendActionOn:(NSEventMaskLeftMouseUp | NSEventMaskRightMouseUp)];

        [self setRatesWithDownload:0 upload:0];
    }
    return self;
}

- (BOOL)setRatesWithDownload:(int64_t)downloadRate upload:(int64_t)uploadRate
{
    if ((downloadRate == self.fDownloadRate) && (uploadRate == self.fUploadRate))
        return NO;

    self.fDownloadRate = downloadRate;
    self.fUploadRate = uploadRate;

    const QString uploadString = Utils::Misc::friendlyUnit(self.fUploadRate, true);
    const QString downloadString = Utils::Misc::friendlyUnit(self.fDownloadRate, true);

    NSString *uploadNSString = uploadString.toNSString();
    NSString *downloadNSString = downloadString.toNSString();

    self.statusItem.button.toolTip = [NSString stringWithFormat:@"qBittorrent\n%@ %@ · %@ %@",
                                      kDownloadArrow, downloadNSString,
                                      kUploadArrow, uploadNSString];

    return YES;
}

- (void)setVisible:(BOOL)visible
{
    // Called again on every preference change. Re-asserting the same value makes the item
    // churn for no reason, which menu bar managers that snapshot the bar can trip over.
    if (self.statusItem.visible == visible)
        return;

    self.statusItem.visible = visible;
}

- (void)setStatusMenu:(NSMenu *)menu
{
    self.contextMenu = menu;
    [self updateClickBehavior];
}

- (void)setOpensWindowOnClick:(BOOL)enabled
{
    if (_opensWindowOnClick == enabled)
        return;

    _opensWindowOnClick = enabled;
    [self updateClickBehavior];
}

- (void)updateClickBehavior
{
    // While a menu is assigned to the status item, AppKit presents it on any click and
    // never invokes the button action. Since macOS 27 it goes further and swallows the
    // mouse events entirely, so the menu must stay detached and be attached only for as
    // long as it takes to present it (see -showContextMenu).
    self.statusItem.menu = self.opensWindowOnClick ? nil : self.contextMenu;
}

- (void)showContextMenu
{
    if (!self.contextMenu)
        return;

    // The replacement for the deprecated -popUpStatusItemMenu:, which also keeps the
    // status item highlighted while the menu is open.
    self.statusItem.menu = self.contextMenu;
    [self.statusItem.button performClick:nil];
    self.statusItem.menu = nil;
}

- (void)handleClick:(id)sender
{
    Q_UNUSED(sender);

    NSEvent *event = NSApp.currentEvent;
    const BOOL menuRequested = (event.type == NSEventTypeRightMouseUp)
        || ((event.modifierFlags & NSEventModifierFlagControl) != 0);

    if (menuRequested)
        [self showContextMenu];
    else if (self.clickHandler)
        self.clickHandler();
}

@end
