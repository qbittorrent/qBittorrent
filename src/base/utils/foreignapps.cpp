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

#include "base/global.h"
#include "base/logger.h"
#include "base/path.h"
#include "base/preferences.h"
#include "base/utils/bytearray.h"

using namespace Utils::ForeignApps;

namespace
{
    bool testPythonInstallation(const QString &exeName, PythonInfo &info)
    {
        info = {};

        QProcess proc;
        proc.start(exeName, {u"--version"_s}, QIODevice::ReadOnly);
        if (proc.waitForFinished() && (proc.exitCode() == QProcess::NormalExit))
        {
            QByteArray procOutput = proc.readAllStandardOutput();
            if (procOutput.isEmpty())
                procOutput = proc.readAllStandardError();
            procOutput = procOutput.simplified();

            // Software 'Anaconda' installs its own python interpreter
            // and `python --version` returns a string like this:
            // "Python 3.4.3 :: Anaconda 2.3.0 (64-bit)"
            const QList<QByteArrayView> outputSplit = Utils::ByteArray::splitToViews(procOutput, " ", Qt::SkipEmptyParts);
            if (outputSplit.size() <= 1)
                return false;

            // User reports: `python --version` -> "Python 3.6.6+"
            // So trim off unrelated characters
            const auto versionStr = QString::fromLocal8Bit(outputSplit[1]);
            const int idx = versionStr.indexOf(QRegularExpression(u"[^\\.\\d]"_s));
            const auto version = PythonInfo::Version::fromString(versionStr.left(idx));
            if (!version.isValid())
                return false;

            info = {exeName, version};
            LogMsg(QCoreApplication::translate("Utils::ForeignApps", "Found Python executable. Name: \"%1\". Version: \"%2\"")
                .arg(info.executableName, info.version.toString()), Log::INFO);
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
        const std::wstring nameWStr = name.toStdWString();
        DWORD type = 0;
        DWORD cbData = 0;

        // Discover the size of the value
        ::RegQueryValueExW(handle, nameWStr.c_str(), NULL, &type, NULL, &cbData);
        DWORD cBuffer = (cbData / sizeof(WCHAR)) + 1;
        LPWSTR lpData = new WCHAR[cBuffer];
        LONG res = ::RegQueryValueExW(handle, nameWStr.c_str(), NULL, &type, reinterpret_cast<LPBYTE>(lpData), &cbData);

        QString result;
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
            versions.sort();

            bool found = false;
            while (!found && !versions.empty())
            {
                const std::wstring version = QString(versions.takeLast() + u"\\InstallPath").toStdWString();

                HKEY hkInstallPath;
                res = ::RegOpenKeyExW(hkPythonCore, version.c_str(), 0, samDesired, &hkInstallPath);

                if (res == ERROR_SUCCESS)
                {
                    qDebug("Detected possible Python v%ls location", version.c_str());
                    path = getRegValue(hkInstallPath);
                    ::RegCloseKey(hkInstallPath);

                    if (!path.isEmpty())
                    {
                        const QDir baseDir {path};

                        if (baseDir.exists(u"python3.exe"_s))
                        {
                            found = true;
                            path = baseDir.filePath(u"python3.exe"_s);
                        }
                        else if (baseDir.exists(u"python.exe"_s))
                        {
                            found = true;
                            path = baseDir.filePath(u"python.exe"_s);
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
        const QFileInfoList dirs = QDir(u"C:/"_s).entryInfoList({u"Python*"_s}, QDir::Dirs, (QDir::Name | QDir::Reversed));
        for (const QFileInfo &info : dirs)
        {
            const QString py3Path {info.absolutePath() + u"/python3.exe"};
            if (QFile::exists(py3Path))
                return py3Path;

            const QString pyPath {info.absolutePath() + u"/python.exe"};
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
    return (version >= Version {3, 9, 0});
}

PythonInfo Utils::ForeignApps::pythonInfo()
{
    static PythonInfo pyInfo;

    const QString preferredPythonPath = Preferences::instance()->getPythonExecutablePath().toString();
    if (pyInfo.isValid() && (preferredPythonPath == pyInfo.executableName))
        return pyInfo;

    if (!preferredPythonPath.isEmpty())
    {
        if (testPythonInstallation(preferredPythonPath, pyInfo))
            return pyInfo;
        LogMsg(QCoreApplication::translate("Utils::ForeignApps", "Failed to find Python executable. Path: \"%1\".")
            .arg(preferredPythonPath), Log::WARNING);
    }
    else
    {
        // auto detect only when there are no preferred python path

        if (!pyInfo.isValid())
        {
            if (testPythonInstallation(u"python3"_s, pyInfo))
                return pyInfo;
            LogMsg(QCoreApplication::translate("Utils::ForeignApps", "Failed to find `python3` executable in PATH environment variable. PATH: \"%1\"")
                .arg(qEnvironmentVariable("PATH")), Log::INFO);

            if (testPythonInstallation(u"python"_s, pyInfo))
                return pyInfo;
            LogMsg(QCoreApplication::translate("Utils::ForeignApps", "Failed to find `python` executable in PATH environment variable. PATH: \"%1\"")
                .arg(qEnvironmentVariable("PATH")), Log::INFO);

#if defined(Q_OS_WIN)
            if (testPythonInstallation(findPythonPath(), pyInfo))
                return pyInfo;
            LogMsg(QCoreApplication::translate("Utils::ForeignApps", "Failed to find `python` executable in Windows Registry."), Log::INFO);
#endif

            LogMsg(QCoreApplication::translate("Utils::ForeignApps", "Failed to find Python executable"), Log::WARNING);

        }
    }

    return pyInfo;
}
