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

#include <QFile>
#include <QHash>
#include <QStringList>
#include <QSettings>

#include "logger.h"
#include "utils/fs.h"
#include "settingsstorage.h"

namespace
{
    inline QSettings *createSettings(const QString &name)
    {
#ifdef Q_OS_WIN
        return new QSettings(QSettings::IniFormat, QSettings::UserScope, "qBittorrent", name);
#else
        return new QSettings("qBittorrent", name);
#endif
    }

#ifdef QBT_USES_QT5
    typedef QHash<QString, QString> MappingTable;
#else
    class MappingTable: public QHash<QString, QString>
    {
    public:
        MappingTable(std::initializer_list<std::pair<QString, QString>> list)
        {
            reserve(static_cast<int>(list.size()));
            for (const auto &i : list)
                insert(i.first, i.second);
        }
    };
#endif

    QString mapKey(const QString &key)
    {
        static const MappingTable keyMapping = {

            { "BitTorrent/Session/MaxRatioAction", "Preferences/Bittorrent/MaxRatioAction" },
            { "BitTorrent/Session/DefaultSavePath", "Preferences/Downloads/SavePath" },
            { "BitTorrent/Session/TempPath", "Preferences/Downloads/TempPath" },
            { "BitTorrent/Session/TempPathEnabled", "Preferences/Downloads/TempPathEnabled" },
            { "BitTorrent/Session/AddTorrentPaused", "Preferences/Downloads/StartInPause" },
#ifdef QBT_USES_QT5
            { "AddNewTorrentDialog/TreeHeaderState", "AddNewTorrentDialog/qt5/treeHeaderState" },
#else
            { "AddNewTorrentDialog/TreeHeaderState", "AddNewTorrentDialog/treeHeaderState" },
#endif
            { "AddNewTorrentDialog/Width", "AddNewTorrentDialog/width" },
            { "AddNewTorrentDialog/Position", "AddNewTorrentDialog/y" },
            { "AddNewTorrentDialog/Expanded", "AddNewTorrentDialog/expanded" },
            { "AddNewTorrentDialog/SavePathHistory", "TorrentAdditionDlg/save_path_history" },
            { "AddNewTorrentDialog/Enabled", "Preferences/Downloads/NewAdditionDialog" },
            { "AddNewTorrentDialog/TopLevel", "Preferences/Downloads/NewAdditionDialogFront" }

        };

        return keyMapping.value(key, key);
    }
}

SettingsStorage *SettingsStorage::m_instance = nullptr;

SettingsStorage::SettingsStorage()
    : m_dirty(false)
    , m_lock(QReadWriteLock::Recursive)
{
    QSettings *settings;
#ifdef Q_OS_MAC
    settings = createSettings("qBittorrent");
#else
    settings = createSettings("qBittorrent_new");
    QString newPath = settings->fileName();

    // This means that the PC closed either due to power outage
    // or because the disk was full. In any case the settings weren't transfered
    // in their final position. So assume that qbittorrent_new.ini/qbittorrent_new.conf
    // contains the most recent settings.
    if (!settings->allKeys().isEmpty()) {
        Logger::instance()->addMessage(tr("Detected unclean program exit. Using fallback file to restore settings."), Log::WARNING);
        m_dirty = true;
    }
    else {
        delete settings;
        settings = createSettings("qBittorrent");
    }
#endif

    QStringList keys = settings->allKeys();

    // Copy everything into memory. This means even keys inserted in the file manually
    // or that we don't touch directly in this code(eg disabled by ifdef). This ensures
    // that they will be copied over when save our settings to disk.
    foreach (const QString &key, keys)
        m_data[key] = settings->value(key);

    //Ensures sync to disk before we attempt to manipulate the files from save().
    delete settings;

#ifndef Q_OS_MAC
    Utils::Fs::forceRemove(newPath);

    if (m_dirty)
        save();
#endif

    m_timer.setSingleShot(true);
    m_timer.setInterval(5 * 1000);
    connect(&m_timer, SIGNAL(timeout()), SLOT(save()));
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
    QWriteLocker locker(&m_lock);

    if (!m_dirty) return false;

#ifndef Q_OS_MAC
    // QSettings delete the file before writing it out. This can result in problems
    // if the disk is full or a power outage occurs. Those events might occur
    // between deleting the file and recreating it. This is a safety measure.
    // Write everything to qBittorrent_new.ini/qBittorrent_new.conf and if it succeeds
    // replace qBittorrent_new.ini/qBittorrent.conf with it.
    QSettings *settings = createSettings("qBittorrent_new");
#else
    QSettings *settings = createSettings("qBittorrent");
#endif

    foreach (const QString &key, m_data.keys())
        settings->setValue(key, m_data[key]);

    m_dirty = false;
    locker.unlock();

#ifndef Q_OS_MAC
    settings->sync(); // Important to get error status
    QSettings::Status status = settings->status();
    QString newPath = settings->fileName();

    if (status != QSettings::NoError) {
        if (status == QSettings::AccessError)
            Logger::instance()->addMessage(tr("An access error occurred while trying to write the configuration file."), Log::CRITICAL);
        else
            Logger::instance()->addMessage(tr("A format error occurred while trying to write the configuration file."), Log::CRITICAL);

        delete settings;
        Utils::Fs::forceRemove(newPath);
        return false;
    }

    delete settings;
    QString finalPath = newPath;
    int index = finalPath.lastIndexOf("_new", -1, Qt::CaseInsensitive);
    finalPath.remove(index, 4);
    Utils::Fs::forceRemove(finalPath);
    QFile::rename(newPath, finalPath);
#else
    delete settings;
#endif

    return true;
}

QVariant SettingsStorage::loadValue(const QString &key, const QVariant &defaultValue) const
{
    QReadLocker locker(&m_lock);
    return m_data.value(mapKey(key), defaultValue);
}

void SettingsStorage::storeValue(const QString &key, const QVariant &value)
{
    QString realKey = mapKey(key);
    QWriteLocker locker(&m_lock);
    if (m_data.value(realKey) != value) {
        m_dirty = true;
        m_timer.start();
        m_data.insert(realKey, value);
    }
}

void SettingsStorage::removeValue(const QString &key)
{
    QString realKey = mapKey(key);
    QWriteLocker locker(&m_lock);
    if (m_data.contains(realKey)) {
        m_dirty = true;
        m_timer.start();
        m_data.remove(realKey);
    }
}
