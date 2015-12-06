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
#include "base/utils/fs.h"
#include "base/preferences.h"

namespace
{
    const QUrl RSS_URL("http://www.fosshub.com/software/feedqBittorent");

#ifdef Q_OS_MAC
    const QString OS_TYPE("Mac OS X");
#else
    const QString OS_TYPE("Windows");
#endif

    QString getStringValue(QXmlStreamReader &xml);
}

ProgramUpdater::ProgramUpdater(QObject *parent, bool invokedByUser)
    : QObject(parent)
    , m_invokedByUser(invokedByUser)
{
    m_networkManager = new QNetworkAccessManager(this);
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
        m_networkManager->setProxy(proxy);
    }
}

ProgramUpdater::~ProgramUpdater()
{
    delete m_networkManager;
}

void ProgramUpdater::checkForUpdates()
{
    // SIGNAL/SLOT
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)),
            this, SLOT(rssDownloadFinished(QNetworkReply*)));
    // Send the request
    QNetworkRequest request(RSS_URL);
    // Don't change this User-Agent. In case our updater goes haywire, the filehost can indetify it and contact us.
    request.setRawHeader("User-Agent", QString("qBittorrent/%1 ProgramUpdater (www.qbittorrent.org)").arg(VERSION).toLocal8Bit());
    m_networkManager->get(request);
}

void ProgramUpdater::rssDownloadFinished(QNetworkReply *reply)
{
    // Disconnect SIGNAL/SLOT
    disconnect(m_networkManager, 0, this, 0);
    qDebug("Finished downloading the new qBittorrent updates RSS");
    QString version;

    if (!reply->error()) {
        qDebug("No download error, good.");
        QXmlStreamReader xml(reply);
        bool inItem = false;
        QString updateLink;
        QString type;

        while (!xml.atEnd()) {
            xml.readNext();

            if (xml.isStartElement()) {
                if (xml.name() == "item")
                    inItem = true;
                else if (inItem && xml.name() == "link")
                    updateLink = getStringValue(xml);
                else if (inItem && xml.name() == "type")
                    type = getStringValue(xml);
                else if (inItem && xml.name() == "version")
                    version = getStringValue(xml);
            }
            else if (xml.isEndElement()) {
                if (inItem && xml.name() == "item") {
                    if (type.compare(OS_TYPE, Qt::CaseInsensitive) == 0) {
                        qDebug("The last update available is %s", qPrintable(version));
                        if (!version.isEmpty()) {
                            qDebug("Detected version is %s", qPrintable(version));
                            if (isVersionMoreRecent(version))
                                m_updateUrl = updateLink;
                        }
                        break;
                    }

                    inItem = false;
                    updateLink.clear();
                    type.clear();
                    version.clear();
                }
            }
        }
    }

    emit updateCheckFinished(!m_updateUrl.isEmpty(), version, m_invokedByUser);
    // Clean up
    reply->deleteLater();
}

void ProgramUpdater::updateProgram()
{
    Q_ASSERT(!m_updateUrl.isEmpty());
    QDesktopServices::openUrl(m_updateUrl);
    return;
}

bool ProgramUpdater::isVersionMoreRecent(const QString &remoteVersion) const
{
    QRegExp regVer("([0-9.]+)");
    if (regVer.indexIn(QString(VERSION)) >= 0) {
        QString localVersion = regVer.cap(1);
        qDebug() << Q_FUNC_INFO << "local version:" << localVersion << "/" << VERSION;
        QStringList remoteParts = remoteVersion.split('.');
        QStringList localParts = localVersion.split('.');
        for (int i = 0; i<qMin(remoteParts.size(), localParts.size()); ++i) {
            if (remoteParts[i].toInt() > localParts[i].toInt())
                return true;
            if (remoteParts[i].toInt() < localParts[i].toInt())
                return false;
        }
        // Compared parts were equal, if remote version is longer, then it's more recent (2.9.2.1 > 2.9.2)
        if (remoteParts.size() > localParts.size())
            return true;
        // versions are equal, check if the local version is a development release, in which case it is older (2.9.2beta < 2.9.2)
        QRegExp regDevel("(alpha|beta|rc)");
        if (regDevel.indexIn(VERSION) >= 0)
            return true;
    }
    return false;
}

namespace
{
    QString getStringValue(QXmlStreamReader &xml)
    {
        xml.readNext();
        if (xml.isCharacters() && !xml.isWhitespace())
            return xml.text().toString();

        return QString();
    }
}
