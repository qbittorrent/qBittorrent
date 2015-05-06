/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2010  Christophe Dumez
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

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QXmlStreamReader>
#include <QNetworkProxy>
#include <QDesktopServices>
#include <QDebug>
#include <QRegExp>
#include <QStringList>

#include "programupdater.h"
#include "core/utils/fs.h"
#include "core/preferences.h"

#ifdef Q_OS_MAC
const QUrl RSS_URL("http://sourceforge.net/projects/qbittorrent/rss?path=/qbittorrent-mac");
const QString FILE_EXT = "DMG";
#else
const QUrl RSS_URL("http://sourceforge.net/projects/qbittorrent/rss?path=/qbittorrent-win32");
const QString FILE_EXT = "EXE";
#endif

ProgramUpdater::ProgramUpdater(QObject *parent, bool invokedByUser) :
  QObject(parent), m_invokedByUser(invokedByUser)
{
  mp_manager = new QNetworkAccessManager(this);
  Preferences* const pref = Preferences::instance();
  // Proxy support
  if (pref->isProxyEnabled()) {
    QNetworkProxy proxy;
    switch(pref->getProxyType()) {
    case Proxy::SOCKS4:
    case Proxy::SOCKS5:
    case Proxy::SOCKS5_PW:
      proxy.setType(QNetworkProxy::Socks5Proxy);
    default:
      proxy.setType(QNetworkProxy::HttpProxy);
      break;
    }
    proxy.setHostName(pref->getProxyIp());
    proxy.setPort(pref->getProxyPort());
    // Proxy authentication
    if (pref->isProxyAuthEnabled()) {
      proxy.setUser(pref->getProxyUsername());
      proxy.setPassword(pref->getProxyPassword());
    }
    mp_manager->setProxy(proxy);
  }
}

ProgramUpdater::~ProgramUpdater() {
  delete mp_manager;
}

void ProgramUpdater::checkForUpdates()
{
  // SIGNAL/SLOT
  connect(mp_manager, SIGNAL(finished(QNetworkReply*)),
          this, SLOT(rssDownloadFinished(QNetworkReply*)));
  // Send the request
  QNetworkRequest request(RSS_URL);
  request.setRawHeader("User-Agent", QString("qBittorrent/%1 ProgramUpdater (www.qbittorrent.org)").arg(VERSION).toLocal8Bit());
  mp_manager->get(request);
}

void ProgramUpdater::setUpdateUrl(QString title) {
  m_updateUrl = "http://downloads.sourceforge.net/project/qbittorrent"+title;
  qDebug("The Update URL is %s", qPrintable(m_updateUrl));
}

void ProgramUpdater::rssDownloadFinished(QNetworkReply *reply)
{
  // Disconnect SIGNAL/SLOT
  disconnect(mp_manager, 0, this, 0);
  qDebug("Finished downloading the new qBittorrent updates RSS");
  QString new_version;
  if (!reply->error()) {
    qDebug("No download error, good.");
    QXmlStreamReader xml(reply);
    QString item_title;
    bool in_title = false;
    bool in_item = false;
    while (!xml.atEnd()) {
      xml.readNext();
      if (xml.isStartElement()) {
        if (in_item && xml.name() == "title") {
          in_title = true;
          item_title = "";
        } else if (xml.name() == "item") {
          in_item = true;
        }
      } else if (xml.isEndElement()) {
        if (in_item && xml.name() == "title") {
          in_title = false;
          const QString ext = Utils::Fs::fileExtension(item_title).toUpper();
          qDebug("Found an update with file extension: %s", qPrintable(ext));
          if (ext == FILE_EXT) {
            qDebug("The last update available is %s", qPrintable(item_title));
            new_version = extractVersionNumber(item_title);
            if (!new_version.isEmpty()) {
              qDebug("Detected version is %s", qPrintable(new_version));
              if (isVersionMoreRecent(new_version))
                setUpdateUrl(item_title);
            }
            break;
          }
        } else if (xml.name() == "item") {
          in_item = false;
        }
      } else if (xml.isCharacters() && !xml.isWhitespace()) {
        if (in_item && in_title)
          item_title += xml.text().toString();
      }
    }
  }
  emit updateCheckFinished(!m_updateUrl.isEmpty(), new_version, m_invokedByUser);
  // Clean up
  reply->deleteLater();
}

void ProgramUpdater::updateProgram()
{
  Q_ASSERT(!m_updateUrl.isEmpty());
  QDesktopServices::openUrl(m_updateUrl);
  return;
}

// title on Windows: /qbittorrent-win32/qbittorrent-2.4.7/qbittorrent_2.4.7_setup.exe
// title on Mac: /qbittorrent-mac/qbittorrent-2.4.4/qbittorrent-2.4.4.dmg
QString ProgramUpdater::extractVersionNumber(const QString& title) const
{
  qDebug() << Q_FUNC_INFO << title;
  QRegExp regVer("qbittorrent[_-]([0-9.]+)(_setup)?(\\.exe|\\.dmg)");
  if (regVer.indexIn(title)  < 0) {
    qWarning() << Q_FUNC_INFO << "Failed to extract version from file name:" << title;
    return QString::null;
  } else {
    QString version = regVer.cap(1);
    qDebug() << Q_FUNC_INFO << "Extracted version:" << version;
    return version;
  }
}

bool ProgramUpdater::isVersionMoreRecent(const QString& remote_version) const
{
  QRegExp regVer("([0-9.]+)");
  if (regVer.indexIn(QString(VERSION)) >= 0) {
    QString local_version = regVer.cap(1);
    qDebug() << Q_FUNC_INFO << "local version:" << local_version << "/" << VERSION;
    QStringList remote_parts = remote_version.split('.');
    QStringList local_parts = local_version.split('.');
    for (int i=0; i<qMin(remote_parts.size(), local_parts.size()); ++i) {
      if (remote_parts[i].toInt() > local_parts[i].toInt())
        return true;
      if (remote_parts[i].toInt() < local_parts[i].toInt())
        return false;
    }
    // Compared parts were equal, if remote version is longer, then it's more recent (2.9.2.1 > 2.9.2)
    if (remote_parts.size() > local_parts.size())
      return true;
    // versions are equal, check if the local version is a development release, in which case it is older (2.9.2beta < 2.9.2)
    QRegExp regDevel("(alpha|beta|rc)");
    if (regDevel.indexIn(VERSION) >= 0)
      return true;
  }
  return false;
}

