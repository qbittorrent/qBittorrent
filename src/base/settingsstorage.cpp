/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2014  sledgehammer999 <hammered999@gmail.com>
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

#include "settingsstorage.h"

#include <memory>
#include <QFile>
#include <QHash>

#include "global.h"
#include "logger.h"
#include "profile.h"
#include "utils/fs.h"

namespace
{
    // Encapsulates serialization of settings in "atomic" way.
    // write() does not leave half-written files,
    // read() has a workaround for a case of power loss during a previous serialization
    class TransactionalSettings
    {
    public:
        explicit TransactionalSettings(const QString &name)
            : m_name(name)
        {
        }

        QVariantHash read() const;
        bool write(const QVariantHash &data) const;

    private:
        // we return actual file names used by QSettings because
        // there is no other way to get that name except
        // actually create a QSettings object.
        // if serialization operation was not successful we return empty string
        QString deserialize(const QString &name, QVariantHash &data) const;
        QString serialize(const QString &name, const QVariantHash &data) const;

        const QString m_name;
    };

    QString mapKey(const QString &key)
    {
        static const QHash<QString, QString> keyMapping =
        {
            {"BitTorrent/Session/MaxRatioAction", "Preferences/Bittorrent/MaxRatioAction"},
            {"BitTorrent/Session/DefaultSavePath", "Preferences/Downloads/SavePath"},
            {"BitTorrent/Session/TempPath", "Preferences/Downloads/TempPath"},
            {"BitTorrent/Session/TempPathEnabled", "Preferences/Downloads/TempPathEnabled"},
            {"BitTorrent/Session/AddTorrentPaused", "Preferences/Downloads/StartInPause"},
            {"BitTorrent/Session/RefreshInterval", "Preferences/General/RefreshInterval"},
            {"BitTorrent/Session/Preallocation", "Preferences/Downloads/PreAllocation"},
            {"BitTorrent/Session/AddExtensionToIncompleteFiles", "Preferences/Downloads/UseIncompleteExtension"},
            {"BitTorrent/Session/TorrentExportDirectory", "Preferences/Downloads/TorrentExportDir"},
            {"BitTorrent/Session/FinishedTorrentExportDirectory", "Preferences/Downloads/FinishedTorrentExportDir"},
            {"BitTorrent/Session/GlobalUPSpeedLimit", "Preferences/Connection/GlobalUPLimit"},
            {"BitTorrent/Session/GlobalDLSpeedLimit", "Preferences/Connection/GlobalDLLimit"},
            {"BitTorrent/Session/AlternativeGlobalUPSpeedLimit", "Preferences/Connection/GlobalUPLimitAlt"},
            {"BitTorrent/Session/AlternativeGlobalDLSpeedLimit", "Preferences/Connection/GlobalDLLimitAlt"},
            {"BitTorrent/Session/UseAlternativeGlobalSpeedLimit", "Preferences/Connection/alt_speeds_on"},
            {"BitTorrent/Session/BandwidthSchedulerEnabled", "Preferences/Scheduler/Enabled"},
            {"BitTorrent/Session/Port", "Preferences/Connection/PortRangeMin"},
            {"BitTorrent/Session/UseRandomPort", "Preferences/General/UseRandomPort"},
            {"BitTorrent/Session/Interface", "Preferences/Connection/Interface"},
            {"BitTorrent/Session/InterfaceName", "Preferences/Connection/InterfaceName"},
            {"BitTorrent/Session/InterfaceAddress", "Preferences/Connection/InterfaceAddress"},
            {"BitTorrent/Session/SaveResumeDataInterval", "Preferences/Downloads/SaveResumeDataInterval"},
            {"BitTorrent/Session/Encryption", "Preferences/Bittorrent/Encryption"},
            {"BitTorrent/Session/ForceProxy", "Preferences/Connection/ProxyForce"},
            {"BitTorrent/Session/ProxyPeerConnections", "Preferences/Connection/ProxyPeerConnections"},
            {"BitTorrent/Session/MaxConnections", "Preferences/Bittorrent/MaxConnecs"},
            {"BitTorrent/Session/MaxUploads", "Preferences/Bittorrent/MaxUploads"},
            {"BitTorrent/Session/MaxConnectionsPerTorrent", "Preferences/Bittorrent/MaxConnecsPerTorrent"},
            {"BitTorrent/Session/MaxUploadsPerTorrent", "Preferences/Bittorrent/MaxUploadsPerTorrent"},
            {"BitTorrent/Session/DHTEnabled", "Preferences/Bittorrent/DHT"},
            {"BitTorrent/Session/LSDEnabled", "Preferences/Bittorrent/LSD"},
            {"BitTorrent/Session/PeXEnabled", "Preferences/Bittorrent/PeX"},
            {"BitTorrent/Session/AddTrackersEnabled", "Preferences/Bittorrent/AddTrackers"},
            {"BitTorrent/Session/AdditionalTrackers", "Preferences/Bittorrent/TrackersList"},
            {"BitTorrent/Session/IPFilteringEnabled", "Preferences/IPFilter/Enabled"},
            {"BitTorrent/Session/TrackerFilteringEnabled", "Preferences/IPFilter/FilterTracker"},
            {"BitTorrent/Session/IPFilter", "Preferences/IPFilter/File"},
            {"BitTorrent/Session/GlobalMaxRatio", "Preferences/Bittorrent/MaxRatio"},
            {"BitTorrent/Session/AnnounceToAllTrackers", "Preferences/Advanced/AnnounceToAllTrackers"},
            {"BitTorrent/Session/DiskCacheSize", "Preferences/Downloads/DiskWriteCacheSize"},
            {"BitTorrent/Session/DiskCacheTTL", "Preferences/Downloads/DiskWriteCacheTTL"},
            {"BitTorrent/Session/UseOSCache", "Preferences/Advanced/osCache"},
            {"BitTorrent/Session/AnonymousModeEnabled", "Preferences/Advanced/AnonymousMode"},
            {"BitTorrent/Session/QueueingSystemEnabled", "Preferences/Queueing/QueueingEnabled"},
            {"BitTorrent/Session/MaxActiveDownloads", "Preferences/Queueing/MaxActiveDownloads"},
            {"BitTorrent/Session/MaxActiveUploads", "Preferences/Queueing/MaxActiveUploads"},
            {"BitTorrent/Session/MaxActiveTorrents", "Preferences/Queueing/MaxActiveTorrents"},
            {"BitTorrent/Session/IgnoreSlowTorrentsForQueueing", "Preferences/Queueing/IgnoreSlowTorrents"},
            {"BitTorrent/Session/OutgoingPortsMin", "Preferences/Advanced/OutgoingPortsMin"},
            {"BitTorrent/Session/OutgoingPortsMax", "Preferences/Advanced/OutgoingPortsMax"},
            {"BitTorrent/Session/IgnoreLimitsOnLAN", "Preferences/Advanced/IgnoreLimitsLAN"},
            {"BitTorrent/Session/IncludeOverheadInLimits", "Preferences/Advanced/IncludeOverhead"},
            {"BitTorrent/Session/AnnounceIP", "Preferences/Connection/InetAddress"},
            {"BitTorrent/Session/SuperSeedingEnabled", "Preferences/Advanced/SuperSeeding"},
            {"BitTorrent/Session/MaxHalfOpenConnections", "Preferences/Connection/MaxHalfOpenConnec"},
            {"BitTorrent/Session/uTPEnabled", "Preferences/Bittorrent/uTP"},
            {"BitTorrent/Session/uTPRateLimited", "Preferences/Bittorrent/uTP_rate_limited"},
            {"BitTorrent/TrackerEnabled", "Preferences/Advanced/trackerEnabled"},
            {"Network/Proxy/OnlyForTorrents", "Preferences/Connection/ProxyOnlyForTorrents"},
            {"Network/Proxy/Type", "Preferences/Connection/ProxyType"},
            {"Network/Proxy/Authentication", "Preferences/Connection/Proxy/Authentication"},
            {"Network/Proxy/Username", "Preferences/Connection/Proxy/Username"},
            {"Network/Proxy/Password", "Preferences/Connection/Proxy/Password"},
            {"Network/Proxy/IP", "Preferences/Connection/Proxy/IP"},
            {"Network/Proxy/Port", "Preferences/Connection/Proxy/Port"},
            {"Network/PortForwardingEnabled", "Preferences/Connection/UPnP"},
            {"AddNewTorrentDialog/TreeHeaderState", "AddNewTorrentDialog/qt5/treeHeaderState"},
            {"AddNewTorrentDialog/Width", "AddNewTorrentDialog/width"},
            {"AddNewTorrentDialog/Position", "AddNewTorrentDialog/y"},
            {"AddNewTorrentDialog/Expanded", "AddNewTorrentDialog/expanded"},
            {"AddNewTorrentDialog/SavePathHistory", "TorrentAdditionDlg/save_path_history"},
            {"AddNewTorrentDialog/Enabled", "Preferences/Downloads/NewAdditionDialog"},
            {"AddNewTorrentDialog/TopLevel", "Preferences/Downloads/NewAdditionDialogFront"},

            {"State/BannedIPs", "Preferences/IPFilter/BannedIPs"}
        };

        return keyMapping.value(key, key);
    }
}

SettingsStorage *SettingsStorage::m_instance = nullptr;

SettingsStorage::SettingsStorage()
    : m_data {TransactionalSettings(QLatin1String("qBittorrent")).read()}
{
    m_timer.setSingleShot(true);
    m_timer.setInterval(5 * 1000);
    connect(&m_timer, &QTimer::timeout, this, &SettingsStorage::save);
}

SettingsStorage::~SettingsStorage()
{
    save();
}

void SettingsStorage::initInstance()
{
    if (!m_instance)
        m_instance = new SettingsStorage;
}

void SettingsStorage::freeInstance()
{
    delete m_instance;
    m_instance = nullptr;
}

SettingsStorage *SettingsStorage::instance()
{
    return m_instance;
}

bool SettingsStorage::save()
{
    if (!m_dirty) return true; // Obtaining the lock is expensive, let's check early
    const QWriteLocker locker(&m_lock);  // to guard for `m_dirty`
    if (!m_dirty) return true; // something might have changed while we were getting the lock

    const TransactionalSettings settings(QLatin1String("qBittorrent"));
    if (!settings.write(m_data))
    {
        m_timer.start();
        return false;
    }

    m_dirty = false;
    return true;
}

QVariant SettingsStorage::loadValueImpl(const QString &key, const QVariant &defaultValue) const
{
    const QString realKey = mapKey(key);
    const QReadLocker locker(&m_lock);
    return m_data.value(realKey, defaultValue);
}

void SettingsStorage::storeValueImpl(const QString &key, const QVariant &value)
{
    const QString realKey = mapKey(key);
    const QWriteLocker locker(&m_lock);

    QVariant &currentValue = m_data[realKey];
    if (currentValue != value)
    {
        m_dirty = true;
        currentValue = value;
        m_timer.start();
    }
}

void SettingsStorage::removeValue(const QString &key)
{
    const QString realKey = mapKey(key);
    const QWriteLocker locker(&m_lock);
    if (m_data.remove(realKey) > 0)
    {
        m_dirty = true;
        m_timer.start();
    }
}

QVariantHash TransactionalSettings::read() const
{
    QVariantHash res;

    const QString newPath = deserialize(m_name + QLatin1String("_new"), res);
    if (!newPath.isEmpty())
    { // "_new" file is NOT empty
        // This means that the PC closed either due to power outage
        // or because the disk was full. In any case the settings weren't transferred
        // in their final position. So assume that qbittorrent_new.ini/qbittorrent_new.conf
        // contains the most recent settings.
        Logger::instance()->addMessage(QObject::tr("Detected unclean program exit. Using fallback file to restore settings: %1")
                .arg(Utils::Fs::toNativePath(newPath))
            , Log::WARNING);

        QString finalPath = newPath;
        int index = finalPath.lastIndexOf("_new", -1, Qt::CaseInsensitive);
        finalPath.remove(index, 4);

        Utils::Fs::forceRemove(finalPath);
        QFile::rename(newPath, finalPath);
    }
    else
    {
        deserialize(m_name, res);
    }

    return res;
}

bool TransactionalSettings::write(const QVariantHash &data) const
{
    // QSettings deletes the file before writing it out. This can result in problems
    // if the disk is full or a power outage occurs. Those events might occur
    // between deleting the file and recreating it. This is a safety measure.
    // Write everything to qBittorrent_new.ini/qBittorrent_new.conf and if it succeeds
    // replace qBittorrent.ini/qBittorrent.conf with it.
    const QString newPath = serialize(m_name + QLatin1String("_new"), data);
    if (newPath.isEmpty())
    {
        Utils::Fs::forceRemove(newPath);
        return false;
    }

    QString finalPath = newPath;
    int index = finalPath.lastIndexOf("_new", -1, Qt::CaseInsensitive);
    finalPath.remove(index, 4);

    Utils::Fs::forceRemove(finalPath);
    return QFile::rename(newPath, finalPath);
}

QString TransactionalSettings::deserialize(const QString &name, QVariantHash &data) const
{
    SettingsPtr settings = Profile::instance()->applicationSettings(name);

    if (settings->allKeys().isEmpty())
        return {};

    // Copy everything into memory. This means even keys inserted in the file manually
    // or that we don't touch directly in this code (eg disabled by ifdef). This ensures
    // that they will be copied over when save our settings to disk.
    for (const QString &key : asConst(settings->allKeys()))
    {
        const QVariant value = settings->value(key);
        if (value.isValid())
            data[key] = value;
    }

    return settings->fileName();
}

QString TransactionalSettings::serialize(const QString &name, const QVariantHash &data) const
{
    SettingsPtr settings = Profile::instance()->applicationSettings(name);
    for (auto i = data.begin(); i != data.end(); ++i)
        settings->setValue(i.key(), i.value());

    settings->sync(); // Important to get error status

    switch (settings->status())
    {
    case QSettings::NoError:
        return settings->fileName();
    case QSettings::AccessError:
        Logger::instance()->addMessage(QObject::tr("An access error occurred while trying to write the configuration file."), Log::CRITICAL);
        break;
    case QSettings::FormatError:
        Logger::instance()->addMessage(QObject::tr("A format error occurred while trying to write the configuration file."), Log::CRITICAL);
        break;
    default:
        Logger::instance()->addMessage(QObject::tr("An unknown error occurred while trying to write the configuration file."), Log::CRITICAL);
        break;
    }
    return {};
}
