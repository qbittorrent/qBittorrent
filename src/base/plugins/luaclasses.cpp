/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Vladimir Golovnev <glassez@yandex.ru>
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

#include "luaclasses.h"

#include <LuaBridge/LuaBridge.h>
#include <LuaBridge/Vector.h>

#include <QDateTime>
#include <QString>

#include "base/bittorrent/addtorrenterror.h"
#include "base/bittorrent/infohash.h"
#include "base/bittorrent/sharelimits.h"
#include "base/bittorrent/torrent.h"
#include "base/bittorrent/trackerentry.h"
#include "base/bittorrent/trackerentrystatus.h"
#include "luanamespace.h"
#include "luastack.h"

namespace
{
    qint64 toSecondsSinceEpoch(const BitTorrent::AnnounceTimePoint &time)
    {
        const auto now = std::chrono::system_clock::now();
        const auto timepointNow = BitTorrent::AnnounceTimePoint::clock::now();
        const auto timeEpoch = (now + (time - timepointNow)).time_since_epoch();
        return std::chrono::duration_cast<std::chrono::seconds>(timeEpoch).count();
    }

    void registerLuaClassTorrent(lua_State *luaState)
    {
        using Torrent = BitTorrent::Torrent;

        auto qBittorrentNS = luabridge::getGlobalNamespace(luaState).beginNamespace(QBT_NAMESPACE);
        auto cls = qBittorrentNS.beginClass<Torrent>("Torrent");

        cls.addProperty("id", +[](const Torrent *torrent) { return torrent->id().toString(); });
        cls.addProperty("infoHashV1", +[](const Torrent *torrent) { return torrent->infoHash().v1().toString(); });
        cls.addProperty("infoHashV2", +[](const Torrent *torrent) { return torrent->infoHash().v2().toString(); });
        cls.addProperty("hasMetadata", +[](const Torrent *torrent) { return torrent->hasMetadata(); });
        cls.addProperty("name", &Torrent::name, &Torrent::setName);
        cls.addProperty("creationTime", +[](const Torrent *torrent) { return torrent->creationDate().toSecsSinceEpoch(); });
        cls.addProperty("creator", &Torrent::creator);
        cls.addProperty("comment", &Torrent::comment, &Torrent::setComment);

        // metadata
        cls.addProperty("filesCount", +[](const Torrent *torrent) { return torrent->filesCount(); });
        cls.addFunction("filePath", &Torrent::filePath);
        cls.addFunction("fileSize", &Torrent::fileSize);
        cls.addProperty("pieceSize", &Torrent::pieceLength);
        cls.addProperty("piecesCount", &Torrent::piecesCount);
        cls.addProperty("totalSize", &Torrent::totalSize);
        cls.addProperty("private", &Torrent::isPrivate);

        cls.addProperty("savePath", &Torrent::savePath, &Torrent::setSavePath);
        cls.addProperty("downloadPath", &Torrent::downloadPath, &Torrent::setDownloadPath);
        cls.addProperty("category", &Torrent::category, +[](Torrent *torrent, const QString &category)
        {
            torrent->setCategory(category);
        });
        cls.addProperty("tags", +[](const Torrent *torrent) -> std::vector<QString>
        {
            const auto tags = torrent->tags();
            return {tags.cbegin(), tags.cend()};
        });
        cls.addFunction("hasTag", &Torrent::hasTag);
        cls.addFunction("addTag", &Torrent::addTag);
        cls.addFunction("removeTag", &Torrent::removeTag);
        cls.addFunction("clearTags", &Torrent::clearTags);

        cls.addProperty("addedTime", +[](const Torrent *torrent) { return torrent->addedTime().toSecsSinceEpoch(); });
        cls.addProperty("completedTime", +[](const Torrent *torrent) { return torrent->completedTime().toSecsSinceEpoch(); });
        cls.addProperty("lastSeenComplete", +[](const Torrent *torrent) { return torrent->lastSeenComplete().toSecsSinceEpoch(); });
        cls.addProperty("activeTime", &Torrent::activeTime);
        cls.addProperty("finishedTime", &Torrent::finishedTime);
        cls.addProperty("timeSinceActivity", &Torrent::timeSinceActivity);
        cls.addProperty("timeSinceDownload", &Torrent::timeSinceDownload);
        cls.addProperty("timeSinceUpload", &Torrent::timeSinceUpload);

        cls.addProperty("wantedSize", &Torrent::wantedSize);
        cls.addProperty("completedSize", &Torrent::completedSize);
        cls.addProperty("wastedSize", &Torrent::wastedSize);
        cls.addProperty("piecesHave", &Torrent::piecesHave);
        cls.addProperty("progress", &Torrent::progress);

        cls.addProperty("rootPath", &Torrent::rootPath);
        cls.addProperty("contentPath", &Torrent::contentPath);

        cls.addProperty("currentTracker", &Torrent::currentTracker);
        cls.addProperty("trackerStatuses", &Torrent::trackers);
        cls.addProperty("urlSeeds", &Torrent::urlSeeds);

        cls.addProperty("shareLimits", &Torrent::shareLimits, &Torrent::setShareLimits);
        cls.addProperty("effectiveShareLimits", &Torrent::effectiveShareLimits);

        cls.addProperty("hasMissingFiles", &Torrent::hasMissingFiles);
        cls.addProperty("hasError", &Torrent::hasError);
        cls.addProperty("error", &Torrent::error);

        cls.addProperty("queuePosition", &Torrent::queuePosition);

        cls.addFunction("start", &Torrent::start);
        cls.addFunction("stop", &Torrent::stop);
    }

    void registerLuaClassTrackerEntry(lua_State *luaState)
    {
        using TrackerEntry = BitTorrent::TrackerEntry;

        auto qBittorrentNS = luabridge::getGlobalNamespace(luaState).beginNamespace(QBT_NAMESPACE);
        auto cls = qBittorrentNS.beginClass<TrackerEntry>("TrackerEntry");

        cls.addConstructor<void ()>();
        cls.addProperty("url", &TrackerEntry::url);
        cls.addProperty("tier", &TrackerEntry::tier);
    }

    void registerLuaEnumTrackerEndpointState(lua_State *luaState)
    {
        auto qBittorrentNS = luabridge::getGlobalNamespace(luaState).beginNamespace(QBT_NAMESPACE);
        qBittorrentNS.beginNamespace("TrackerEndpointState")
            .addProperty("NotContacted", +[] { return BitTorrent::TrackerEndpointState::NotContacted; })
            .addProperty("Working", +[] { return BitTorrent::TrackerEndpointState::Working; })
            .addProperty("NotWorking", +[] { return BitTorrent::TrackerEndpointState::NotWorking; })
            .addProperty("TrackerError", +[] { return BitTorrent::TrackerEndpointState::TrackerError; })
            .addProperty("Unreachable", +[] { return BitTorrent::TrackerEndpointState::Unreachable; })
            .endNamespace();
    }

    void registerLuaClassTrackerEndpointStatus(lua_State *luaState)
    {
        using TrackerEndpointStatus = BitTorrent::TrackerEndpointStatus;

        auto qBittorrentNS = luabridge::getGlobalNamespace(luaState).beginNamespace(QBT_NAMESPACE);
        auto cls = qBittorrentNS.beginClass<TrackerEndpointStatus>("TrackerEndpointStatus");

        cls.addProperty("name", &TrackerEndpointStatus::name);
        cls.addProperty("btVersion", &TrackerEndpointStatus::btVersion);

        cls.addProperty("updating", &TrackerEndpointStatus::isUpdating);
        cls.addProperty("state", &TrackerEndpointStatus::state);
        cls.addProperty("message", &TrackerEndpointStatus::message);

        cls.addProperty("numSeeds", &TrackerEndpointStatus::numSeeds);
        cls.addProperty("numPeers", &TrackerEndpointStatus::numPeers);
        cls.addProperty("numLeeches", &TrackerEndpointStatus::numLeeches);
        cls.addProperty("numDownloaded", &TrackerEndpointStatus::numDownloaded);

        cls.addProperty("nextAnnounceTime", +[](const BitTorrent::TrackerEndpointStatus &endpoint)
        {
            return toSecondsSinceEpoch(endpoint.nextAnnounceTime);
        });
        cls.addProperty("minAnnounceTime", +[](const BitTorrent::TrackerEndpointStatus &endpoint)
        {
            return toSecondsSinceEpoch(endpoint.minAnnounceTime);
        });
    }

    void registerLuaClassTrackerEntryStatus(lua_State *luaState)
    {
        using TrackerEntryStatus = BitTorrent::TrackerEntryStatus;

        auto qBittorrentNS = luabridge::getGlobalNamespace(luaState).beginNamespace(QBT_NAMESPACE);
        auto cls = qBittorrentNS.beginClass<TrackerEntryStatus>("TrackerEntryStatus");

        cls.addProperty("url", &TrackerEntryStatus::url);
        cls.addProperty("tier", &TrackerEntryStatus::tier);

        cls.addProperty("updating", &TrackerEntryStatus::isUpdating);
        cls.addProperty("state", &TrackerEntryStatus::state);
        cls.addProperty("message", &TrackerEntryStatus::message);

        cls.addProperty("numSeeds", &TrackerEntryStatus::numSeeds);
        cls.addProperty("numPeers", &TrackerEntryStatus::numPeers);
        cls.addProperty("numLeeches", &TrackerEntryStatus::numLeeches);
        cls.addProperty("numDownloaded", &TrackerEntryStatus::numDownloaded);

        cls.addProperty("endpoints", &TrackerEntryStatus::endpoints);
    }

    void registerLuaEnumShareLimitAction(lua_State *luaState)
    {
        auto qBittorrentNS = luabridge::getGlobalNamespace(luaState).beginNamespace(QBT_NAMESPACE);
        qBittorrentNS.beginNamespace("ShareLimitAction")
            .addProperty("Default", +[] { return BitTorrent::ShareLimitAction::Default; })
            .addProperty("Stop", +[] { return BitTorrent::ShareLimitAction::Stop; })
            .addProperty("Remove", +[] { return BitTorrent::ShareLimitAction::Remove; })
            .addProperty("RemoveWithContent", +[] { return BitTorrent::ShareLimitAction::RemoveWithContent; })
            .addProperty("EnableSuperSeeding", +[] { return BitTorrent::ShareLimitAction::EnableSuperSeeding; })
            .endNamespace();
    }

    void registerLuaEnumShareLimitsMode(lua_State *luaState)
    {
        auto qBittorrentNS = luabridge::getGlobalNamespace(luaState).beginNamespace(QBT_NAMESPACE);
        qBittorrentNS.beginNamespace("ShareLimitsMode")
            .addProperty("Default", +[] { return BitTorrent::ShareLimitsMode::Default; })
            .addProperty("MatchAny", +[] { return BitTorrent::ShareLimitsMode::MatchAny; })
            .addProperty("MatchAll", +[] { return BitTorrent::ShareLimitsMode::MatchAll; })
            .endNamespace();
    }

    void registerLuaClassShareLimits(lua_State *luaState)
    {
        using ShareLimits = BitTorrent::ShareLimits;

        auto qBittorrentNS = luabridge::getGlobalNamespace(luaState).beginNamespace(QBT_NAMESPACE);
        auto cls = qBittorrentNS.beginClass<ShareLimits>("ShareLimits");

        cls.addConstructor<void ()>();
        cls.addProperty("ratioLimit", &ShareLimits::ratioLimit);
        cls.addProperty("seedingTimeLimit", &ShareLimits::seedingTimeLimit);
        cls.addProperty("inactiveSeedingTimeLimit", &ShareLimits::inactiveSeedingTimeLimit);
        cls.addProperty("mode", &ShareLimits::mode);
        cls.addProperty("action", &ShareLimits::action);
    }

    void registerLuaEnumAddTorrentErrorKind(lua_State *luaState)
    {
        auto qBittorrentNS = luabridge::getGlobalNamespace(luaState).beginNamespace(QBT_NAMESPACE);
        qBittorrentNS.beginNamespace("AddTorrentErrorKind")
            .addProperty("DuplicateTorrent", +[] { return BitTorrent::AddTorrentError::Kind::DuplicateTorrent; })
            .addProperty("Other", +[] { return BitTorrent::AddTorrentError::Kind::Other; })
            .endNamespace();
    }

    void registerLuaClassAddTorrentError(lua_State *luaState)
    {
        using AddTorrentError = BitTorrent::AddTorrentError;

        auto qBittorrentNS = luabridge::getGlobalNamespace(luaState).beginNamespace(QBT_NAMESPACE);
        auto cls = qBittorrentNS.beginClass<AddTorrentError>("AddTorrentError");

        cls.addProperty("kind", &AddTorrentError::kind);
        cls.addProperty("message", &AddTorrentError::message);
    }
}

void registerLuaClasses(lua_State *luaState)
{
    registerLuaClassTorrent(luaState);

    registerLuaClassTrackerEntry(luaState);

    registerLuaEnumTrackerEndpointState(luaState);
    registerLuaClassTrackerEndpointStatus(luaState);
    registerLuaClassTrackerEntryStatus(luaState);

    registerLuaEnumShareLimitAction(luaState);
    registerLuaEnumShareLimitsMode(luaState);
    registerLuaClassShareLimits(luaState);

    registerLuaEnumAddTorrentErrorKind(luaState);
    registerLuaClassAddTorrentError(luaState);
}
