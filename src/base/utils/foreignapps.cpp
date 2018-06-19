/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2018  Mike Tzou
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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

#include "foreignapps.h"

#include <QCoreApplication>
#include <QProcess>
#include <QStringList>

#include "base/logger.h"

using namespace Utils::ForeignApps;

namespace
{
    bool testPythonInstallation(const QString &exeName, PythonInfo &info)
    {
        QProcess proc;
        proc.start(exeName, {"--version"}, QIODevice::ReadOnly);
        if (proc.waitForFinished() && (proc.exitCode() == QProcess::NormalExit)) {
            QByteArray procOutput = proc.readAllStandardOutput();
            if (procOutput.isEmpty())
                procOutput = proc.readAllStandardError();
            procOutput = procOutput.simplified();

            // Software 'Anaconda' installs its own python interpreter
            // and `python --version` returns a string like this:
            // "Python 3.4.3 :: Anaconda 2.3.0 (64-bit)"
            const QList<QByteArray> outputSplit = procOutput.split(' ');
            if (outputSplit.size() <= 1)
                return false;

            try {
                info = {exeName, outputSplit[1]};
            }
            catch (const std::runtime_error &err) {
                return false;
            }

            LogMsg(QCoreApplication::translate("Utils::ForeignApps", "Python detected, version: %1").arg(info.version), Log::INFO);
            return true;
        }

        return false;
    }
}

bool Utils::ForeignApps::PythonInfo::isValid() const
{
    return (!executableName.isEmpty() && version.isValid());
}

PythonInfo Utils::ForeignApps::pythonInfo()
{
    static PythonInfo pyInfo;
    if (!pyInfo.isValid()) {
#if defined(Q_OS_UNIX)
        // On Unix-Like Systems python2 and python3 should always exist
        // https://legacy.python.org/dev/peps/pep-0394/
        if (testPythonInstallation("python3", pyInfo))
            return pyInfo;
        if (testPythonInstallation("python2", pyInfo))
            return pyInfo;
#endif
        // Look for "python" in Windows and in UNIX if "python2" and "python3" are
        // not detected.
        if (testPythonInstallation("python", pyInfo))
            return pyInfo;

        LogMsg(QCoreApplication::translate("Utils::ForeignApps", "Python not detected"), Log::INFO);
    }

    return pyInfo;
}
