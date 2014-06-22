/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez
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
 *
 * Contact : chris@qbittorrent.org
 */

#ifndef TORRENTPERSISTENTDATA_H
#define TORRENTPERSISTENTDATA_H

#include <QVariant>
#include <QDateTime>
#include <QDebug>
#include <libtorrent/version.hpp>
#include <libtorrent/magnet_uri.hpp>
#include "qtorrenthandle.h"
#include "misc.h"
#include <vector>
#include "qinisettings.h"
#include <QHash>

class TorrentTempData {
  // This class stores strings w/o modifying separators
public:
  static bool hasTempData(const QString &hash) {
    return data.contains(hash);
  }

  static void deleteTempData(const QString &hash) {
    data.remove(hash);
  }

  static void setFilesPriority(const QString &hash,  const std::vector<int> &pp) {
    data[hash].files_priority = pp;
  }

  static void setFilesPath(const QString &hash, const QStringList &path_list) {
    data[hash].path_list = path_list;
  }

  static void setSavePath(const QString &hash, const QString &save_path) {
    data[hash].save_path = save_path;
  }

  static void setLabel(const QString &hash, const QString &label) {
    data[hash].label = label;
  }

  static void setSequential(const QString &hash, const bool &sequential) {
    data[hash].sequential = sequential;
  }

  static bool isSequential(const QString &hash) {
    return data.value(hash).sequential;
  }

  static void setSeedingMode(const QString &hash, const bool &seed) {
    data[hash].seed = seed;
  }

  static bool isSeedingMode(const QString &hash) {
    return data.value(hash).seed;
  }

  static QString getSavePath(const QString &hash) {
    return data.value(hash).save_path;
  }

  static QStringList getFilesPath(const QString &hash) {
    return data.value(hash).path_list;
  }

  static QString getLabel(const QString &hash) {
    return data.value(hash).label;
  }

  static void getFilesPriority(const QString &hash, std::vector<int> &fp) {
    fp = data.value(hash).files_priority;
  }

private:
  struct TorrentData {
    TorrentData(): sequential(false), seed(false) {}
    std::vector<int> files_priority;
    QStringList path_list;
    QString save_path;
    QString label;
    bool sequential;
    bool seed;
  };

  static QHash<QString, TorrentData> data;
};

class HiddenData {
public:
  static void addData(const QString &hash) {
    data[hash] = false;
  }

  static bool hasData(const QString &hash) {
    return data.contains(hash);
  }

  static void deleteData(const QString &hash) {
    if (data.value(hash, false))
      metadata_counter--;
    data.remove(hash);
  }

  static int getSize() {
    return data.size();
  }

  static int getDownloadingSize() {
    return data.size() - metadata_counter;
  }

  static void gotMetadata(const QString &hash) {
    if (!data.contains(hash))
        return;
    data[hash] = true;
    metadata_counter++;
  }

private:
  static QHash<QString, bool> data;
  static unsigned int metadata_counter;
};

class TorrentPersistentData {
  // This class stores strings w/o modifying separators
public:
  enum RatioLimit {
    USE_GLOBAL_RATIO = -2,
    NO_RATIO_LIMIT = -1
  };

public:
  static bool isKnownTorrent(QString hash) {
    QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent-resume"));
    const QHash<QString, QVariant> all_data = settings.value("torrents").toHash();
    return all_data.contains(hash);
  }

  static QStringList knownTorrents() {
    QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent-resume"));
    const QHash<QString, QVariant> all_data = settings.value("torrents").toHash();
    return all_data.keys();
  }

  static void setRatioLimit(const QString &hash, const qreal &ratio) {
    QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent-resume"));
    QHash<QString, QVariant> all_data = settings.value("torrents").toHash();
    QHash<QString, QVariant> data = all_data.value(hash).toHash();
    data["max_ratio"] = ratio;
    all_data[hash] = data;
    settings.setValue("torrents", all_data);
  }

  static qreal getRatioLimit(const QString &hash) {
    QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent-resume"));
    const QHash<QString, QVariant> all_data = settings.value("torrents").toHash();
    const QHash<QString, QVariant> data = all_data.value(hash).toHash();
    return data.value("max_ratio", USE_GLOBAL_RATIO).toReal();
  }

  static bool hasPerTorrentRatioLimit() {
    QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent-resume"));
    const QHash<QString, QVariant> all_data = settings.value("torrents").toHash();
    QHash<QString, QVariant>::ConstIterator it = all_data.constBegin();
    QHash<QString, QVariant>::ConstIterator itend = all_data.constEnd();
    for ( ; it != itend; ++it) {
      if (it.value().toHash().value("max_ratio", USE_GLOBAL_RATIO).toReal() >= 0) {
        return true;
      }
    }
    return false;
  }

  static void setAddedDate(const QString &hash, const QDateTime &time = QDateTime::currentDateTime()) {
    QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent-resume"));
    QHash<QString, QVariant> all_data = settings.value("torrents").toHash();
    QHash<QString, QVariant> data = all_data.value(hash).toHash();
    if (!data.contains("add_date")) {
      data["add_date"] = time;
      all_data[hash] = data;
      settings.setValue("torrents", all_data);
    }
  }

  static QDateTime getAddedDate(const QString &hash) {
    QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent-resume"));
    const QHash<QString, QVariant> all_data = settings.value("torrents").toHash();
    const QHash<QString, QVariant> data = all_data.value(hash).toHash();
    QDateTime dt = data.value("add_date").toDateTime();
    if (!dt.isValid()) {
      setAddedDate(hash);
      dt = QDateTime::currentDateTime();
    }
    return dt;
  }

  static void setErrorState(const QString &hash, const bool has_error) {
    QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent-resume"));
    QHash<QString, QVariant> all_data = settings.value("torrents").toHash();
    QHash<QString, QVariant> data = all_data.value(hash).toHash();
    data["has_error"] = has_error;
    all_data[hash] = data;
    settings.setValue("torrents", all_data);
  }

  static bool hasError(const QString &hash) {
    QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent-resume"));
    const QHash<QString, QVariant> all_data = settings.value("torrents").toHash();
    const QHash<QString, QVariant> data = all_data.value(hash).toHash();
    return data.value("has_error", false).toBool();
  }

  static void setPreviousSavePath(const QString &hash, const QString &previous_path) {
    QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent-resume"));
    QHash<QString, QVariant> all_data = settings.value("torrents").toHash();
    QHash<QString, QVariant> data = all_data.value(hash).toHash();
    data["previous_path"] = previous_path;
    all_data[hash] = data;
    settings.setValue("torrents", all_data);
  }

  static QString getPreviousPath(const QString &hash) {
    QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent-resume"));
    const QHash<QString, QVariant> all_data = settings.value("torrents").toHash();
    const QHash<QString, QVariant> data = all_data.value(hash).toHash();
    return data.value("previous_path").toString();
  }

  static QDateTime getSeedDate(const QString &hash) {
    QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent-resume"));
    const QHash<QString, QVariant> all_data = settings.value("torrents").toHash();
    const QHash<QString, QVariant> data = all_data.value(hash).toHash();
    return data.value("seed_date").toDateTime();
  }

  static void deletePersistentData(const QString &hash) {
    QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent-resume"));
    QHash<QString, QVariant> all_data = settings.value("torrents").toHash();
    if (all_data.contains(hash)) {
      all_data.remove(hash);
      settings.setValue("torrents", all_data);
    }
  }

  static void saveTorrentPersistentData(const QTorrentHandle &h, const QString &save_path = QString::null, const bool is_magnet = false) {
    Q_ASSERT(h.is_valid());
    qDebug("Saving persistent data for %s", qPrintable(h.hash()));
    // Save persistent data
    QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent-resume"));
    QHash<QString, QVariant> all_data = settings.value("torrents").toHash();
    QHash<QString, QVariant> data = all_data.value(h.hash()).toHash();
    data["is_magnet"] = is_magnet;
    if (is_magnet) {
      data["magnet_uri"] = misc::toQString(make_magnet_uri(h));
    }
    data["seed"] = h.is_seed();
    data["priority"] = h.queue_position();
    if (save_path.isEmpty()) {
      qDebug("TorrentPersistantData: save path is %s", qPrintable(h.save_path()));
      data["save_path"] = h.save_path();
    } else {
      qDebug("TorrentPersistantData: overriding save path is %s", qPrintable(save_path));
      data["save_path"] = save_path; // Override torrent save path (e.g. because it is a temp dir)
    }
    // Label
    data["label"] = TorrentTempData::getLabel(h.hash());
    // Save data
    all_data[h.hash()] = data;
    settings.setValue("torrents", all_data);
    qDebug("TorrentPersistentData: Saving save_path %s, hash: %s", qPrintable(h.save_path()), qPrintable(h.hash()));
    // Set Added date
    setAddedDate(h.hash());
    // Finally, remove temp data
    TorrentTempData::deleteTempData(h.hash());
  }

  // Setters

  static void saveSavePath(const QString &hash, const QString &save_path) {
    Q_ASSERT(!hash.isEmpty());
    qDebug("TorrentPersistentData::saveSavePath(%s)", qPrintable(save_path));
    QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent-resume"));
    QHash<QString, QVariant> all_data = settings.value("torrents").toHash();
    QHash<QString, QVariant> data = all_data.value(hash).toHash();
    data["save_path"] = save_path;
    all_data[hash] = data;
    settings.setValue("torrents", all_data);
    qDebug("TorrentPersistentData: Saving save_path: %s, hash: %s", qPrintable(save_path), qPrintable(hash));
  }

  static void saveLabel(const QString &hash, const QString &label) {
    Q_ASSERT(!hash.isEmpty());
    QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent-resume"));
    QHash<QString, QVariant> all_data = settings.value("torrents").toHash();
    QHash<QString, QVariant> data = all_data.value(hash).toHash();
    data["label"] = label;
    all_data[hash] = data;
    settings.setValue("torrents", all_data);
  }

  static void saveName(const QString &hash, const QString &name) {
    Q_ASSERT(!hash.isEmpty());
    QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent-resume"));
    QHash<QString, QVariant> all_data = settings.value("torrents").toHash();
    QHash<QString, QVariant> data = all_data.value(hash).toHash();
    data["name"] = name;
    all_data[hash] = data;
    settings.setValue("torrents", all_data);
  }

  static void savePriority(const QTorrentHandle &h) {
    QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent-resume"));
    QHash<QString, QVariant> all_data = settings.value("torrents").toHash();
    QHash<QString, QVariant> data = all_data[h.hash()].toHash();
    data["priority"] = h.queue_position();
    all_data[h.hash()] = data;
    settings.setValue("torrents", all_data);
  }

  static void savePriority(const QString &hash, const int &queue_pos) {
    QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent-resume"));
    QHash<QString, QVariant> all_data = settings.value("torrents").toHash();
    QHash<QString, QVariant> data = all_data[hash].toHash();
    data["priority"] = queue_pos;
    all_data[hash] = data;
    settings.setValue("torrents", all_data);
  }

  static void saveSeedStatus(const QTorrentHandle &h) {
    QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent-resume"));
    QHash<QString, QVariant> all_data = settings.value("torrents").toHash();
    QHash<QString, QVariant> data = all_data[h.hash()].toHash();
    bool was_seed = data.value("seed", false).toBool();
    if (was_seed != h.is_seed()) {
      data["seed"] = !was_seed;
      all_data[h.hash()] = data;
      settings.setValue("torrents", all_data);
    }
  }

  static void saveSeedStatus(const QString &hash, const bool seedStatus) {
    QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent-resume"));
    QHash<QString, QVariant> all_data = settings.value("torrents").toHash();
    QHash<QString, QVariant> data = all_data[hash].toHash();
    data["seed"] = seedStatus;
    all_data[hash] = data;
    settings.setValue("torrents", all_data);
  }

  // Getters
  static QString getSavePath(const QString &hash) {
    QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent-resume"));
    const QHash<QString, QVariant> all_data = settings.value("torrents").toHash();
    const QHash<QString, QVariant> data = all_data.value(hash).toHash();
    //qDebug("TorrentPersistentData: getSavePath %s", data["save_path"].toString().toLocal8Bit().data());
    return data.value("save_path").toString();
  }

  static QString getLabel(const QString &hash) {
    QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent-resume"));
    const QHash<QString, QVariant> all_data = settings.value("torrents").toHash();
    const QHash<QString, QVariant> data = all_data.value(hash).toHash();
    return data.value("label", "").toString();
  }

  static QString getName(const QString &hash) {
    QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent-resume"));
    const QHash<QString, QVariant> all_data = settings.value("torrents").toHash();
    const QHash<QString, QVariant> data = all_data.value(hash).toHash();
    return data.value("name", "").toString();
  }

  static int getPriority(const QString &hash) {
    QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent-resume"));
    const QHash<QString, QVariant> all_data = settings.value("torrents").toHash();
    const QHash<QString, QVariant> data = all_data.value(hash).toHash();
    return data.value("priority", -1).toInt();
  }

  static bool isSeed(const QString &hash) {
    QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent-resume"));
    const QHash<QString, QVariant> all_data = settings.value("torrents").toHash();
    const QHash<QString, QVariant> data = all_data.value(hash).toHash();
    return data.value("seed", false).toBool();
  }

  static bool isMagnet(const QString &hash) {
    QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent-resume"));
    const QHash<QString, QVariant> all_data = settings.value("torrents").toHash();
    const QHash<QString, QVariant> data = all_data.value(hash).toHash();
    return data.value("is_magnet", false).toBool();
  }

  static QString getMagnetUri(const QString &hash) {
    QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent-resume"));
    const QHash<QString, QVariant> all_data = settings.value("torrents").toHash();
    const QHash<QString, QVariant> data = all_data.value(hash).toHash();
    Q_ASSERT(data.value("is_magnet", false).toBool());
    return data.value("magnet_uri").toString();
  }

};

#endif // TORRENTPERSISTENTDATA_H
