/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  qBittorrent contributors
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

#import <AppKit/AppKit.h>

#include "testproptabbar_macos.h"

namespace
{
    QColor toQColor(NSColor *color)
    {
        __block QColor result;
        [NSApp.effectiveAppearance performAsCurrentDrawingAppearance:
        ^{
            NSColor *sRGBColor = [color colorUsingColorSpace:NSColorSpace.sRGBColorSpace];
            if (sRGBColor)
            {
                result = QColor::fromRgbF(sRGBColor.redComponent, sRGBColor.greenComponent
                        , sRGBColor.blueComponent, sRGBColor.alphaComponent);
            }
        }];

        return result;
    }
}

void TestMacOS::activateApplication()
{
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 140000
    if (@available(macOS 14.0, *))
    {
        [NSApp activate];
        return;
    }
#endif

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    [NSApp activateIgnoringOtherApps:YES];
#pragma clang diagnostic pop
}

void TestMacOS::setApplicationAppearance(const bool dark)
{
    NSApp.appearance = [NSAppearance appearanceNamed:(dark ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua)];
}

void TestMacOS::resetApplicationAppearance()
{
    NSApp.appearance = nil;
}

bool TestMacOS::isDarkAppearance()
{
    NSString *appearanceName = [NSApp.effectiveAppearance bestMatchFromAppearancesWithNames:
            @[NSAppearanceNameDarkAqua, NSAppearanceNameAqua]];
    return [appearanceName isEqualToString:NSAppearanceNameDarkAqua];
}

QColor TestMacOS::controlAccentColor()
{
    return toQColor(NSColor.controlAccentColor);
}

QColor TestMacOS::alternateSelectedControlTextColor()
{
    return toQColor(NSColor.alternateSelectedControlTextColor);
}
