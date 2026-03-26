/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2021  Mike Tzou (Chocobo1)
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

#include <algorithm>

#include <libtorrent/version.hpp>

#include <QtCore/qconfig.h>
#include <QtSystemDetection>
#include <QDebug>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonValue>
#include <QRegularExpression>
#include <QXmlStreamReader>

#include "base/global.h"
#include "base/logger.h"
#include "base/net/downloadmanager.h"
#include "base/preferences.h"
#include "base/version.h"

namespace
{
    bool isVersionMoreRecent(const ProgramUpdater::Version &remoteVersion)
    {
        if (!remoteVersion.isValid())
            return false;

        const ProgramUpdater::Version currentVersion {QBT_VERSION_MAJOR, QBT_VERSION_MINOR, QBT_VERSION_BUGFIX, QBT_VERSION_BUILD};
        if (remoteVersion == currentVersion)
        {
            const bool isDevVersion = QStringLiteral(QBT_VERSION_STATUS).contains(
                QRegularExpression(u"(alpha|beta|rc)"_s));
            if (isDevVersion)
                return true;
        }
        return (remoteVersion > currentVersion);
    }

    QString buildVariant()
    {
#if defined(Q_OS_MACOS)
        const auto BASE_OS = u"Mac OS X"_s;
#elif defined(Q_OS_WIN)
        const auto BASE_OS = u"Windows x64"_s;
#endif

        if constexpr ((QT_VERSION_MAJOR == 6) && (LIBTORRENT_VERSION_MAJOR == 1))
            return BASE_OS;

        return u"%1 (qt%2 lt%3%4)"_s.arg(BASE_OS, QString::number(QT_VERSION_MAJOR), QString::number(LIBTORRENT_VERSION_MAJOR), QString::number(LIBTORRENT_VERSION_MINOR));
    }
}

void ProgramUpdater::checkForUpdates()
{
    // Don't change this User-Agent. In case our updater goes haywire,
    // the filehost can identify it and contact us.
    const auto USER_AGENT = QStringLiteral("qBittorrent/" QBT_VERSION_2 " ProgramUpdater (www.qbittorrent.org)");
    const auto FOSSHUB_URL = u"https://www.fosshub.com/feed/5b8793a7f9ee5a5c3e97a3b2.xml"_s;
    const auto QBT_MAIN_URL = u"https://www.qbittorrent.org/versions.json"_s;
    const auto QBT_BACKUP_URL = u"https://qbittorrent.github.io/qBittorrent-website/versions.json"_s;

    Net::DownloadManager *netManager = Net::DownloadManager::instance();
    const bool useProxy = Preferences::instance()->useProxyForGeneralPurposes();

    m_pendingRequestCount = 3;
    netManager->download(Net::DownloadRequest(FOSSHUB_URL).userAgent(USER_AGENT), useProxy, this, &ProgramUpdater::rssDownloadFinished);
    // don't use the custom user agent for the following requests, disguise as a normal browser instead
    netManager->download(Net::DownloadRequest(QBT_MAIN_URL), useProxy, this, [this](const Net::DownloadResult &result)
    {
        fallbackDownloadFinished(result, m_qbtMainVersion);
    });
    netManager->download(Net::DownloadRequest(QBT_BACKUP_URL), useProxy, this, [this](const Net::DownloadResult &result)
    {
        fallbackDownloadFinished(result, m_qbtBackupVersion);
    });
}

ProgramUpdater::Version ProgramUpdater::getNewVersion() const
{
    switch (getLatestRemoteSource())
    {
    case RemoteSource::Fosshub:
        return m_fosshubVersion;
    case RemoteSource::QbtMain:
        return m_qbtMainVersion;
    case RemoteSource::QbtBackup:
        return m_qbtBackupVersion;
    }
    Q_UNREACHABLE();
}

void ProgramUpdater::rssDownloadFinished(const Net::DownloadResult &result)
{
    if (result.status != Net::DownloadStatus::Success)
    {
        LogMsg(tr("Failed to download the program update info. URL: \"%1\". Error: \"%2\"").arg(result.url, result.errorString) , Log::WARNING);
        handleFinishedRequest();
        return;
    }

    const auto getStringValue = [](QXmlStreamReader &xml) -> QString
    {
        xml.readNext();
        return (xml.isCharacters() && !xml.isWhitespace())
            ? xml.text().toString()
            : QString {};
    };

    const QString variant = buildVariant();
    bool inItem = false;
    QString version;
    QString updateLink;
    QString type;
    QXmlStreamReader xml(result.data);

    while (!xml.atEnd())
    {
        xml.readNext();

        if (xml.isStartElement())
        {
            if (xml.name() == u"item")
                inItem = true;
            else if (inItem && (xml.name() == u"link"))
                updateLink = getStringValue(xml);
            else if (inItem && (xml.name() == u"type"))
                type = getStringValue(xml);
            else if (inItem && (xml.name() == u"version"))
                version = getStringValue(xml);
        }
        else if (xml.isEndElement())
        {
            if (inItem && (xml.name() == u"item"))
            {
                if (type.compare(variant, Qt::CaseInsensitive) == 0)
                {
                    qDebug("The last update available is %s", qUtf8Printable(version));
                    if (!version.isEmpty())
                    {
                        qDebug("Detected version is %s", qUtf8Printable(version));
                        const Version tmpVer {version};
                        if (isVersionMoreRecent(tmpVer))
                        {
                            m_fosshubVersion = tmpVer;
                            m_updateURL = updateLink;
                        }
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

    handleFinishedRequest();
}

void ProgramUpdater::fallbackDownloadFinished(const Net::DownloadResult &result, Version &version)
{
    version = {};

    if (result.status != Net::DownloadStatus::Success)
    {
        LogMsg(tr("Failed to download the program update info. URL: \"%1\". Error: \"%2\"").arg(result.url, result.errorString) , Log::WARNING);
        handleFinishedRequest();
        return;
    }

    const auto json = QJsonDocument::fromJson(result.data);

#if defined(Q_OS_MACOS)
    const QString platformKey = u"macos"_s;
#elif defined(Q_OS_WIN)
    const QString platformKey = u"win"_s;
#endif

    if (const QJsonValue verJSON = json[platformKey][u"version"_s]; verJSON.isString())
    {
        const Version tmpVer {verJSON.toString()};
        if (isVersionMoreRecent(tmpVer))
            version = tmpVer;
    }

    handleFinishedRequest();
}

bool ProgramUpdater::updateProgram() const
{
    switch (getLatestRemoteSource())
    {
    case RemoteSource::Fosshub:
        return QDesktopServices::openUrl(m_updateURL);
    case RemoteSource::QbtMain:
        return QDesktopServices::openUrl(u"https://www.qbittorrent.org/download"_s);
    case RemoteSource::QbtBackup:
        return QDesktopServices::openUrl(u"https://qbittorrent.github.io/qBittorrent-website/download"_s);
    }
    Q_UNREACHABLE();
}

void ProgramUpdater::handleFinishedRequest()
{
    --m_pendingRequestCount;
    if (m_pendingRequestCount == 0)
        emit updateCheckFinished();
}

ProgramUpdater::RemoteSource ProgramUpdater::getLatestRemoteSource() const
{
    const Version max = std::max({m_fosshubVersion, m_qbtMainVersion, m_qbtBackupVersion});
    if (max == m_fosshubVersion)
        return RemoteSource::Fosshub;
    if (max == m_qbtMainVersion)
        return RemoteSource::QbtMain;
    if (max == m_qbtBackupVersion)
        return RemoteSource::QbtBackup;
    Q_UNREACHABLE();
}
