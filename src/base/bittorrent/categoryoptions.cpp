/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2021-2025  Vladimir Golovnev <glassez@yandex.ru>
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

#include "categoryoptions.h"

#include <QJsonObject>
#include <QJsonValue>

#include "base/global.h"
#include "base/utils/string.h"

const QString OPTION_SAVEPATH = u"save_path"_s;
const QString OPTION_DOWNLOADPATH = u"download_path"_s;

const QString OPTION_RATIOLIMIT = u"ratio_limit"_s;
const QString OPTION_SEEDINGTIMELIMIT = u"seeding_time_limit"_s;
const QString OPTION_INACTIVESEEDINGTIMELIMIT = u"inactive_seeding_time_limit"_s;
const QString OPTION_SHARELIMITACTION = u"share_limit_action"_s;

BitTorrent::CategoryOptions BitTorrent::CategoryOptions::fromJSON(const QJsonObject &jsonObj)
{
    CategoryOptions options;
    options.savePath = Path(jsonObj.value(OPTION_SAVEPATH).toString());

    const QJsonValue downloadPathValue = jsonObj.value(OPTION_DOWNLOADPATH);
    if (downloadPathValue.isBool())
        options.downloadPath = {downloadPathValue.toBool(), {}};
    else if (downloadPathValue.isString())
        options.downloadPath = {true, Path(downloadPathValue.toString())};

    options.ratioLimit = jsonObj.value(OPTION_RATIOLIMIT).toDouble(DEFAULT_RATIO_LIMIT);
    options.seedingTimeLimit = jsonObj.value(OPTION_SEEDINGTIMELIMIT).toInt(DEFAULT_SEEDING_TIME_LIMIT);
    options.inactiveSeedingTimeLimit = jsonObj.value(OPTION_INACTIVESEEDINGTIMELIMIT).toInt(DEFAULT_SEEDING_TIME_LIMIT);
    options.shareLimitAction = Utils::String::toEnum<ShareLimitAction>(jsonObj.value(OPTION_SHARELIMITACTION).toString(), ShareLimitAction::Default);

    return options;
}

QJsonObject BitTorrent::CategoryOptions::toJSON() const
{
    QJsonValue downloadPathValue = QJsonValue::Null;
    if (downloadPath)
    {
        if (downloadPath->enabled)
            downloadPathValue = downloadPath->path.data();
        else
            downloadPathValue = false;
    }

    return {
        {OPTION_SAVEPATH, savePath.data()},
        {OPTION_DOWNLOADPATH, downloadPathValue},
        {OPTION_RATIOLIMIT, ratioLimit},
        {OPTION_SEEDINGTIMELIMIT, seedingTimeLimit},
        {OPTION_INACTIVESEEDINGTIMELIMIT, inactiveSeedingTimeLimit},
        {OPTION_SHARELIMITACTION, Utils::String::fromEnum(shareLimitAction)}
    };
}
