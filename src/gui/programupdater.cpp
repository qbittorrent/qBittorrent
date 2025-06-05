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

#include <libtorrent/version.hpp>

#include <QtCore/qconfig.h>
#include <QtSystemDetection>
#include <QDebug>
#include <QDesktopServices>
#include <QRegularExpression>
#include <QXmlStreamReader>

#include "base/global.h"
#include "base/net/downloadmanager.h"
#include "base/preferences.h"
#include "base/utils/version.h"
#include "base/version.h"

namespace
{
    bool isVersionMoreRecent(const QString &remoteVersion)
    {
        using Version = Utils::Version<4, 3>;

        const auto newVersion = Version::fromString(remoteVersion);
        if (!newVersion.isValid())
            return false;

        const Version currentVersion {QBT_VERSION_MAJOR, QBT_VERSION_MINOR, QBT_VERSION_BUGFIX, QBT_VERSION_BUILD};
        if (newVersion == currentVersion)
        {
            const bool isDevVersion = QStringLiteral(QBT_VERSION_STATUS).contains(
                QRegularExpression(u"(alpha|beta|rc)"_s));
            if (isDevVersion)
                return true;
        }
        return (newVersion > currentVersion);
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

void ProgramUpdater::checkForUpdates() const
{
    const auto RSS_URL = u"https://www.fosshub.com/feed/5b8793a7f9ee5a5c3e97a3b2.xml"_s;
    // Don't change this User-Agent. In case our updater goes haywire,
    // the filehost can identify it and contact us.
    Net::DownloadManager::instance()->download(
            Net::DownloadRequest(RSS_URL).userAgent(QStringLiteral("qBittorrent/" QBT_VERSION_2 " ProgramUpdater (www.qbittorrent.org)"))
            , Preferences::instance()->useProxyForGeneralPurposes(), this, &ProgramUpdater::rssDownloadFinished);
}

QString ProgramUpdater::getNewVersion() const
{
    return m_newVersion;
}

void ProgramUpdater::rssDownloadFinished(const Net::DownloadResult &result)
{
    if (result.status != Net::DownloadStatus::Success)
    {
        qDebug() << "Downloading the new qBittorrent updates RSS failed:" << result.errorString;
        emit updateCheckFinished();
        return;
    }

    qDebug("Finished downloading the new qBittorrent updates RSS");

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
                        if (isVersionMoreRecent(version))
                        {
                            m_newVersion = version;
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

    emit updateCheckFinished();
}

bool ProgramUpdater::updateProgram() const
{
    return QDesktopServices::openUrl(m_updateURL);
}
