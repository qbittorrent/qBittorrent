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

// The rest of the Qt UI's Speed options, in addition to those in
// QBTSettings. Each setter takes effect at once.

#pragma once

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface QBTSpeedSettings : NSObject

// Switches to the alternative speed limits during the scheduled hours. The
// scheduler checks the schedule every 30 seconds.
@property (class, nonatomic) BOOL schedulerEnabled;
// Minutes since midnight. An end before the start spans midnight.
@property (class, nonatomic) int schedulerStartMinute;
@property (class, nonatomic) int schedulerEndMinute;
// Mirrors Scheduler::Days: 0 every day, 1 weekdays, 2 weekends, 3 to 9
// Monday to Sunday.
@property (class, nonatomic) int schedulerDays;

@end

NS_ASSUME_NONNULL_END
