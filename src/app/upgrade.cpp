/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2019  Mike Tzou (Chocobo1)
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

#include "upgrade.h"

#include <QtGlobal>
#include <QMetaEnum>

#include "base/bittorrent/torrentcontentlayout.h"
#include "base/global.h"
#include "base/logger.h"
#include "base/net/proxyconfigurationmanager.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/settingsstorage.h"
#include "base/settingvalue.h"
#include "base/utils/fs.h"
#include "base/utils/io.h"
#include "base/utils/string.h"

namespace
{
    const int MIGRATION_VERSION = 4;
    const QString MIGRATION_VERSION_KEY = u"Meta/MigrationVersion"_qs;

    void exportWebUIHttpsFiles()
    {
        const auto migrate = [](const QString &oldKey, const QString &newKey, const Path &savePath)
        {
            SettingsStorage *settingsStorage {SettingsStorage::instance()};
            const auto oldData {settingsStorage->loadValue<QByteArray>(oldKey)};
            const auto newData {settingsStorage->loadValue<QString>(newKey)};
            const QString errorMsgFormat {QObject::tr("Migrate preferences failed: WebUI https, file: \"%1\", error: \"%2\"")};

            if (!newData.isEmpty() || oldData.isEmpty())
                return;

            const nonstd::expected<void, QString> result = Utils::IO::saveToFile(savePath, oldData);
            if (!result)
            {
                LogMsg(errorMsgFormat.arg(savePath.toString(), result.error()) , Log::WARNING);
                return;
            }

            settingsStorage->storeValue(newKey, savePath);
            settingsStorage->removeValue(oldKey);

            LogMsg(QObject::tr("Migrated preferences: WebUI https, exported data to file: \"%1\"").arg(savePath.toString())
                , Log::INFO);
        };

        const Path configPath = specialFolderLocation(SpecialFolder::Config);
        migrate(u"Preferences/WebUI/HTTPS/Certificate"_qs
            , u"Preferences/WebUI/HTTPS/CertificatePath"_qs
            , (configPath / Path(u"WebUICertificate.crt"_qs)));
        migrate(u"Preferences/WebUI/HTTPS/Key"_qs
            , u"Preferences/WebUI/HTTPS/KeyPath"_qs
            , (configPath / Path(u"WebUIPrivateKey.pem"_qs)));
    }

    void upgradeTorrentContentLayout()
    {
        const QString oldKey = u"BitTorrent/Session/CreateTorrentSubfolder"_qs;
        const QString newKey = u"BitTorrent/Session/TorrentContentLayout"_qs;

        SettingsStorage *settingsStorage {SettingsStorage::instance()};
        const auto oldData {settingsStorage->loadValue<QVariant>(oldKey)};
        const auto newData {settingsStorage->loadValue<QString>(newKey)};

        if (!newData.isEmpty() || !oldData.isValid())
            return;

        const bool createSubfolder = oldData.toBool();
        const BitTorrent::TorrentContentLayout torrentContentLayout =
                (createSubfolder ? BitTorrent::TorrentContentLayout::Original : BitTorrent::TorrentContentLayout::NoSubfolder);

        settingsStorage->storeValue(newKey, Utils::String::fromEnum(torrentContentLayout));
        settingsStorage->removeValue(oldKey);
    }

    void upgradeListenPortSettings()
    {
        const auto oldKey = u"BitTorrent/Session/UseRandomPort"_qs;
        const auto newKey = u"Preferences/Connection/PortRangeMin"_qs;
        auto *settingsStorage = SettingsStorage::instance();

        if (settingsStorage->hasKey(oldKey))
        {
            if (settingsStorage->loadValue<bool>(oldKey))
                settingsStorage->storeValue(newKey, 0);

            settingsStorage->removeValue(oldKey);
        }
    }

    void upgradeSchedulerDaysSettings()
    {
        auto *settingsStorage = SettingsStorage::instance();
        const auto key = u"Preferences/Scheduler/days"_qs;
        const auto value = settingsStorage->loadValue<QString>(key);

        bool ok = false;
        const auto number = value.toInt(&ok);

        if (ok)
        {
            switch (number)
            {
            case 0:
                settingsStorage->storeValue(key, Scheduler::Days::EveryDay);
                break;
            case 1:
                settingsStorage->storeValue(key, Scheduler::Days::Weekday);
                break;
            case 2:
                settingsStorage->storeValue(key, Scheduler::Days::Weekend);
                break;
            case 3:
                settingsStorage->storeValue(key, Scheduler::Days::Monday);
                break;
            case 4:
                settingsStorage->storeValue(key, Scheduler::Days::Tuesday);
                break;
            case 5:
                settingsStorage->storeValue(key, Scheduler::Days::Wednesday);
                break;
            case 6:
                settingsStorage->storeValue(key, Scheduler::Days::Thursday);
                break;
            case 7:
                settingsStorage->storeValue(key, Scheduler::Days::Friday);
                break;
            case 8:
                settingsStorage->storeValue(key, Scheduler::Days::Saturday);
                break;
            case 9:
                settingsStorage->storeValue(key, Scheduler::Days::Sunday);
                break;
            default:
                LogMsg(QObject::tr("Invalid value found in configuration file, reverting it to default. Key: \"%1\". Invalid value: \"%2\".")
                    .arg(key, QString::number(number)), Log::WARNING);
                settingsStorage->removeValue(key);
                break;
            }
        }
    }

    void upgradeDNSServiceSettings()
    {
        auto *settingsStorage = SettingsStorage::instance();
        const auto key = u"Preferences/DynDNS/Service"_qs;
        const auto value = settingsStorage->loadValue<QString>(key);

        bool ok = false;
        const auto number = value.toInt(&ok);

        if (ok)
        {
            switch (number)
            {
            case -1:
                settingsStorage->storeValue(key, DNS::Service::None);
                break;
            case 0:
                settingsStorage->storeValue(key, DNS::Service::DynDNS);
                break;
            case 1:
                settingsStorage->storeValue(key, DNS::Service::NoIP);
                break;
            default:
                LogMsg(QObject::tr("Invalid value found in configuration file, reverting it to default. Key: \"%1\". Invalid value: \"%2\".")
                    .arg(key, QString::number(number)), Log::WARNING);
                settingsStorage->removeValue(key);
                break;
            }
        }
    }

    void upgradeTrayIconStyleSettings()
    {
        auto *settingsStorage = SettingsStorage::instance();
        const auto key = u"Preferences/Advanced/TrayIconStyle"_qs;
        const auto value = settingsStorage->loadValue<QString>(key);

        bool ok = false;
        const auto number = value.toInt(&ok);

        if (ok)
        {
            switch (number)
            {
            case 0:
                settingsStorage->storeValue(key, TrayIcon::Style::Normal);
                break;
            case 1:
                settingsStorage->storeValue(key, TrayIcon::Style::MonoDark);
                break;
            case 2:
                settingsStorage->storeValue(key, TrayIcon::Style::MonoLight);
                break;
            default:
                LogMsg(QObject::tr("Invalid value found in configuration file, reverting it to default. Key: \"%1\". Invalid value: \"%2\".")
                    .arg(key, QString::number(number)), Log::WARNING);
                settingsStorage->removeValue(key);
                break;
            }
        }
    }

    void migrateSettingKeys()
    {
        struct KeyMapping
        {
            QString newKey;
            QString oldKey;
        };

        const KeyMapping mappings[] =
        {
            {u"AddNewTorrentDialog/Enabled"_qs, u"Preferences/Downloads/NewAdditionDialog"_qs},
            {u"AddNewTorrentDialog/Expanded"_qs, u"AddNewTorrentDialog/expanded"_qs},
            {u"AddNewTorrentDialog/Position"_qs, u"AddNewTorrentDialog/y"_qs},
            {u"AddNewTorrentDialog/SavePathHistory"_qs, u"TorrentAdditionDlg/save_path_history"_qs},
            {u"AddNewTorrentDialog/TopLevel"_qs, u"Preferences/Downloads/NewAdditionDialogFront"_qs},
            {u"AddNewTorrentDialog/TreeHeaderState"_qs, u"AddNewTorrentDialog/qt5/treeHeaderState"_qs},
            {u"AddNewTorrentDialog/Width"_qs, u"AddNewTorrentDialog/width"_qs},
            {u"BitTorrent/Session/AddExtensionToIncompleteFiles"_qs, u"Preferences/Downloads/UseIncompleteExtension"_qs},
            {u"BitTorrent/Session/AdditionalTrackers"_qs, u"Preferences/Bittorrent/TrackersList"_qs},
            {u"BitTorrent/Session/AddTorrentPaused"_qs, u"Preferences/Downloads/StartInPause"_qs},
            {u"BitTorrent/Session/AddTrackersEnabled"_qs, u"Preferences/Bittorrent/AddTrackers"_qs},
            {u"BitTorrent/Session/AlternativeGlobalDLSpeedLimit"_qs, u"Preferences/Connection/GlobalDLLimitAlt"_qs},
            {u"BitTorrent/Session/AlternativeGlobalUPSpeedLimit"_qs, u"Preferences/Connection/GlobalUPLimitAlt"_qs},
            {u"BitTorrent/Session/AnnounceIP"_qs, u"Preferences/Connection/InetAddress"_qs},
            {u"BitTorrent/Session/AnnounceToAllTrackers"_qs, u"Preferences/Advanced/AnnounceToAllTrackers"_qs},
            {u"BitTorrent/Session/AnonymousModeEnabled"_qs, u"Preferences/Advanced/AnonymousMode"_qs},
            {u"BitTorrent/Session/BandwidthSchedulerEnabled"_qs, u"Preferences/Scheduler/Enabled"_qs},
            {u"BitTorrent/Session/DefaultSavePath"_qs, u"Preferences/Downloads/SavePath"_qs},
            {u"BitTorrent/Session/DHTEnabled"_qs, u"Preferences/Bittorrent/DHT"_qs},
            {u"BitTorrent/Session/DiskCacheSize"_qs, u"Preferences/Downloads/DiskWriteCacheSize"_qs},
            {u"BitTorrent/Session/DiskCacheTTL"_qs, u"Preferences/Downloads/DiskWriteCacheTTL"_qs},
            {u"BitTorrent/Session/Encryption"_qs, u"Preferences/Bittorrent/Encryption"_qs},
            {u"BitTorrent/Session/FinishedTorrentExportDirectory"_qs, u"Preferences/Downloads/FinishedTorrentExportDir"_qs},
            {u"BitTorrent/Session/ForceProxy"_qs, u"Preferences/Connection/ProxyForce"_qs},
            {u"BitTorrent/Session/GlobalDLSpeedLimit"_qs, u"Preferences/Connection/GlobalDLLimit"_qs},
            {u"BitTorrent/Session/GlobalMaxRatio"_qs, u"Preferences/Bittorrent/MaxRatio"_qs},
            {u"BitTorrent/Session/GlobalUPSpeedLimit"_qs, u"Preferences/Connection/GlobalUPLimit"_qs},
            {u"BitTorrent/Session/IgnoreLimitsOnLAN"_qs, u"Preferences/Advanced/IgnoreLimitsLAN"_qs},
            {u"BitTorrent/Session/IgnoreSlowTorrentsForQueueing"_qs, u"Preferences/Queueing/IgnoreSlowTorrents"_qs},
            {u"BitTorrent/Session/IncludeOverheadInLimits"_qs, u"Preferences/Advanced/IncludeOverhead"_qs},
            {u"BitTorrent/Session/Interface"_qs, u"Preferences/Connection/Interface"_qs},
            {u"BitTorrent/Session/InterfaceAddress"_qs, u"Preferences/Connection/InterfaceAddress"_qs},
            {u"BitTorrent/Session/InterfaceName"_qs, u"Preferences/Connection/InterfaceName"_qs},
            {u"BitTorrent/Session/IPFilter"_qs, u"Preferences/IPFilter/File"_qs},
            {u"BitTorrent/Session/IPFilteringEnabled"_qs, u"Preferences/IPFilter/Enabled"_qs},
            {u"BitTorrent/Session/LSDEnabled"_qs, u"Preferences/Bittorrent/LSD"_qs},
            {u"BitTorrent/Session/MaxActiveDownloads"_qs, u"Preferences/Queueing/MaxActiveDownloads"_qs},
            {u"BitTorrent/Session/MaxActiveTorrents"_qs, u"Preferences/Queueing/MaxActiveTorrents"_qs},
            {u"BitTorrent/Session/MaxActiveUploads"_qs, u"Preferences/Queueing/MaxActiveUploads"_qs},
            {u"BitTorrent/Session/MaxConnections"_qs, u"Preferences/Bittorrent/MaxConnecs"_qs},
            {u"BitTorrent/Session/MaxConnectionsPerTorrent"_qs, u"Preferences/Bittorrent/MaxConnecsPerTorrent"_qs},
            {u"BitTorrent/Session/MaxHalfOpenConnections"_qs, u"Preferences/Connection/MaxHalfOpenConnec"_qs},
            {u"BitTorrent/Session/MaxRatioAction"_qs, u"Preferences/Bittorrent/MaxRatioAction"_qs},
            {u"BitTorrent/Session/MaxUploads"_qs, u"Preferences/Bittorrent/MaxUploads"_qs},
            {u"BitTorrent/Session/MaxUploadsPerTorrent"_qs, u"Preferences/Bittorrent/MaxUploadsPerTorrent"_qs},
            {u"BitTorrent/Session/OutgoingPortsMax"_qs, u"Preferences/Advanced/OutgoingPortsMax"_qs},
            {u"BitTorrent/Session/OutgoingPortsMin"_qs, u"Preferences/Advanced/OutgoingPortsMin"_qs},
            {u"BitTorrent/Session/PeXEnabled"_qs, u"Preferences/Bittorrent/PeX"_qs},
            {u"BitTorrent/Session/Port"_qs, u"Preferences/Connection/PortRangeMin"_qs},
            {u"BitTorrent/Session/Preallocation"_qs, u"Preferences/Downloads/PreAllocation"_qs},
            {u"BitTorrent/Session/ProxyPeerConnections"_qs, u"Preferences/Connection/ProxyPeerConnections"_qs},
            {u"BitTorrent/Session/QueueingSystemEnabled"_qs, u"Preferences/Queueing/QueueingEnabled"_qs},
            {u"BitTorrent/Session/RefreshInterval"_qs, u"Preferences/General/RefreshInterval"_qs},
            {u"BitTorrent/Session/SaveResumeDataInterval"_qs, u"Preferences/Downloads/SaveResumeDataInterval"_qs},
            {u"BitTorrent/Session/SuperSeedingEnabled"_qs, u"Preferences/Advanced/SuperSeeding"_qs},
            {u"BitTorrent/Session/TempPath"_qs, u"Preferences/Downloads/TempPath"_qs},
            {u"BitTorrent/Session/TempPathEnabled"_qs, u"Preferences/Downloads/TempPathEnabled"_qs},
            {u"BitTorrent/Session/TorrentExportDirectory"_qs, u"Preferences/Downloads/TorrentExportDir"_qs},
            {u"BitTorrent/Session/TrackerFilteringEnabled"_qs, u"Preferences/IPFilter/FilterTracker"_qs},
            {u"BitTorrent/Session/UseAlternativeGlobalSpeedLimit"_qs, u"Preferences/Connection/alt_speeds_on"_qs},
            {u"BitTorrent/Session/UseOSCache"_qs, u"Preferences/Advanced/osCache"_qs},
            {u"BitTorrent/Session/UseRandomPort"_qs, u"Preferences/General/UseRandomPort"_qs},
            {u"BitTorrent/Session/uTPEnabled"_qs, u"Preferences/Bittorrent/uTP"_qs},
            {u"BitTorrent/Session/uTPRateLimited"_qs, u"Preferences/Bittorrent/uTP_rate_limited"_qs},
            {u"BitTorrent/TrackerEnabled"_qs, u"Preferences/Advanced/trackerEnabled"_qs},
            {u"Network/PortForwardingEnabled"_qs, u"Preferences/Connection/UPnP"_qs},
            {u"Network/Proxy/Authentication"_qs, u"Preferences/Connection/Proxy/Authentication"_qs},
            {u"Network/Proxy/IP"_qs, u"Preferences/Connection/Proxy/IP"_qs},
            {u"Network/Proxy/OnlyForTorrents"_qs, u"Preferences/Connection/ProxyOnlyForTorrents"_qs},
            {u"Network/Proxy/Password"_qs, u"Preferences/Connection/Proxy/Password"_qs},
            {u"Network/Proxy/Port"_qs, u"Preferences/Connection/Proxy/Port"_qs},
            {u"Network/Proxy/Type"_qs, u"Preferences/Connection/ProxyType"_qs},
            {u"Network/Proxy/Username"_qs, u"Preferences/Connection/Proxy/Username"_qs},
            {u"State/BannedIPs"_qs, u"Preferences/IPFilter/BannedIPs"_qs}
        };

        auto *settingsStorage = SettingsStorage::instance();
        for (const KeyMapping &mapping : mappings)
        {
            if (settingsStorage->hasKey(mapping.oldKey))
            {
                const auto value = settingsStorage->loadValue<QVariant>(mapping.oldKey);
                settingsStorage->storeValue(mapping.newKey, value);
                // TODO: Remove oldKey after ~v4.4.3 and bump migration version
            }
        }
    }

    void migrateProxySettingsEnum()
    {
        auto *settingsStorage = SettingsStorage::instance();
        const auto key = u"Network/Proxy/Type"_qs;
        const auto value = settingsStorage->loadValue<QString>(key);

        bool ok = false;
        const auto number = value.toInt(&ok);

        if (ok)
        {
            switch (number)
            {
            case 0:
                settingsStorage->storeValue(key, Net::ProxyType::None);
                break;
            case 1:
                settingsStorage->storeValue(key, Net::ProxyType::HTTP);
                break;
            case 2:
                settingsStorage->storeValue(key, Net::ProxyType::SOCKS5);
                break;
            case 3:
                settingsStorage->storeValue(key, Net::ProxyType::HTTP_PW);
                break;
            case 4:
                settingsStorage->storeValue(key, Net::ProxyType::SOCKS5_PW);
                break;
            case 5:
                settingsStorage->storeValue(key, Net::ProxyType::SOCKS4);
                break;
            default:
                LogMsg(QObject::tr("Invalid value found in configuration file, reverting it to default. Key: \"%1\". Invalid value: \"%2\".")
                           .arg(key, QString::number(number)), Log::WARNING);
                settingsStorage->removeValue(key);
                break;
            }
        }
    }

#ifdef Q_OS_WIN
    void migrateMemoryPrioritySettings()
    {
        auto *settingsStorage = SettingsStorage::instance();
        const QString oldKey = u"BitTorrent/OSMemoryPriority"_qs;
        const QString newKey = u"Application/ProcessMemoryPriority"_qs;

        if (settingsStorage->hasKey(oldKey))
        {
            const auto value = settingsStorage->loadValue<QVariant>(oldKey);
            settingsStorage->storeValue(newKey, value);
        }
    }
#endif
}

bool upgrade(const bool /*ask*/)
{
    CachedSettingValue<int> version {MIGRATION_VERSION_KEY, 0};

    if (version != MIGRATION_VERSION)
    {
        if (version < 1)
        {
            exportWebUIHttpsFiles();
            upgradeTorrentContentLayout();
            upgradeListenPortSettings();
            upgradeSchedulerDaysSettings();
            upgradeDNSServiceSettings();
            upgradeTrayIconStyleSettings();
        }

        if (version < 2)
            migrateSettingKeys();

        if (version < 3)
            migrateProxySettingsEnum();

#ifdef Q_OS_WIN
        if (version < 4)
            migrateMemoryPrioritySettings();
#endif

        version = MIGRATION_VERSION;
    }

    return true;
}

void setCurrentMigrationVersion()
{
    SettingsStorage::instance()->storeValue(MIGRATION_VERSION_KEY, MIGRATION_VERSION);
}

void handleChangedDefaults(const DefaultPreferencesMode mode)
{
    struct DefaultValue
    {
        QString name;
        QVariant legacy;
        QVariant current;
    };

    const DefaultValue changedDefaults[] =
    {
        {u"BitTorrent/Session/QueueingSystemEnabled"_qs, true, false}
    };

    auto *settingsStorage = SettingsStorage::instance();
    for (const DefaultValue &value : changedDefaults)
    {
        if (!settingsStorage->hasKey(value.name))
        {
            settingsStorage->storeValue(value.name
                , (mode == DefaultPreferencesMode::Legacy ? value.legacy : value.current));
        }
    }
}
