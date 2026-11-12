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

// Macros that define a settings property from a core getter and setter,
// shared by the bridge's settings classes. Not for Swift.

#pragma once

#import <Foundation/Foundation.h>

#include "base/path.h"
#include "bridgeutils.h"

// A property read with `get` and written with `set` on the given object.
#define QBT_PROPERTY(Type, name, Setter, object, get, set, toBridge, fromBridge) \
    + (Type)name { return toBridge((object)->get()); } \
    + (void)Setter:(Type)value { (object)->set(fromBridge(value)); }

#define QBT_SAME(value) (value)
#define QBT_BOOL(Name, Setter, object, get, set) QBT_PROPERTY(BOOL, Name, Setter, object, get, set, QBT_SAME, static_cast<bool>)
#define QBT_INT(Name, Setter, object, get, set) QBT_PROPERTY(int, Name, Setter, object, get, set, QBT_SAME, QBT_SAME)
#define QBT_PATH(Name, Setter, object, get, set) QBT_PROPERTY(NSString *, Name, Setter, object, get, set, BridgeUtils::pathToNS, BridgeUtils::toPath)
#define QBT_STRING(Name, Setter, object, get, set) QBT_PROPERTY(NSString *, Name, Setter, object, get, set, BridgeUtils::toNS, BridgeUtils::toQt)
// An enum stored in the core, passed to Swift as its integer value.
#define QBT_ENUM(Name, Setter, object, get, set, Enum) QBT_PROPERTY(int, Name, Setter, object, get, set, static_cast<int>, static_cast<Enum>)

namespace BridgeUtils
{
    inline NSString *pathToNS(const Path &path)
    {
        return toNS(path.toString());
    }

    inline Path toPath(NSString *string)
    {
        return Path(toQt(string));
    }

    // One item per line, as the Qt UI's text boxes take lists.
    inline NSString *linesToNS(const QStringList &lines)
    {
        return toNS(lines.join(u'\n'));
    }

    inline QStringList linesToQt(NSString *string)
    {
        return toQt(string).split(u'\n', Qt::SkipEmptyParts);
    }
}
