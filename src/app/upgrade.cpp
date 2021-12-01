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
#include <QVector>

#include "base/bittorrent/torrentcontentlayout.h"
#include "base/logger.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/settingsstorage.h"
#include "base/utils/fs.h"
#include "base/utils/io.h"
#include "base/utils/string.h"

namespace
{
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
            , Utils::Fs::toNativePath(configPath + QLatin1String("WebUICertificate.crt")));
        migrate(QLatin1String("Preferences/WebUI/HTTPS/Key")
            , QLatin1String("Preferences/WebUI/HTTPS/KeyPath")
            , Utils::Fs::toNativePath(configPath + QLatin1String("WebUIPrivateKey.pem")));
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
}

bool upgrade(const bool /*ask*/)
{
    exportWebUIHttpsFiles();
    upgradeTorrentContentLayout();
    upgradeListenPortSettings();
    upgradeSchedulerDaysSettings();
    upgradeDNSServiceSettings();
    upgradeTrayIconStyleSettings();
    return true;
}

void handleChangedDefaults(const DefaultPreferencesMode mode)
{
    struct DefaultValue
    {
        QString name;
        QVariant legacy;
        QVariant current;
    };

    const QVector<DefaultValue> changedDefaults
    {
        {QLatin1String {"BitTorrent/Session/QueueingSystemEnabled"}, true, false}
    };

    SettingsStorage *settingsStorage {SettingsStorage::instance()};
    for (auto it = changedDefaults.cbegin(); it != changedDefaults.cend(); ++it)
    {
        if (settingsStorage->loadValue<QVariant>(it->name).isNull())
            settingsStorage->storeValue(it->name, (mode == DefaultPreferencesMode::Legacy ? it->legacy : it->current));
    }
}
