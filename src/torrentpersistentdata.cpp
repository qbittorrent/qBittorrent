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

#include "torrentpersistentdata.h"

#include <QDateTime>
#include <QDebug>
#include <QVariant>

#include "qinisettings.h"
#include "misc.h"
#include "qtorrenthandle.h"

#include <libtorrent/magnet_uri.hpp>

QHash<QString, TorrentTempData::TorrentData> TorrentTempData::data = QHash<QString, TorrentTempData::TorrentData>();
QHash<QString, TorrentTempData::TorrentMoveState> TorrentTempData::torrentMoveStates = QHash<QString, TorrentTempData::TorrentMoveState>();
QHash<QString, bool> HiddenData::data = QHash<QString, bool>();
unsigned int HiddenData::metadata_counter = 0;
TorrentPersistentData* TorrentPersistentData::m_instance = 0;

TorrentTempData::TorrentData::TorrentData()
    : sequential(false)
    , seed(false)
    , add_paused(Preferences::instance()->addTorrentsInPause())
{
}

TorrentTempData::TorrentMoveState::TorrentMoveState(QString oldPath, QString newPath)
    : oldPath(oldPath)
    , newPath(newPath)
{
}

bool TorrentTempData::hasTempData(const QString &hash)
{
    return data.contains(hash);
}

void TorrentTempData::deleteTempData(const QString &hash)
{
    data.remove(hash);
}

void TorrentTempData::setFilesPriority(const QString &hash,  const std::vector<int> &pp)
{
    data[hash].files_priority = pp;
}

void TorrentTempData::setFilesPath(const QString &hash, const QStringList &path_list)
{
    data[hash].path_list = path_list;
}

void TorrentTempData::setSavePath(const QString &hash, const QString &save_path)
{
    data[hash].save_path = save_path;
}

void TorrentTempData::setLabel(const QString &hash, const QString &label)
{
    data[hash].label = label;
}

void TorrentTempData::setSequential(const QString &hash, const bool &sequential)
{
    data[hash].sequential = sequential;
}

bool TorrentTempData::isSequential(const QString &hash)
{
    return data.value(hash).sequential;
}

void TorrentTempData::setSeedingMode(const QString &hash, const bool &seed)
{
    data[hash].seed = seed;
}

bool TorrentTempData::isSeedingMode(const QString &hash)
{
    return data.value(hash).seed;
}

QString TorrentTempData::getSavePath(const QString &hash)
{
    return data.value(hash).save_path;
}

QStringList TorrentTempData::getFilesPath(const QString &hash)
{
    return data.value(hash).path_list;
}

QString TorrentTempData::getLabel(const QString &hash)
{
    return data.value(hash).label;
}

void TorrentTempData::getFilesPriority(const QString &hash, std::vector<int> &fp)
{
    fp = data.value(hash).files_priority;
}

bool TorrentTempData::isMoveInProgress(const QString &hash)
{
    return torrentMoveStates.find(hash) != torrentMoveStates.end();
}

void TorrentTempData::enqueueMove(const QString &hash, const QString &queuedPath)
{
    QHash<QString, TorrentMoveState>::iterator i = torrentMoveStates.find(hash);
    if (i == torrentMoveStates.end()) {
        Q_ASSERT(false);
        return;
    }
    i->queuedPath = queuedPath;
}

void TorrentTempData::startMove(const QString &hash, const QString &oldPath, const QString& newPath)
{
    QHash<QString, TorrentMoveState>::iterator i = torrentMoveStates.find(hash);
    if (i != torrentMoveStates.end()) {
        Q_ASSERT(false);
        return;
    }

    torrentMoveStates.insert(hash, TorrentMoveState(oldPath, newPath));
}

void TorrentTempData::finishMove(const QString &hash)
{
    QHash<QString, TorrentMoveState>::iterator i = torrentMoveStates.find(hash);
    if (i == torrentMoveStates.end()) {
        Q_ASSERT(false);
        return;
    }
    torrentMoveStates.erase(i);
}

QString TorrentTempData::getOldPath(const QString &hash)
{
    QHash<QString, TorrentMoveState>::iterator i = torrentMoveStates.find(hash);
    if (i == torrentMoveStates.end()) {
        Q_ASSERT(false);
        return QString();
    }
    return i->oldPath;
}

QString TorrentTempData::getNewPath(const QString &hash)
{
    QHash<QString, TorrentMoveState>::iterator i = torrentMoveStates.find(hash);
    if (i == torrentMoveStates.end()) {
        Q_ASSERT(false);
        return QString();
    }
    return i->newPath;
}

QString TorrentTempData::getQueuedPath(const QString &hash)
{
    QHash<QString, TorrentMoveState>::iterator i = torrentMoveStates.find(hash);
    if (i == torrentMoveStates.end()) {
        Q_ASSERT(false);
        return QString();
    }
    return i->queuedPath;
}

void TorrentTempData::setAddPaused(const QString &hash, const bool &paused)
{
    data[hash].add_paused = paused;
}

bool TorrentTempData::isAddPaused(const QString &hash)
{
    return data.value(hash).add_paused;
}

void HiddenData::addData(const QString &hash)
{
    data[hash] = false;
}

bool HiddenData::hasData(const QString &hash)
{
    return data.contains(hash);
}

void HiddenData::deleteData(const QString &hash)
{
    if (data.value(hash, false))
        metadata_counter--;
    data.remove(hash);
}

int HiddenData::getSize()
{
    return data.size();
}

int HiddenData::getDownloadingSize()
{
    return data.size() - metadata_counter;
}

void HiddenData::gotMetadata(const QString &hash)
{
    if (!data.contains(hash))
        return;
    data[hash] = true;
    metadata_counter++;
}

TorrentPersistentData::TorrentPersistentData()
    : m_data(QIniSettings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent-resume")).value("torrents").toHash())
    , dirty(false)
{
    timer.setSingleShot(true);
    timer.setInterval(5*1000);
    connect(&timer, SIGNAL(timeout()), SLOT(save()));
}

TorrentPersistentData::~TorrentPersistentData()
{
    save();
}

void TorrentPersistentData::save()
{
    if (!dirty)
        return;

    QIniSettings settings(QString::fromUtf8("qBittorrent"), QString::fromUtf8("qBittorrent-resume"));
    settings.setValue("torrents", m_data);
    dirty = false;
}

const QVariant TorrentPersistentData::value(const QString &key, const QVariant &defaultValue) const {
    QReadLocker locker(&lock);
    return m_data.value(key, defaultValue);
}

void TorrentPersistentData::setValue(const QString &key, const QVariant &value) {
    QWriteLocker locker(&lock);
    if (m_data.value(key) == value)
        return;
    dirty = true;
    timer.start();
    m_data.insert(key, value);
}

TorrentPersistentData* TorrentPersistentData::instance()
{
    if (!m_instance)
        m_instance = new TorrentPersistentData;

    return m_instance;
}

void TorrentPersistentData::drop()
{
    if (m_instance) {
        delete m_instance;
        m_instance = 0;
    }
}

bool TorrentPersistentData::isKnownTorrent(QString hash) const
{
    QReadLocker locker(&lock);
    return m_data.contains(hash);
}

QStringList TorrentPersistentData::knownTorrents() const
{
    QReadLocker locker(&lock);
    return m_data.keys();
}

void TorrentPersistentData::setRatioLimit(const QString &hash, const qreal &ratio)
{
    QHash<QString, QVariant> torrent = value(hash).toHash();
    torrent["max_ratio"] = ratio;
    setValue(hash, torrent);
}

qreal TorrentPersistentData::getRatioLimit(const QString &hash) const
{
    const QHash<QString, QVariant> torrent = value(hash).toHash();
    return torrent.value("max_ratio", USE_GLOBAL_RATIO).toReal();
}

bool TorrentPersistentData::hasPerTorrentRatioLimit() const
{
    QReadLocker locker(&lock);
    QHash<QString, QVariant>::ConstIterator it = m_data.constBegin();
    QHash<QString, QVariant>::ConstIterator itend = m_data.constEnd();
    for (; it != itend; ++it)
        if (it.value().toHash().value("max_ratio", USE_GLOBAL_RATIO).toReal() >= 0)
            return true;
    return false;
}

void TorrentPersistentData::setAddedDate(const QString &hash, const QDateTime &time)
{
    QHash<QString, QVariant> torrent = value(hash).toHash();
    if (!torrent.contains("add_date")) {
        torrent["add_date"] = time;
        setValue(hash, torrent);
    }
}

QDateTime TorrentPersistentData::getAddedDate(const QString &hash) const
{
    const QHash<QString, QVariant> torrent = value(hash).toHash();
    QDateTime dt = torrent.value("add_date").toDateTime();
    if (!dt.isValid())
        dt = QDateTime::currentDateTime();
    return dt;
}

void TorrentPersistentData::setErrorState(const QString &hash, const bool has_error)
{
    QHash<QString, QVariant> torrent = value(hash).toHash();
    torrent["has_error"] = has_error;
    setValue(hash, torrent);
}

bool TorrentPersistentData::hasError(const QString &hash) const
{
    const QHash<QString, QVariant> torrent = value(hash).toHash();
    return torrent.value("has_error", false).toBool();
}

QDateTime TorrentPersistentData::getSeedDate(const QString &hash) const
{
    const QHash<QString, QVariant> torrent = value(hash).toHash();
    return torrent.value("seed_date").toDateTime();
}

void TorrentPersistentData::deletePersistentData(const QString &hash)
{
    QWriteLocker locker(&lock);
    if (m_data.contains(hash)) {
        m_data.remove(hash);
        dirty = true;
        timer.start();
    }
}

void TorrentPersistentData::saveTorrentPersistentData(const QTorrentHandle &h, const QString &save_path, const bool is_magnet)
{
    Q_ASSERT(h.is_valid());
    QString hash = h.hash();
    qDebug("Saving persistent data for %s", qPrintable(hash));
    // Save persistent data
    QHash<QString, QVariant> torrent = value(hash).toHash();
    torrent["is_magnet"] = is_magnet;
    if (is_magnet)
        torrent["magnet_uri"] = misc::toQString(make_magnet_uri(h));
    torrent["seed"] = h.is_seed();
    torrent["priority"] = h.queue_position();
    if (save_path.isEmpty()) {
        qDebug("TorrentPersistantData: save path is %s", qPrintable(h.save_path()));
        torrent["save_path"] = h.save_path();
    }
    else {
        qDebug("TorrentPersistantData: overriding save path is %s", qPrintable(save_path));
        torrent["save_path"] = save_path; // Override torrent save path (e.g. because it is a temp dir)
    }
    // Label
    torrent["label"] = TorrentTempData::getLabel(hash);
    // Save data
    setValue(hash, torrent);
    qDebug("TorrentPersistentData: Saving save_path %s, hash: %s", qPrintable(h.save_path()), qPrintable(hash));
    // Set Added date
    setAddedDate(hash, QDateTime::currentDateTime());
    // Finally, remove temp data
    TorrentTempData::deleteTempData(hash);
}

void TorrentPersistentData::saveSavePath(const QString &hash, const QString &save_path)
{
    Q_ASSERT(!hash.isEmpty());
    qDebug("TorrentPersistentData::saveSavePath(%s)", qPrintable(save_path));
    QHash<QString, QVariant> torrent = value(hash).toHash();
    torrent["save_path"] = save_path;
    setValue(hash, torrent);
    qDebug("TorrentPersistentData: Saving save_path: %s, hash: %s", qPrintable(save_path), qPrintable(hash));
}

void TorrentPersistentData::saveLabel(const QString &hash, const QString &label)
{
    Q_ASSERT(!hash.isEmpty());
    QHash<QString, QVariant> torrent = value(hash).toHash();
    torrent["label"] = label;
    setValue(hash, torrent);
}

void TorrentPersistentData::saveName(const QString &hash, const QString &name)
{
    Q_ASSERT(!hash.isEmpty());
    QHash<QString, QVariant> torrent = value(hash).toHash();
    torrent["name"] = name;
    setValue(hash, torrent);
}

void TorrentPersistentData::savePriority(const QTorrentHandle &h)
{
    QString hash = h.hash();
    QHash<QString, QVariant> torrent = value(hash).toHash();
    torrent["priority"] = h.queue_position();
    setValue(hash, torrent);
}

void TorrentPersistentData::savePriority(const QString &hash, const int &queue_pos)
{
    QHash<QString, QVariant> torrent = value(hash).toHash();
    torrent["priority"] = queue_pos;
    setValue(hash, torrent);
}

void TorrentPersistentData::saveSeedStatus(const QTorrentHandle &h)
{
    QString hash = h.hash();
    QHash<QString, QVariant> torrent = value(hash).toHash();
    bool was_seed = torrent.value("seed", false).toBool();
    if (was_seed != h.is_seed()) {
        torrent["seed"] = !was_seed;
        setValue(hash, torrent);
    }
}

void TorrentPersistentData::saveSeedStatus(const QString &hash, const bool seedStatus)
{
    QHash<QString, QVariant> torrent = value(hash).toHash();
    torrent["seed"] = seedStatus;
    setValue(hash, torrent);
}

QString TorrentPersistentData::getSavePath(const QString &hash) const
{
    const QHash<QString, QVariant> torrent = value(hash).toHash();
    //qDebug("TorrentPersistentData: getSavePath %s", data["save_path"].toString().toLocal8Bit().data());
    return torrent.value("save_path").toString();
}

QString TorrentPersistentData::getLabel(const QString &hash) const
{
    const QHash<QString, QVariant> torrent = value(hash).toHash();
    return torrent.value("label", "").toString();
}

QString TorrentPersistentData::getName(const QString &hash) const
{
    const QHash<QString, QVariant> torrent = value(hash).toHash();
    return torrent.value("name", "").toString();
}

int TorrentPersistentData::getPriority(const QString &hash) const
{
    const QHash<QString, QVariant> torrent = value(hash).toHash();
    return torrent.value("priority", -1).toInt();
}

bool TorrentPersistentData::isSeed(const QString &hash) const
{
    const QHash<QString, QVariant> torrent = value(hash).toHash();
    return torrent.value("seed", false).toBool();
}

bool TorrentPersistentData::isMagnet(const QString &hash) const
{
    const QHash<QString, QVariant> torrent = value(hash).toHash();
    return torrent.value("is_magnet", false).toBool();
}

QString TorrentPersistentData::getMagnetUri(const QString &hash) const
{
    const QHash<QString, QVariant> torrent = value(hash).toHash();
    Q_ASSERT(torrent.value("is_magnet", false).toBool());
    return torrent.value("magnet_uri").toString();
}
