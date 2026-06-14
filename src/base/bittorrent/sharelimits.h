/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015-2026  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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

#pragma once

#include <algorithm>

#include <QMetaEnum>

#include "base/types.h"

namespace BitTorrent
{
    inline const qreal DEFAULT_RATIO_LIMIT = -2;
    inline const qreal NO_RATIO_LIMIT = -1;

    inline const int DEFAULT_SEEDING_TIME_LIMIT = -2;
    inline const int NO_SEEDING_TIME_LIMIT = -1;

    // Using `Q_ENUM_NS()` without a wrapper namespace in our case is not advised
    // since `Q_NAMESPACE` cannot be used when the same namespace resides at different files.
    // https://www.kdab.com/new-qt-5-8-meta-object-support-namespaces/#comment-143779
    inline namespace ShareLimitsNS
    {
        Q_NAMESPACE

        // These values should remain unchanged when adding new items
        // so as not to break the existing user settings.
        enum class ShareLimitAction
        {
            Default = -1, // special value

            Stop = 0,
            Remove = 1,
            RemoveWithContent = 3,
            EnableSuperSeeding = 2
        };

        Q_ENUM_NS(ShareLimitAction)

        enum class ShareLimitsMode
        {
            Default = -1, // special value

            MatchAny = 0,
            MatchAll = 1
        };

        Q_ENUM_NS(ShareLimitsMode)
    }

    struct ShareLimits
    {
        qreal ratioLimit = DEFAULT_RATIO_LIMIT;
        int seedingTimeLimit = DEFAULT_SEEDING_TIME_LIMIT;
        int inactiveSeedingTimeLimit = DEFAULT_SEEDING_TIME_LIMIT;

        ShareLimitsMode mode = ShareLimitsMode::Default;

        ShareLimitAction action = ShareLimitAction::Default;

        friend bool operator==(const ShareLimits &lhs, const ShareLimits &rhs) = default;
    };

    inline qlonglong shareLimitsEta(const ShareLimits &shareLimits, const qlonglong ratioEta
            , const qlonglong seedingTimeEta, const qlonglong inactiveSeedingTimeEta)
    {
        const bool matchAll = (shareLimits.mode == ShareLimitsMode::MatchAll);
        qlonglong result = (matchAll ? 0 : MAX_ETA);
        bool hasLimit = false;

        const auto applyEta = [&](const bool enabled, const qlonglong eta)
        {
            if (!enabled)
                return;

            hasLimit = true;
            const qlonglong normalizedEta = std::max<qlonglong>(eta, 0);
            result = (matchAll ? std::max(result, normalizedEta) : std::min(result, normalizedEta));
        };

        applyEta((shareLimits.ratioLimit >= 0), ratioEta);
        applyEta((shareLimits.seedingTimeLimit >= 0), seedingTimeEta);
        applyEta((shareLimits.inactiveSeedingTimeLimit >= 0), inactiveSeedingTimeEta);

        return (hasLimit ? result : MAX_ETA);
    }
}
