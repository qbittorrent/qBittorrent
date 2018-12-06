/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2010  Christophe Dumez <chris@qbittorrent.org>
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

#include "programupdater.h"

#include <QDebug>
#include <QDesktopServices>
#include <QRegularExpression>
#include <QStringList>
#include <QSysInfo>
#include <QXmlStreamReader>

#include "base/net/downloadhandler.h"
#include "base/net/downloadmanager.h"
#include "base/utils/fs.h"

namespace
{
    const QString RSS_URL {QStringLiteral("https://www.fosshub.com/feed/5b8793a7f9ee5a5c3e97a3b2.xml")};

    QString getStringValue(QXmlStreamReader &xml);
}

ProgramUpdater::ProgramUpdater(QObject *parent, bool invokedByUser)
    : QObject(parent)
    , m_invokedByUser(invokedByUser)
{
}

void ProgramUpdater::checkForUpdates()
{
    // Don't change this User-Agent. In case our updater goes haywire,
    // the filehost can identify it and contact us.
    Net::DownloadHandler *handler = Net::DownloadManager::instance()->download(
                Net::DownloadRequest(RSS_URL).userAgent("qBittorrent/" QBT_VERSION_2 " ProgramUpdater (www.qbittorrent.org)"));
    connect(handler, static_cast<void (Net::DownloadHandler::*)(const QString &, const QByteArray &)>(&Net::DownloadHandler::downloadFinished)
            , this, &ProgramUpdater::rssDownloadFinished);
    connect(handler, &Net::DownloadHandler::downloadFailed, this, &ProgramUpdater::rssDownloadFailed);
}

void ProgramUpdater::rssDownloadFinished(const QString &url, const QByteArray &data)
{
    Q_UNUSED(url);
    qDebug("Finished downloading the new qBittorrent updates RSS");

#ifdef Q_OS_MAC
    const QString OS_TYPE {"Mac OS X"};
#elif defined(Q_OS_WIN)
    const QString OS_TYPE {((QSysInfo::windowsVersion() >= QSysInfo::WV_WINDOWS7)
            && QSysInfo::currentCpuArchitecture().endsWith("64"))
        ? "Windows x64" : "Windows"};
#endif

    QString version;
    QXmlStreamReader xml(data);
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
                    qDebug("The last update available is %s", qUtf8Printable(version));
                    if (!version.isEmpty()) {
                        qDebug("Detected version is %s", qUtf8Printable(version));
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

    emit updateCheckFinished(!m_updateUrl.isEmpty(), version, m_invokedByUser);
}

void ProgramUpdater::rssDownloadFailed(const QString &url, const QString &error)
{
    Q_UNUSED(url);

    qDebug() << "Downloading the new qBittorrent updates RSS failed:" << error;
    emit updateCheckFinished(false, QString(), m_invokedByUser);
}

void ProgramUpdater::updateProgram()
{
    Q_ASSERT(!m_updateUrl.isEmpty());
    QDesktopServices::openUrl(m_updateUrl);
    return;
}

bool ProgramUpdater::isVersionMoreRecent(const QString &remoteVersion) const
{
    const QRegularExpressionMatch regVerMatch = QRegularExpression("([0-9.]+)").match(QBT_VERSION);
    if (regVerMatch.hasMatch()) {
        QString localVersion = regVerMatch.captured(1);
        qDebug() << Q_FUNC_INFO << "local version:" << localVersion << "/" << QBT_VERSION;
        QStringList remoteParts = remoteVersion.split('.');
        QStringList localParts = localVersion.split('.');
        for (int i = 0; i < qMin(remoteParts.size(), localParts.size()); ++i) {
            if (remoteParts[i].toInt() > localParts[i].toInt())
                return true;
            if (remoteParts[i].toInt() < localParts[i].toInt())
                return false;
        }
        // Compared parts were equal, if remote version is longer, then it's more recent (2.9.2.1 > 2.9.2)
        if (remoteParts.size() > localParts.size())
            return true;
        // versions are equal, check if the local version is a development release, in which case it is older (2.9.2beta < 2.9.2)
        const QRegularExpressionMatch regDevelMatch = QRegularExpression("(alpha|beta|rc)").match(QBT_VERSION);
        if (regDevelMatch.hasMatch())
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
