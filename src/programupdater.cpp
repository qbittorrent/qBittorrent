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

#include "programupdater.h"
#include "misc.h"
#include "preferences.h"

#ifdef Q_WS_MAC
const QString RSS_URL = "http://sourceforge.net/api/file/index/project-id/163414/mtime/desc/rss?path=/qbittorrent-mac";
const QString FILE_EXT = "DMG";
#else
const QString RSS_URL = "http://sourceforge.net/api/file/index/project-id/163414/mtime/desc/rss?path=/qbittorrent-win32";
const QString FILE_EXT = "EXE";
#endif

using namespace libtorrent;

ProgramUpdater::ProgramUpdater(QObject *parent) :
    QObject(parent)
{
  mp_manager = new QNetworkAccessManager(this);
  Preferences pref;
  // Proxy support
  if(pref.isProxyEnabled()) {
    QNetworkProxy proxy;
    switch(pref.getProxyType()) {
    case Proxy::SOCKS4:
    case Proxy::SOCKS5:
    case Proxy::SOCKS5_PW:
      proxy.setType(QNetworkProxy::Socks5Proxy);
    default:
      proxy.setType(QNetworkProxy::HttpProxy);
      break;
    }
    proxy.setHostName(pref.getProxyIp());
    proxy.setPort(pref.getProxyPort());
    // Proxy authentication
    if(pref.isProxyAuthEnabled()) {
      proxy.setUser(pref.getProxyUsername());
      proxy.setPassword(pref.getProxyPassword());
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
  mp_manager->get(QNetworkRequest(QUrl(RSS_URL)));
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
  if(!reply->error()) {
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
        if(in_item && xml.name() == "title") {
          in_title = false;
          const QString ext = misc::file_extension(item_title).toUpper();
          qDebug("Found an update with file extension: %s", qPrintable(ext));
          if(ext == FILE_EXT) {
            qDebug("The last update available is %s", qPrintable(item_title));
            new_version = extractVersionNumber(item_title);
            if(!new_version.isEmpty()) {
              qDebug("Detected version is %s", qPrintable(new_version));
              if(isVersionMoreRecent(new_version))
                setUpdateUrl(item_title);
            }
            break;
          }
        } else if (xml.name() == "item") {
          in_item = false;
        }
      } else if (xml.isCharacters() && !xml.isWhitespace()) {
        if(in_item && in_title)
          item_title += xml.text().toString();
      }
    }
  }
  emit updateCheckFinished(!m_updateUrl.isEmpty(), new_version);
  // Clean up
  reply->deleteLater();
}

void ProgramUpdater::updateProgram()
{
  Q_ASSERT(!m_updateUrl.isEmpty());
  QDesktopServices::openUrl(m_updateUrl);
  return;
  /*connect(mp_manager, SIGNAL(finished(QNetworkReply*)),
          this, SLOT(saveUpdate(QNetworkReply*)));
  // Send the request
  mp_manager->get(QNetworkRequest(QUrl(m_updateUrl)));*/
}

/*void ProgramUpdater::saveUpdate(QNetworkReply *reply)
{
  // Disconnect SIGNAL/SLOT
  disconnect(mp_manager, 0, this, 0);
  // Process the download
  if(!reply->error()) {
    // Save the file
    const QString installer_path = QDir::temp().absoluteFilePath("qbittorrent_update."+FILE_EXT.toLower());
    QFile update_installer(installer_path);
    if(update_installer.exists()) {
      update_installer.remove();
    }
    if(update_installer.open(QIODevice::WriteOnly)) {
      update_installer.write(reply->readAll());
      reply->close();
      update_installer.close();
      // Install the update
      installUpdate(installer_path);
    } else {
      emit updateInstallFinished(tr("Could not create the file %1").arg(installer_path));
    }
  } else {
    emit updateInstallFinished(tr("Failed to download the update at %1", "%1 is an URL").arg(m_updateUrl));
  }
  reply->deleteLater();
  deleteLater();
}*/

/*void ProgramUpdater::installUpdate(QString update_path)
{
  qDebug("Installing the update at %s...", qPrintable(update_path));
#ifdef Q_WS_WIN
  QDesktopServices::openUrl(QUrl(QString("file:///")+update_path, QUrl::TolerantMode));
#else
  QDesktopServices::openUrl(QUrl(QString("file://")+update_path, QUrl::TolerantMode));
#endif
}*/

// title on Windows: /qbittorrent-win32/qbittorrent-2.4.7/qbittorrent_2.4.7_setup.exe
// title on Mac: /qbittorrent-mac/qbittorrent-2.4.4/qbittorrent-2.4.4.dmg
QString ProgramUpdater::extractVersionNumber(QString title) const
{
  QString version;
  QStringList parts = title.split("/");
  if(parts.size() != 4) {
    qDebug("ProgramUpdater: Unrecognized title: %s", qPrintable(title));
    return version;
  }
  QString folder = parts.at(2);
  if(!folder.contains("-")) {
    qDebug("ProgramUpdater: Unrecognized folder name: %s", qPrintable(folder));
    return version;
  }
  version = folder.mid(folder.lastIndexOf("-")+1);
  if(version.split(".").size() != 3) {
    qDebug("ProgramUpdater: Unrecognized version format: %s", qPrintable(version));
    return QString::null;
  }
  return version;
}

bool ProgramUpdater::isVersionMoreRecent(QString new_version) const
{
  const QStringList parts = new_version.split(".");
  Q_ASSERT(parts.size() == 3);
  const int major = parts.at(0).toInt();
  const int minor = parts.at(1).toInt();
  const int bugfix = parts.at(2).toInt();
  if(major < VERSION_MAJOR)
    return false;
  if(minor < VERSION_MINOR)
    return false;
  if(bugfix <= VERSION_BUGFIX)
    return false;
  return true;
}

