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
#include "path.h"
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
        Path deserialize(const QString &name, QVariantHash &data) const;
        Path serialize(const QString &name, const QVariantHash &data) const;

        const QString m_name;
    };
}

SettingsStorage *SettingsStorage::m_instance = nullptr;

SettingsStorage::SettingsStorage()
    : m_data {TransactionalSettings(u"qBittorrent"_qs).read()}
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
    const QWriteLocker locker(&m_lock);  // guard for `m_dirty` too
    if (!m_dirty) return true;

    const TransactionalSettings settings(u"qBittorrent"_qs);
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
    const QReadLocker locker(&m_lock);
    return m_data.value(key, defaultValue);
}

void SettingsStorage::storeValueImpl(const QString &key, const QVariant &value)
{
    const QWriteLocker locker(&m_lock);
    QVariant &currentValue = m_data[key];
    if (currentValue != value)
    {
        m_dirty = true;
        currentValue = value;
        m_timer.start();
    }
}

void SettingsStorage::removeValue(const QString &key)
{
    const QWriteLocker locker(&m_lock);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    if (m_data.remove(key))
#else
    if (m_data.remove(key) > 0)
#endif
    {
        m_dirty = true;
        m_timer.start();
    }
}

bool SettingsStorage::hasKey(const QString &key) const
{
    const QReadLocker locker {&m_lock};
    return m_data.contains(key);
}

QVariantHash TransactionalSettings::read() const
{
    QVariantHash res;

    const Path newPath = deserialize(m_name + u"_new", res);
    if (!newPath.isEmpty())
    { // "_new" file is NOT empty
        // This means that the PC closed either due to power outage
        // or because the disk was full. In any case the settings weren't transferred
        // in their final position. So assume that qbittorrent_new.ini/qbittorrent_new.conf
        // contains the most recent settings.
        Logger::instance()->addMessage(QObject::tr("Detected unclean program exit. Using fallback file to restore settings: %1")
                .arg(newPath.toString())
            , Log::WARNING);

        QString finalPathStr = newPath.data();
        const int index = finalPathStr.lastIndexOf(u"_new", -1, Qt::CaseInsensitive);
        finalPathStr.remove(index, 4);

        const Path finalPath {finalPathStr};
        Utils::Fs::removeFile(finalPath);
        Utils::Fs::renameFile(newPath, finalPath);
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
    const Path newPath = serialize(m_name + u"_new", data);
    if (newPath.isEmpty())
    {
        Utils::Fs::removeFile(newPath);
        return false;
    }

    QString finalPathStr = newPath.data();
    const int index = finalPathStr.lastIndexOf(u"_new", -1, Qt::CaseInsensitive);
    finalPathStr.remove(index, 4);

    const Path finalPath {finalPathStr};
    Utils::Fs::removeFile(finalPath);
    return Utils::Fs::renameFile(newPath, finalPath);
}

Path TransactionalSettings::deserialize(const QString &name, QVariantHash &data) const
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

    return Path(settings->fileName());
}

Path TransactionalSettings::serialize(const QString &name, const QVariantHash &data) const
{
    SettingsPtr settings = Profile::instance()->applicationSettings(name);
    for (auto i = data.begin(); i != data.end(); ++i)
        settings->setValue(i.key(), i.value());

    settings->sync(); // Important to get error status

    switch (settings->status())
    {
    case QSettings::NoError:
        return Path(settings->fileName());
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
