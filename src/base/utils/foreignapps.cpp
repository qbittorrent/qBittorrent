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

#if defined(Q_OS_WIN)
#include <windows.h>
#endif

#include <QCoreApplication>
#include <QProcess>
#include <QRegularExpression>
#include <QStringList>

#if defined(Q_OS_WIN)
#include <QDir>
#endif

#include "base/logger.h"
#include "base/utils/bytearray.h"

using namespace Utils::ForeignApps;

namespace
{
    bool testPythonInstallation(const QString &exeName, PythonInfo &info)
    {
        QProcess proc;
        proc.start(exeName, {"--version"}, QIODevice::ReadOnly);
        if (proc.waitForFinished() && (proc.exitCode() == QProcess::NormalExit))
        {
            QByteArray procOutput = proc.readAllStandardOutput();
            if (procOutput.isEmpty())
                procOutput = proc.readAllStandardError();
            procOutput = procOutput.simplified();

            // Software 'Anaconda' installs its own python interpreter
            // and `python --version` returns a string like this:
            // "Python 3.4.3 :: Anaconda 2.3.0 (64-bit)"
            const QVector<QByteArray> outputSplit = Utils::ByteArray::splitToViews(procOutput, " ", QString::SkipEmptyParts);
            if (outputSplit.size() <= 1)
                return false;

            // User reports: `python --version` -> "Python 3.6.6+"
            // So trim off unrelated characters
            const QString versionStr = outputSplit[1];
            const int idx = versionStr.indexOf(QRegularExpression("[^\\.\\d]"));

            try
            {
                info = {exeName, versionStr.left(idx)};
            }
            catch (const std::runtime_error &)
            {
                return false;
            }

            LogMsg(QCoreApplication::translate("Utils::ForeignApps", "Python detected, executable name: '%1', version: %2")
                .arg(info.executableName, info.version), Log::INFO);
            return true;
        }

        return false;
    }

#if defined(Q_OS_WIN)
    enum REG_SEARCH_TYPE
    {
        USER,
        SYSTEM_32BIT,
        SYSTEM_64BIT
    };

    QStringList getRegSubkeys(const HKEY handle)
    {
        QStringList keys;

        DWORD cSubKeys = 0;
        DWORD cMaxSubKeyLen = 0;
        LONG res = ::RegQueryInfoKeyW(handle, NULL, NULL, NULL, &cSubKeys, &cMaxSubKeyLen, NULL, NULL, NULL, NULL, NULL, NULL);

        if (res == ERROR_SUCCESS)
        {
            ++cMaxSubKeyLen; // For null character
            LPWSTR lpName = new WCHAR[cMaxSubKeyLen];
            DWORD cName;

            for (DWORD i = 0; i < cSubKeys; ++i)
            {
                cName = cMaxSubKeyLen;
                res = ::RegEnumKeyExW(handle, i, lpName, &cName, NULL, NULL, NULL, NULL);
                if (res == ERROR_SUCCESS)
                    keys.push_back(QString::fromWCharArray(lpName));
            }

            delete[] lpName;
        }

        return keys;
    }

    QString getRegValue(const HKEY handle, const QString &name = {})
    {
        QString result;

        DWORD type = 0;
        DWORD cbData = 0;
        LPWSTR lpValueName = NULL;
        if (!name.isEmpty())
        {
            lpValueName = new WCHAR[name.size() + 1];
            name.toWCharArray(lpValueName);
            lpValueName[name.size()] = 0;
        }

        // Discover the size of the value
        ::RegQueryValueExW(handle, lpValueName, NULL, &type, NULL, &cbData);
        DWORD cBuffer = (cbData / sizeof(WCHAR)) + 1;
        LPWSTR lpData = new WCHAR[cBuffer];
        LONG res = ::RegQueryValueExW(handle, lpValueName, NULL, &type, (LPBYTE)lpData, &cbData);
        if (lpValueName)
            delete[] lpValueName;

        if (res == ERROR_SUCCESS)
        {
            lpData[cBuffer - 1] = 0;
            result = QString::fromWCharArray(lpData);
        }
        delete[] lpData;

        return result;
    }

    QString pythonSearchReg(const REG_SEARCH_TYPE type)
    {
        HKEY hkRoot;
        if (type == USER)
            hkRoot = HKEY_CURRENT_USER;
        else
            hkRoot = HKEY_LOCAL_MACHINE;

        REGSAM samDesired = KEY_READ;
        if (type == SYSTEM_32BIT)
            samDesired |= KEY_WOW64_32KEY;
        else if (type == SYSTEM_64BIT)
            samDesired |= KEY_WOW64_64KEY;

        QString path;
        LONG res = 0;
        HKEY hkPythonCore;
        res = ::RegOpenKeyExW(hkRoot, L"SOFTWARE\\Python\\PythonCore", 0, samDesired, &hkPythonCore);

        if (res == ERROR_SUCCESS)
        {
            QStringList versions = getRegSubkeys(hkPythonCore);
            qDebug("Python versions nb: %d", versions.size());
            versions.sort();

            bool found = false;
            while (!found && !versions.empty())
            {
                const QString version = versions.takeLast() + "\\InstallPath";
                LPWSTR lpSubkey = new WCHAR[version.size() + 1];
                version.toWCharArray(lpSubkey);
                lpSubkey[version.size()] = 0;

                HKEY hkInstallPath;
                res = ::RegOpenKeyExW(hkPythonCore, lpSubkey, 0, samDesired, &hkInstallPath);
                delete[] lpSubkey;

                if (res == ERROR_SUCCESS)
                {
                    qDebug("Detected possible Python v%s location", qUtf8Printable(version));
                    path = getRegValue(hkInstallPath);
                    ::RegCloseKey(hkInstallPath);

                    if (!path.isEmpty())
                    {
                        const QDir baseDir {path};

                        if (baseDir.exists("python3.exe"))
                        {
                            found = true;
                            path = baseDir.filePath("python3.exe");
                        }
                        else if (baseDir.exists("python.exe"))
                        {
                            found = true;
                            path = baseDir.filePath("python.exe");
                        }
                    }
                }
            }

            if (!found)
                path = QString();

            ::RegCloseKey(hkPythonCore);
        }

        return path;
    }

    QString findPythonPath()
    {
        QString path = pythonSearchReg(USER);
        if (!path.isEmpty())
            return path;

        path = pythonSearchReg(SYSTEM_32BIT);
        if (!path.isEmpty())
            return path;

        path = pythonSearchReg(SYSTEM_64BIT);
        if (!path.isEmpty())
            return path;

        // Fallback: Detect python from default locations
        const QFileInfoList dirs = QDir("C:/").entryInfoList({"Python*"}, QDir::Dirs, (QDir::Name | QDir::Reversed));
        for (const QFileInfo &info : dirs)
        {
            const QString py3Path {info.absolutePath() + "/python3.exe"};
            if (QFile::exists(py3Path))
                return py3Path;

            const QString pyPath {info.absolutePath() + "/python.exe"};
            if (QFile::exists(pyPath))
                return pyPath;
        }

        return {};
    }
#endif // Q_OS_WIN
}

bool Utils::ForeignApps::PythonInfo::isValid() const
{
    return (!executableName.isEmpty() && version.isValid());
}

bool Utils::ForeignApps::PythonInfo::isSupportedVersion() const
{
    return (version >= Version {3, 5, 0});
}

PythonInfo Utils::ForeignApps::pythonInfo()
{
    static PythonInfo pyInfo;
    if (!pyInfo.isValid())
    {
        if (testPythonInstallation("python3", pyInfo))
            return pyInfo;

        if (testPythonInstallation("python", pyInfo))
            return pyInfo;

#if defined(Q_OS_WIN)
        if (testPythonInstallation(findPythonPath(), pyInfo))
            return pyInfo;
#endif

        LogMsg(QCoreApplication::translate("Utils::ForeignApps", "Python not detected"), Log::INFO);
    }

    return pyInfo;
}
