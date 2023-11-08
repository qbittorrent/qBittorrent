/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2023  Nick Korotysh <nick.korotysh@gmail.com>
 * Copyright (C) 2007-2023 Transmission authors and contributors.
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

#import "badgeview.h"

#include <QString>

#include "base/utils/misc.h"

static const CGFloat kBetweenPadding = 2.0;
static const NSColor *const kUploadBadgeColor = [NSColor colorWithRed:0.094 green:0.243 blue:0.835 alpha:0.9];    // #183ed5
static const NSColor *const kDownloadBadgeColor = [NSColor colorWithRed:0.216 green:0.482 blue:0.173 alpha:0.9];  // #377b2c
static const NSSize kBadgeSize = {128.0, 30.0};
static const NSString *const kUploadArrow = @"\u2191";    // or U+2b61
static const NSString *const kDownloadArrow = @"\u2193";  // or U+2b63
static CGSize kArrowInset;
static CGSize kArrowSize;

@interface BadgeView ()

@property(nonatomic) NSMutableDictionary *fAttributes;

@property(nonatomic) int64_t fDownloadRate;
@property(nonatomic) int64_t fUploadRate;

@end

@implementation BadgeView

- (instancetype)init
{
    if ((self = [super init]))
    {
        _fDownloadRate = 0.0;
        _fUploadRate = 0.0;

        NSShadow *stringShadow = [[NSShadow alloc] init];
        stringShadow.shadowOffset = NSMakeSize(2.0, -2.0);
        stringShadow.shadowBlurRadius = 4.0;

        _fAttributes = [[NSMutableDictionary alloc] initWithCapacity:3];
        _fAttributes[NSForegroundColorAttributeName] = NSColor.whiteColor;
        _fAttributes[NSShadowAttributeName] = stringShadow;
        _fAttributes[NSFontAttributeName] = [NSFont boldSystemFontOfSize:26.0];

        // DownloadBadge and UploadBadge should have the same size
        // DownArrowTemplate and UpArrowTemplate should have the same size
        kArrowInset = { kBadgeSize.height * 0.2, kBadgeSize.height * 0.1 };
        kArrowSize = [kDownloadArrow sizeWithAttributes:self.fAttributes];
    }
    return self;
}

- (BOOL)setRatesWithDownload:(int64_t)downloadRate upload:(int64_t)uploadRate
{
    // only needs update if the badges were displayed or are displayed now
    if ((self.fDownloadRate == downloadRate) && (self.fUploadRate == uploadRate))
    {
        return NO;
    }

    self.fDownloadRate = downloadRate;
    self.fUploadRate = uploadRate;

    return YES;
}

- (void)drawRect:(NSRect)rect
{
    [NSApp.applicationIconImage drawInRect:rect fromRect:NSZeroRect operation:NSCompositingOperationSourceOver fraction:1.0];

    const BOOL upload = self.fUploadRate >= 0.1;
    const BOOL download = self.fDownloadRate >= 0.1;
    CGFloat bottom = 0.0;
    if (download)
    {
        [self badge:kDownloadBadgeColor arrow:kDownloadArrow
            string:Utils::Misc::friendlyUnitCompact(self.fDownloadRate).toNSString()
            atHeight:bottom];

        if (upload)
        {
            bottom += kBadgeSize.height + kBetweenPadding; // upload rate above download rate
        }
    }
    if (upload)
    {
        [self badge:kUploadBadgeColor arrow:kUploadArrow
            string:Utils::Misc::friendlyUnitCompact(self.fUploadRate).toNSString()
            atHeight:bottom];
    }
}

- (void)badge:(const NSColor*)badgeColor arrow:(const NSString*)arrowSymbol string:(const NSString*)string atHeight:(CGFloat)height
{
    // background
    const NSRect badgeRect = { { 0.0, height }, kBadgeSize };
    [badgeColor setFill];
    const CGFloat r = kBadgeSize.height / 2.0;
    [[NSBezierPath bezierPathWithRoundedRect:badgeRect xRadius:r yRadius:r] fill];

    // string is in center of image
    const NSSize stringSize = [string sizeWithAttributes:self.fAttributes];
    NSRect stringRect;
    stringRect.origin.x = NSMidX(badgeRect) - stringSize.width * 0.5 + kArrowInset.width; // adjust for arrow
    stringRect.origin.y = NSMidY(badgeRect) - stringSize.height * 0.5 + 1.0; // adjust for shadow
    stringRect.size = stringSize;
    [string drawInRect:stringRect withAttributes:self.fAttributes];

    // arrow
    const NSRect arrowRect = { { kArrowInset.width, stringRect.origin.y }, kArrowSize };
    [arrowSymbol drawInRect:arrowRect withAttributes:self.fAttributes];
}

@end
