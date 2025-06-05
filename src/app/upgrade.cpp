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

#include <QtSystemDetection>
#include <QCoreApplication>
#include <QMetaEnum>

#include "base/bittorrent/sharelimitaction.h"
#include "base/bittorrent/torrentcontentlayout.h"
#include "base/global.h"
#include "base/logger.h"
#include "base/net/proxyconfigurationmanager.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/settingsstorage.h"
#include "base/settingvalue.h"
#include "base/utils/io.h"
#include "base/utils/string.h"

namespace
{
    const int MIGRATION_VERSION = 8;
    const QString MIGRATION_VERSION_KEY = u"Meta/MigrationVersion"_s;

    void exportWebUIHttpsFiles()
    {
        const auto migrate = [](const QString &oldKey, const QString &newKey, const Path &savePath)
        {
            SettingsStorage *settingsStorage {SettingsStorage::instance()};
            const auto oldData {settingsStorage->loadValue<QByteArray>(oldKey)};
            const auto newData {settingsStorage->loadValue<QString>(newKey)};
            const QString errorMsgFormat {QCoreApplication::translate("Upgrade", "Migrate preferences failed: WebUI https, file: \"%1\", error: \"%2\"")};

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

            LogMsg(QCoreApplication::translate("Upgrade", "Migrated preferences: WebUI https, exported data to file: \"%1\"").arg(savePath.toString())
                , Log::INFO);
        };

        const Path configPath = specialFolderLocation(SpecialFolder::Config);
        migrate(u"Preferences/WebUI/HTTPS/Certificate"_s
            , u"Preferences/WebUI/HTTPS/CertificatePath"_s
            , (configPath / Path(u"WebUICertificate.crt"_s)));
        migrate(u"Preferences/WebUI/HTTPS/Key"_s
            , u"Preferences/WebUI/HTTPS/KeyPath"_s
            , (configPath / Path(u"WebUIPrivateKey.pem"_s)));
    }

    void upgradeTorrentContentLayout()
    {
        const QString oldKey = u"BitTorrent/Session/CreateTorrentSubfolder"_s;
        const QString newKey = u"BitTorrent/Session/TorrentContentLayout"_s;

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
        const auto oldKey = u"BitTorrent/Session/UseRandomPort"_s;
        const auto newKey = u"Preferences/Connection/PortRangeMin"_s;
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
        const auto key = u"Preferences/Scheduler/days"_s;
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
                LogMsg(QCoreApplication::translate("Upgrade", "Invalid value found in configuration file, reverting it to default. Key: \"%1\". Invalid value: \"%2\".")
                    .arg(key, QString::number(number)), Log::WARNING);
                settingsStorage->removeValue(key);
                break;
            }
        }
    }

    void upgradeDNSServiceSettings()
    {
        auto *settingsStorage = SettingsStorage::instance();
        const auto key = u"Preferences/DynDNS/Service"_s;
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
                LogMsg(QCoreApplication::translate("Upgrade", "Invalid value found in configuration file, reverting it to default. Key: \"%1\". Invalid value: \"%2\".")
                    .arg(key, QString::number(number)), Log::WARNING);
                settingsStorage->removeValue(key);
                break;
            }
        }
    }

    void upgradeTrayIconStyleSettings()
    {
        auto *settingsStorage = SettingsStorage::instance();
        const auto key = u"Preferences/Advanced/TrayIconStyle"_s;
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
                LogMsg(QCoreApplication::translate("Upgrade", "Invalid value found in configuration file, reverting it to default. Key: \"%1\". Invalid value: \"%2\".")
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
            {u"AddNewTorrentDialog/Enabled"_s, u"Preferences/Downloads/NewAdditionDialog"_s},
            {u"AddNewTorrentDialog/Expanded"_s, u"AddNewTorrentDialog/expanded"_s},
            {u"AddNewTorrentDialog/Position"_s, u"AddNewTorrentDialog/y"_s},
            {u"AddNewTorrentDialog/SavePathHistory"_s, u"TorrentAdditionDlg/save_path_history"_s},
            {u"AddNewTorrentDialog/TopLevel"_s, u"Preferences/Downloads/NewAdditionDialogFront"_s},
            {u"AddNewTorrentDialog/TreeHeaderState"_s, u"AddNewTorrentDialog/qt5/treeHeaderState"_s},
            {u"AddNewTorrentDialog/Width"_s, u"AddNewTorrentDialog/width"_s},
            {u"BitTorrent/Session/AddExtensionToIncompleteFiles"_s, u"Preferences/Downloads/UseIncompleteExtension"_s},
            {u"BitTorrent/Session/AdditionalTrackers"_s, u"Preferences/Bittorrent/TrackersList"_s},
            {u"BitTorrent/Session/AddTorrentPaused"_s, u"Preferences/Downloads/StartInPause"_s},
            {u"BitTorrent/Session/AddTrackersEnabled"_s, u"Preferences/Bittorrent/AddTrackers"_s},
            {u"BitTorrent/Session/AlternativeGlobalDLSpeedLimit"_s, u"Preferences/Connection/GlobalDLLimitAlt"_s},
            {u"BitTorrent/Session/AlternativeGlobalUPSpeedLimit"_s, u"Preferences/Connection/GlobalUPLimitAlt"_s},
            {u"BitTorrent/Session/AnnounceIP"_s, u"Preferences/Connection/InetAddress"_s},
            {u"BitTorrent/Session/AnnounceToAllTrackers"_s, u"Preferences/Advanced/AnnounceToAllTrackers"_s},
            {u"BitTorrent/Session/AnonymousModeEnabled"_s, u"Preferences/Advanced/AnonymousMode"_s},
            {u"BitTorrent/Session/BandwidthSchedulerEnabled"_s, u"Preferences/Scheduler/Enabled"_s},
            {u"BitTorrent/Session/DefaultSavePath"_s, u"Preferences/Downloads/SavePath"_s},
            {u"BitTorrent/Session/DHTEnabled"_s, u"Preferences/Bittorrent/DHT"_s},
            {u"BitTorrent/Session/DiskCacheSize"_s, u"Preferences/Downloads/DiskWriteCacheSize"_s},
            {u"BitTorrent/Session/DiskCacheTTL"_s, u"Preferences/Downloads/DiskWriteCacheTTL"_s},
            {u"BitTorrent/Session/Encryption"_s, u"Preferences/Bittorrent/Encryption"_s},
            {u"BitTorrent/Session/FinishedTorrentExportDirectory"_s, u"Preferences/Downloads/FinishedTorrentExportDir"_s},
            {u"BitTorrent/Session/ForceProxy"_s, u"Preferences/Connection/ProxyForce"_s},
            {u"BitTorrent/Session/GlobalDLSpeedLimit"_s, u"Preferences/Connection/GlobalDLLimit"_s},
            {u"BitTorrent/Session/GlobalMaxRatio"_s, u"Preferences/Bittorrent/MaxRatio"_s},
            {u"BitTorrent/Session/GlobalUPSpeedLimit"_s, u"Preferences/Connection/GlobalUPLimit"_s},
            {u"BitTorrent/Session/IgnoreLimitsOnLAN"_s, u"Preferences/Advanced/IgnoreLimitsLAN"_s},
            {u"BitTorrent/Session/IgnoreSlowTorrentsForQueueing"_s, u"Preferences/Queueing/IgnoreSlowTorrents"_s},
            {u"BitTorrent/Session/IncludeOverheadInLimits"_s, u"Preferences/Advanced/IncludeOverhead"_s},
            {u"BitTorrent/Session/Interface"_s, u"Preferences/Connection/Interface"_s},
            {u"BitTorrent/Session/InterfaceAddress"_s, u"Preferences/Connection/InterfaceAddress"_s},
            {u"BitTorrent/Session/InterfaceName"_s, u"Preferences/Connection/InterfaceName"_s},
            {u"BitTorrent/Session/IPFilter"_s, u"Preferences/IPFilter/File"_s},
            {u"BitTorrent/Session/IPFilteringEnabled"_s, u"Preferences/IPFilter/Enabled"_s},
            {u"BitTorrent/Session/LSDEnabled"_s, u"Preferences/Bittorrent/LSD"_s},
            {u"BitTorrent/Session/MaxActiveDownloads"_s, u"Preferences/Queueing/MaxActiveDownloads"_s},
            {u"BitTorrent/Session/MaxActiveTorrents"_s, u"Preferences/Queueing/MaxActiveTorrents"_s},
            {u"BitTorrent/Session/MaxActiveUploads"_s, u"Preferences/Queueing/MaxActiveUploads"_s},
            {u"BitTorrent/Session/MaxConnections"_s, u"Preferences/Bittorrent/MaxConnecs"_s},
            {u"BitTorrent/Session/MaxConnectionsPerTorrent"_s, u"Preferences/Bittorrent/MaxConnecsPerTorrent"_s},
            {u"BitTorrent/Session/MaxHalfOpenConnections"_s, u"Preferences/Connection/MaxHalfOpenConnec"_s},
            {u"BitTorrent/Session/MaxRatioAction"_s, u"Preferences/Bittorrent/MaxRatioAction"_s},
            {u"BitTorrent/Session/MaxUploads"_s, u"Preferences/Bittorrent/MaxUploads"_s},
            {u"BitTorrent/Session/MaxUploadsPerTorrent"_s, u"Preferences/Bittorrent/MaxUploadsPerTorrent"_s},
            {u"BitTorrent/Session/OutgoingPortsMax"_s, u"Preferences/Advanced/OutgoingPortsMax"_s},
            {u"BitTorrent/Session/OutgoingPortsMin"_s, u"Preferences/Advanced/OutgoingPortsMin"_s},
            {u"BitTorrent/Session/PeXEnabled"_s, u"Preferences/Bittorrent/PeX"_s},
            {u"BitTorrent/Session/Port"_s, u"Preferences/Connection/PortRangeMin"_s},
            {u"BitTorrent/Session/Preallocation"_s, u"Preferences/Downloads/PreAllocation"_s},
            {u"BitTorrent/Session/ProxyPeerConnections"_s, u"Preferences/Connection/ProxyPeerConnections"_s},
            {u"BitTorrent/Session/QueueingSystemEnabled"_s, u"Preferences/Queueing/QueueingEnabled"_s},
            {u"BitTorrent/Session/RefreshInterval"_s, u"Preferences/General/RefreshInterval"_s},
            {u"BitTorrent/Session/SaveResumeDataInterval"_s, u"Preferences/Downloads/SaveResumeDataInterval"_s},
            {u"BitTorrent/Session/SuperSeedingEnabled"_s, u"Preferences/Advanced/SuperSeeding"_s},
            {u"BitTorrent/Session/TempPath"_s, u"Preferences/Downloads/TempPath"_s},
            {u"BitTorrent/Session/TempPathEnabled"_s, u"Preferences/Downloads/TempPathEnabled"_s},
            {u"BitTorrent/Session/TorrentExportDirectory"_s, u"Preferences/Downloads/TorrentExportDir"_s},
            {u"BitTorrent/Session/TrackerFilteringEnabled"_s, u"Preferences/IPFilter/FilterTracker"_s},
            {u"BitTorrent/Session/UseAlternativeGlobalSpeedLimit"_s, u"Preferences/Connection/alt_speeds_on"_s},
            {u"BitTorrent/Session/UseOSCache"_s, u"Preferences/Advanced/osCache"_s},
            {u"BitTorrent/Session/UseRandomPort"_s, u"Preferences/General/UseRandomPort"_s},
            {u"BitTorrent/Session/uTPEnabled"_s, u"Preferences/Bittorrent/uTP"_s},
            {u"BitTorrent/Session/uTPRateLimited"_s, u"Preferences/Bittorrent/uTP_rate_limited"_s},
            {u"BitTorrent/TrackerEnabled"_s, u"Preferences/Advanced/trackerEnabled"_s},
            {u"Network/PortForwardingEnabled"_s, u"Preferences/Connection/UPnP"_s},
            {u"Network/Proxy/Authentication"_s, u"Preferences/Connection/Proxy/Authentication"_s},
            {u"Network/Proxy/IP"_s, u"Preferences/Connection/Proxy/IP"_s},
            {u"Network/Proxy/OnlyForTorrents"_s, u"Preferences/Connection/ProxyOnlyForTorrents"_s},
            {u"Network/Proxy/Password"_s, u"Preferences/Connection/Proxy/Password"_s},
            {u"Network/Proxy/Port"_s, u"Preferences/Connection/Proxy/Port"_s},
            {u"Network/Proxy/Type"_s, u"Preferences/Connection/ProxyType"_s},
            {u"Network/Proxy/Username"_s, u"Preferences/Connection/Proxy/Username"_s},
            {u"State/BannedIPs"_s, u"Preferences/IPFilter/BannedIPs"_s}
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
        const auto key = u"Network/Proxy/Type"_s;
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
                settingsStorage->storeValue(key, u"HTTP_PW"_s);
                break;
            case 4:
                settingsStorage->storeValue(key, u"SOCKS5_PW"_s);
                break;
            case 5:
                settingsStorage->storeValue(key, Net::ProxyType::SOCKS4);
                break;
            default:
                LogMsg(QCoreApplication::translate("Upgrade", "Invalid value found in configuration file, reverting it to default. Key: \"%1\". Invalid value: \"%2\".")
                        .arg(key, QString::number(number)), Log::WARNING);
                settingsStorage->removeValue(key);
                break;
            }
        }
    }

    void migrateProxySettings()
    {
        auto *settingsStorage = SettingsStorage::instance();
        const auto proxyType = settingsStorage->loadValue<QString>(u"Network/Proxy/Type"_s, u"None"_s);
        const auto onlyForTorrents = settingsStorage->loadValue<bool>(u"Network/Proxy/OnlyForTorrents"_s)
                || (proxyType == u"SOCKS4");

        settingsStorage->storeValue(u"Network/Proxy/Profiles/BitTorrent"_s, true);
        settingsStorage->storeValue(u"Network/Proxy/Profiles/RSS"_s, !onlyForTorrents);
        settingsStorage->storeValue(u"Network/Proxy/Profiles/Misc"_s, !onlyForTorrents);

        if (proxyType == u"HTTP_PW"_s)
        {
            settingsStorage->storeValue(u"Network/Proxy/Type"_s, Net::ProxyType::HTTP);
            settingsStorage->storeValue(u"Network/Proxy/AuthEnabled"_s, true);
        }
        else if (proxyType == u"SOCKS5_PW"_s)
        {
            settingsStorage->storeValue(u"Network/Proxy/Type"_s, Net::ProxyType::SOCKS5);
            settingsStorage->storeValue(u"Network/Proxy/AuthEnabled"_s, true);
        }

        settingsStorage->removeValue(u"Network/Proxy/OnlyForTorrents"_s);

        const auto proxyHostnameLookup = settingsStorage->loadValue<bool>(u"BitTorrent/Session/ProxyHostnameLookup"_s);
        settingsStorage->storeValue(u"Network/Proxy/HostnameLookupEnabled"_s, proxyHostnameLookup);
        settingsStorage->removeValue(u"BitTorrent/Session/ProxyHostnameLookup"_s);
    }

#ifdef Q_OS_WIN
    void migrateMemoryPrioritySettings()
    {
        auto *settingsStorage = SettingsStorage::instance();
        const QString oldKey = u"BitTorrent/OSMemoryPriority"_s;
        const QString newKey = u"Application/ProcessMemoryPriority"_s;

        if (settingsStorage->hasKey(oldKey))
        {
            const auto value = settingsStorage->loadValue<QVariant>(oldKey);
            settingsStorage->storeValue(newKey, value);
        }
    }
#endif

    void migrateStartupWindowState()
    {
        auto *settingsStorage = SettingsStorage::instance();
        if (settingsStorage->hasKey(u"Preferences/General/StartMinimized"_s))
        {
            const auto startMinimized = settingsStorage->loadValue<bool>(u"Preferences/General/StartMinimized"_s);
            const auto minimizeToTray = settingsStorage->loadValue<bool>(u"Preferences/General/MinimizeToTray"_s);
            const QString windowState = startMinimized ? (minimizeToTray ? u"Hidden"_s : u"Minimized"_s) : u"Normal"_s;
            settingsStorage->storeValue(u"GUI/StartUpWindowState"_s, windowState);
        }
    }

    void migrateChineseLocale()
    {
        auto *settingsStorage = SettingsStorage::instance();
        const auto key = u"Preferences/General/Locale"_s;
        if (settingsStorage->hasKey(key))
        {
            const auto locale = settingsStorage->loadValue<QString>(key);
            if (locale.compare(u"zh"_s, Qt::CaseInsensitive) == 0)
                settingsStorage->storeValue(key, u"zh_CN"_s);
        }
    }

    void migrateShareLimitActionSettings()
    {
        auto *settingsStorage = SettingsStorage::instance();
        const auto oldKey = u"BitTorrent/Session/MaxRatioAction"_s;
        const auto newKey = u"BitTorrent/Session/ShareLimitAction"_s;
        const auto value = settingsStorage->loadValue<int>(oldKey);

        switch (value)
        {
        case 0:
            settingsStorage->storeValue(newKey, BitTorrent::ShareLimitAction::Stop);
            break;
        case 1:
            settingsStorage->storeValue(newKey, BitTorrent::ShareLimitAction::Remove);
            break;
        case 2:
            settingsStorage->storeValue(newKey, BitTorrent::ShareLimitAction::EnableSuperSeeding);
            break;
        case 3:
            settingsStorage->storeValue(newKey, BitTorrent::ShareLimitAction::RemoveWithContent);
            break;
        default:
            LogMsg(QCoreApplication::translate("Upgrade", "Invalid value found in configuration file, reverting it to default. Key: \"%1\". Invalid value: \"%2\".")
                    .arg(oldKey, QString::number(value)), Log::WARNING);
            break;
        }

        settingsStorage->removeValue(oldKey);
    }

    void migrateAddPausedSetting()
    {
        auto *settingsStorage = SettingsStorage::instance();
        const auto oldKey = u"BitTorrent/Session/AddTorrentPaused"_s;
        const auto newKey = u"BitTorrent/Session/AddTorrentStopped"_s;

        settingsStorage->storeValue(newKey, settingsStorage->loadValue<bool>(oldKey));
        settingsStorage->removeValue(oldKey);
    }
}

bool upgrade()
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

        if (version < 5)
        {
            migrateStartupWindowState();
            migrateChineseLocale();
        }

        if (version < 6)
            migrateProxySettings();

        if (version < 7)
            migrateShareLimitActionSettings();

        if (version < 8)
            migrateAddPausedSetting();

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
        {u"BitTorrent/Session/QueueingSystemEnabled"_s, true, false}
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
