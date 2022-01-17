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

#include <QMetaEnum>

#include "base/bittorrent/torrentcontentlayout.h"
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
    const int MIGRATION_VERSION = 3;
    const char MIGRATION_VERSION_KEY[] = "Meta/MigrationVersion";

    void exportWebUIHttpsFiles()
    {
        const auto migrate = [](const QString &oldKey, const QString &newKey, const QString &savePath)
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
                LogMsg(errorMsgFormat.arg(savePath, result.error()) , Log::WARNING);
                return;
            }

            settingsStorage->storeValue(newKey, savePath);
            settingsStorage->removeValue(oldKey);

            LogMsg(QObject::tr("Migrated preferences: WebUI https, exported data to file: \"%1\"").arg(savePath)
                , Log::INFO);
        };

        const QString configPath {specialFolderLocation(SpecialFolder::Config)};
        migrate(QLatin1String("Preferences/WebUI/HTTPS/Certificate")
            , QLatin1String("Preferences/WebUI/HTTPS/CertificatePath")
            , Utils::Fs::toNativePath(configPath + QLatin1String("/WebUICertificate.crt")));
        migrate(QLatin1String("Preferences/WebUI/HTTPS/Key")
            , QLatin1String("Preferences/WebUI/HTTPS/KeyPath")
            , Utils::Fs::toNativePath(configPath + QLatin1String("/WebUIPrivateKey.pem")));
    }

    void upgradeTorrentContentLayout()
    {
        const QString oldKey {QLatin1String {"BitTorrent/Session/CreateTorrentSubfolder"}};
        const QString newKey {QLatin1String {"BitTorrent/Session/TorrentContentLayout"}};

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
        const auto oldKey = QString::fromLatin1("BitTorrent/Session/UseRandomPort");
        const auto newKey = QString::fromLatin1("Preferences/Connection/PortRangeMin");
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
        const auto key = QString::fromLatin1("Preferences/Scheduler/days");
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
        const auto key = QString::fromLatin1("Preferences/DynDNS/Service");
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
        const auto key = QString::fromLatin1("Preferences/Advanced/TrayIconStyle");
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
            {"AddNewTorrentDialog/Enabled", "Preferences/Downloads/NewAdditionDialog"},
            {"AddNewTorrentDialog/Expanded", "AddNewTorrentDialog/expanded"},
            {"AddNewTorrentDialog/Position", "AddNewTorrentDialog/y"},
            {"AddNewTorrentDialog/SavePathHistory", "TorrentAdditionDlg/save_path_history"},
            {"AddNewTorrentDialog/TopLevel", "Preferences/Downloads/NewAdditionDialogFront"},
            {"AddNewTorrentDialog/TreeHeaderState", "AddNewTorrentDialog/qt5/treeHeaderState"},
            {"AddNewTorrentDialog/Width", "AddNewTorrentDialog/width"},
            {"BitTorrent/Session/AddExtensionToIncompleteFiles", "Preferences/Downloads/UseIncompleteExtension"},
            {"BitTorrent/Session/AdditionalTrackers", "Preferences/Bittorrent/TrackersList"},
            {"BitTorrent/Session/AddTorrentPaused", "Preferences/Downloads/StartInPause"},
            {"BitTorrent/Session/AddTrackersEnabled", "Preferences/Bittorrent/AddTrackers"},
            {"BitTorrent/Session/AlternativeGlobalDLSpeedLimit", "Preferences/Connection/GlobalDLLimitAlt"},
            {"BitTorrent/Session/AlternativeGlobalUPSpeedLimit", "Preferences/Connection/GlobalUPLimitAlt"},
            {"BitTorrent/Session/AnnounceIP", "Preferences/Connection/InetAddress"},
            {"BitTorrent/Session/AnnounceToAllTrackers", "Preferences/Advanced/AnnounceToAllTrackers"},
            {"BitTorrent/Session/AnonymousModeEnabled", "Preferences/Advanced/AnonymousMode"},
            {"BitTorrent/Session/BandwidthSchedulerEnabled", "Preferences/Scheduler/Enabled"},
            {"BitTorrent/Session/DefaultSavePath", "Preferences/Downloads/SavePath"},
            {"BitTorrent/Session/DHTEnabled", "Preferences/Bittorrent/DHT"},
            {"BitTorrent/Session/DiskCacheSize", "Preferences/Downloads/DiskWriteCacheSize"},
            {"BitTorrent/Session/DiskCacheTTL", "Preferences/Downloads/DiskWriteCacheTTL"},
            {"BitTorrent/Session/Encryption", "Preferences/Bittorrent/Encryption"},
            {"BitTorrent/Session/FinishedTorrentExportDirectory", "Preferences/Downloads/FinishedTorrentExportDir"},
            {"BitTorrent/Session/ForceProxy", "Preferences/Connection/ProxyForce"},
            {"BitTorrent/Session/GlobalDLSpeedLimit", "Preferences/Connection/GlobalDLLimit"},
            {"BitTorrent/Session/GlobalMaxRatio", "Preferences/Bittorrent/MaxRatio"},
            {"BitTorrent/Session/GlobalUPSpeedLimit", "Preferences/Connection/GlobalUPLimit"},
            {"BitTorrent/Session/IgnoreLimitsOnLAN", "Preferences/Advanced/IgnoreLimitsLAN"},
            {"BitTorrent/Session/IgnoreSlowTorrentsForQueueing", "Preferences/Queueing/IgnoreSlowTorrents"},
            {"BitTorrent/Session/IncludeOverheadInLimits", "Preferences/Advanced/IncludeOverhead"},
            {"BitTorrent/Session/Interface", "Preferences/Connection/Interface"},
            {"BitTorrent/Session/InterfaceAddress", "Preferences/Connection/InterfaceAddress"},
            {"BitTorrent/Session/InterfaceName", "Preferences/Connection/InterfaceName"},
            {"BitTorrent/Session/IPFilter", "Preferences/IPFilter/File"},
            {"BitTorrent/Session/IPFilteringEnabled", "Preferences/IPFilter/Enabled"},
            {"BitTorrent/Session/LSDEnabled", "Preferences/Bittorrent/LSD"},
            {"BitTorrent/Session/MaxActiveDownloads", "Preferences/Queueing/MaxActiveDownloads"},
            {"BitTorrent/Session/MaxActiveTorrents", "Preferences/Queueing/MaxActiveTorrents"},
            {"BitTorrent/Session/MaxActiveUploads", "Preferences/Queueing/MaxActiveUploads"},
            {"BitTorrent/Session/MaxConnections", "Preferences/Bittorrent/MaxConnecs"},
            {"BitTorrent/Session/MaxConnectionsPerTorrent", "Preferences/Bittorrent/MaxConnecsPerTorrent"},
            {"BitTorrent/Session/MaxHalfOpenConnections", "Preferences/Connection/MaxHalfOpenConnec"},
            {"BitTorrent/Session/MaxRatioAction", "Preferences/Bittorrent/MaxRatioAction"},
            {"BitTorrent/Session/MaxUploads", "Preferences/Bittorrent/MaxUploads"},
            {"BitTorrent/Session/MaxUploadsPerTorrent", "Preferences/Bittorrent/MaxUploadsPerTorrent"},
            {"BitTorrent/Session/OutgoingPortsMax", "Preferences/Advanced/OutgoingPortsMax"},
            {"BitTorrent/Session/OutgoingPortsMin", "Preferences/Advanced/OutgoingPortsMin"},
            {"BitTorrent/Session/PeXEnabled", "Preferences/Bittorrent/PeX"},
            {"BitTorrent/Session/Port", "Preferences/Connection/PortRangeMin"},
            {"BitTorrent/Session/Preallocation", "Preferences/Downloads/PreAllocation"},
            {"BitTorrent/Session/ProxyPeerConnections", "Preferences/Connection/ProxyPeerConnections"},
            {"BitTorrent/Session/QueueingSystemEnabled", "Preferences/Queueing/QueueingEnabled"},
            {"BitTorrent/Session/RefreshInterval", "Preferences/General/RefreshInterval"},
            {"BitTorrent/Session/SaveResumeDataInterval", "Preferences/Downloads/SaveResumeDataInterval"},
            {"BitTorrent/Session/SuperSeedingEnabled", "Preferences/Advanced/SuperSeeding"},
            {"BitTorrent/Session/TempPath", "Preferences/Downloads/TempPath"},
            {"BitTorrent/Session/TempPathEnabled", "Preferences/Downloads/TempPathEnabled"},
            {"BitTorrent/Session/TorrentExportDirectory", "Preferences/Downloads/TorrentExportDir"},
            {"BitTorrent/Session/TrackerFilteringEnabled", "Preferences/IPFilter/FilterTracker"},
            {"BitTorrent/Session/UseAlternativeGlobalSpeedLimit", "Preferences/Connection/alt_speeds_on"},
            {"BitTorrent/Session/UseOSCache", "Preferences/Advanced/osCache"},
            {"BitTorrent/Session/UseRandomPort", "Preferences/General/UseRandomPort"},
            {"BitTorrent/Session/uTPEnabled", "Preferences/Bittorrent/uTP"},
            {"BitTorrent/Session/uTPRateLimited", "Preferences/Bittorrent/uTP_rate_limited"},
            {"BitTorrent/TrackerEnabled", "Preferences/Advanced/trackerEnabled"},
            {"Network/PortForwardingEnabled", "Preferences/Connection/UPnP"},
            {"Network/Proxy/Authentication", "Preferences/Connection/Proxy/Authentication"},
            {"Network/Proxy/IP", "Preferences/Connection/Proxy/IP"},
            {"Network/Proxy/OnlyForTorrents", "Preferences/Connection/ProxyOnlyForTorrents"},
            {"Network/Proxy/Password", "Preferences/Connection/Proxy/Password"},
            {"Network/Proxy/Port", "Preferences/Connection/Proxy/Port"},
            {"Network/Proxy/Type", "Preferences/Connection/ProxyType"},
            {"Network/Proxy/Username", "Preferences/Connection/Proxy/Username"},
            {"State/BannedIPs", "Preferences/IPFilter/BannedIPs"}
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
        const auto key = QString::fromLatin1("Network/Proxy/Type");
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

        version = MIGRATION_VERSION;
    }

    return true;
}

void setCurrentMigrationVersion()
{
    SettingsStorage::instance()->storeValue(QLatin1String(MIGRATION_VERSION_KEY), MIGRATION_VERSION);
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
        {QLatin1String {"BitTorrent/Session/QueueingSystemEnabled"}, true, false}
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
