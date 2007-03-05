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
 * Contact : chris@qbittorrent.org
 */
#ifndef __BITTORRENT_H__
#define __BITTORRENT_H__

#include <QHash>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <libtorrent/entry.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/fingerprint.hpp>
#include <libtorrent/session_settings.hpp>
#include <libtorrent/identify_client.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/extensions/metadata_transfer.hpp>
#include <libtorrent/extensions/ut_pex.hpp>

#include <boost/format.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem/exception.hpp>

#include "deleteThread.h"

#define VERSION "v0.9.0beta3"
#define VERSION_MAJOR 0
#define VERSION_MINOR 9
#define VERSION_BUGFIX 0

using namespace libtorrent;
namespace fs = boost::filesystem;

class bittorrent{
  private:
    session *s;
    QHash<QString, QStringList> trackerErrors;
    bool DHTEnabled;
    String scan_dir;
    QTimer *timerScan;

    // Constructor / Destructor
    bittorrent();
    ~bittorrent();

  public:
    torrent_handle& getTorrentHandle(const QString& hash) const;
    bool hasFilteredFiles(const QString& fileHash) const;
    bool isDHTEnabled() const;

  public slots:
    void addTorrent(const QString& path, bool fromScanDir = false, const QString& from_url = QString());
    void deleteTorrent(const QString& hash, bool permanent=false);
    void pauseTorrent(const QString& hash);
    void resumeTorrent(const QString& hash);
    void enableDHT();
    void disableDHT();
    void saveFastResumeData();
    void enableDirectoryScanning(const QString& scan_dir);
    void disableDirectoryScanning();
    // Session configuration - Setters
    void setListeningPortsRange(std::pair<unsigned short, unsigned short> ports);
    void setDownloadRateLimit(int rate);
    void setUploadRateLimit(int rate);
    void setGlobalRatio(float ratio);

  protected slots:
    void cleanDeleter(deleteThread* deleter);
    void loadFilteredFiles(torrent_handle& h);
    void scanDirectory();

  signals:
    void invalidTorrent(const QString& path);
    void duplicateTorrent(const QString& path);
    void addedTorrent(const QString& path, torrent_handle& h, bool fastResume);
    void resumedTorrent(const QString& path);
}

#endif
