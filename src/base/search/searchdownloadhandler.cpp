/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2018-2024  Vladimir Golovnev <glassez@yandex.ru>
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

#include "searchdownloadhandler.h"

#include <QProcess>

#include "base/global.h"
#include "base/path.h"
#include "base/utils/foreignapps.h"
#include "base/utils/fs.h"
#include "searchpluginmanager.h"

SearchDownloadHandler::SearchDownloadHandler(const QString &pluginName, const QString &url, SearchPluginManager *manager)
    : QObject(manager)
    , m_manager {manager}
    , m_downloadProcess {new QProcess(this)}
{
    m_downloadProcess->setEnvironment(QProcess::systemEnvironment());
    connect(m_downloadProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished)
            , this, &SearchDownloadHandler::downloadProcessFinished);
    const QStringList params
    {
        Utils::ForeignApps::PYTHON_ISOLATE_MODE_FLAG,
        (SearchPluginManager::engineLocation() / Path(u"nova2dl.py"_s)).toString(),
        pluginName,
        url
    };
    // Launch search
    m_downloadProcess->start(Utils::ForeignApps::pythonInfo().executableName, params, QIODevice::ReadOnly);
}

void SearchDownloadHandler::downloadProcessFinished(int exitcode)
{
    QString path;

    if ((exitcode == 0) && (m_downloadProcess->exitStatus() == QProcess::NormalExit))
    {
        const QString line = QString::fromUtf8(m_downloadProcess->readAllStandardOutput()).trimmed();
        const QList<QStringView> parts = QStringView(line).split(u' ');
        if (parts.size() == 2)
            path = parts[0].toString();
    }

    emit downloadFinished(path);
}
