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
#include <algorithm>
#include <windows.h>
#endif

#include <QCoreApplication>
#include <QProcess>
#include <QRegularExpression>
#include <QStringList>

#if defined(Q_OS_WIN)
#include <QDir>
#include <QScopeGuard>
#endif

#include "base/logger.h"
#include "base/path.h"
#include "base/preferences.h"
#include "base/utils/bytearray.h"

#if defined(Q_OS_WIN)
#include "base/utils/compare.h"
#endif

using namespace Utils::ForeignApps;

namespace
{
    bool testPythonInstallation(const QString &exeName, PythonInfo &info)
    {
        info = {};

        QProcess proc;
#ifdef Q_OS_UNIX
        proc.setUnixProcessParameters(QProcess::UnixProcessFlag::CloseFileDescriptors);
#endif
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
            const QList<QByteArrayView> outputSplit = Utils::ByteArray::splitToViews(procOutput, " ");
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
        const LSTATUS result = ::RegQueryInfoKeyW(handle, NULL, NULL, NULL, &cSubKeys, &cMaxSubKeyLen, NULL, NULL, NULL, NULL, NULL, NULL);

        if (result == ERROR_SUCCESS)
        {
            ++cMaxSubKeyLen; // For null character
            LPWSTR lpName = new WCHAR[cMaxSubKeyLen];
            [[maybe_unused]] const auto lpNameGuard = qScopeGuard([&lpName] { delete[] lpName; });

            keys.reserve(cSubKeys);

            for (DWORD i = 0; i < cSubKeys; ++i)
            {
                DWORD cName = cMaxSubKeyLen;
                const LSTATUS res = ::RegEnumKeyExW(handle, i, lpName, &cName, NULL, NULL, NULL, NULL);
                if (res == ERROR_SUCCESS)
                    keys.append(QString::fromWCharArray(lpName));
            }
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

        const DWORD cBuffer = (cbData / sizeof(WCHAR)) + 1;
        LPWSTR lpData = new WCHAR[cBuffer]{0};
        [[maybe_unused]] const auto lpDataGuard = qScopeGuard([&lpData] { delete[] lpData; });

        const LSTATUS res = ::RegQueryValueExW(handle, nameWStr.c_str(), NULL, &type, reinterpret_cast<LPBYTE>(lpData), &cbData);
        if (res == ERROR_SUCCESS)
            return QString::fromWCharArray(lpData);

        return {};
    }

    QStringList pythonSearchReg(const REG_SEARCH_TYPE type)
    {
        const HKEY hkRoot = (type == USER) ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
        const REGSAM samDesired = KEY_READ
            | ((type == SYSTEM_64BIT) ? KEY_WOW64_64KEY : KEY_WOW64_32KEY);
        QStringList ret;

        HKEY hkPythonCore {0};
        if (::RegOpenKeyExW(hkRoot, L"SOFTWARE\\Python\\PythonCore", 0, samDesired, &hkPythonCore) == ERROR_SUCCESS)
        {
            [[maybe_unused]] const auto hkPythonCoreGuard = qScopeGuard([&hkPythonCore] { ::RegCloseKey(hkPythonCore); });

            // start with the largest version
            QStringList versions = getRegSubkeys(hkPythonCore);
            // ordinary sort won't suffice, it needs to sort ["3.9", "3.10"] correctly
            std::sort(versions.begin(), versions.end(), Utils::Compare::NaturalLessThan<Qt::CaseInsensitive>());
            ret.reserve(versions.size() * 2);

            while (!versions.empty())
            {
                const std::wstring version = QString(versions.takeLast() + u"\\InstallPath").toStdWString();

                HKEY hkInstallPath {0};
                if (::RegOpenKeyExW(hkPythonCore, version.c_str(), 0, samDesired, &hkInstallPath) == ERROR_SUCCESS)
                {
                    [[maybe_unused]] const auto hkInstallPathGuard = qScopeGuard([&hkInstallPath] { ::RegCloseKey(hkInstallPath); });

                    const QString basePath = getRegValue(hkInstallPath);
                    if (basePath.isEmpty())
                        continue;

                    if (const QString path = (basePath + u"python3.exe"); QFile::exists(path))
                        ret.append(path);
                    if (const QString path = (basePath + u"python.exe"); QFile::exists(path))
                        ret.append(path);
                }
            }
        }

        return ret;
    }

    QStringList searchPythonPaths()
    {
        QStringList ret;

        // From registry
        ret.append(pythonSearchReg(USER));
        ret.append(pythonSearchReg(SYSTEM_64BIT));
        ret.append(pythonSearchReg(SYSTEM_32BIT));

        // Fallback: Detect python from default locations
        const QFileInfoList dirs = QDir(u"C:/"_s).entryInfoList({u"Python*"_s}, QDir::Dirs, (QDir::Name | QDir::Reversed));
        for (const QFileInfo &info : dirs)
        {
            const QString absPath = info.absolutePath();

            if (const QString path = (absPath + u"/python3.exe"); QFile::exists(path))
                ret.append(path);
            if (const QString path = (absPath + u"/python.exe"); QFile::exists(path))
                ret.append(path);
        }

        return ret;
    }
#endif // Q_OS_WIN
}

bool Utils::ForeignApps::PythonInfo::isValid() const
{
    return (!executableName.isEmpty() && version.isValid());
}

bool Utils::ForeignApps::PythonInfo::isSupportedVersion() const
{
    return (version >= MINIMUM_SUPPORTED_VERSION);
}

PythonInfo Utils::ForeignApps::pythonInfo()
{
    static PythonInfo pyInfo;

    const QString preferredPythonPath = Preferences::instance()->getPythonExecutablePath().toString();
    if (pyInfo.isValid() && (preferredPythonPath == pyInfo.executableName))
        return pyInfo;

    const QString invalidVersionMessage = QCoreApplication::translate("Utils::ForeignApps"
            , "Python failed to meet minimum version requirement. Path: \"%1\". Found version: \"%2\". Minimum supported version: \"%3\".");

    if (!preferredPythonPath.isEmpty())
    {
        if (testPythonInstallation(preferredPythonPath, pyInfo))
        {
            if (pyInfo.isSupportedVersion())
                return pyInfo;

            LogMsg(invalidVersionMessage.arg(pyInfo.executableName, pyInfo.version.toString(), PythonInfo::MINIMUM_SUPPORTED_VERSION.toString()), Log::WARNING);
        }
        else
        {
            LogMsg(QCoreApplication::translate("Utils::ForeignApps", "Failed to find Python executable. Path: \"%1\".")
                .arg(preferredPythonPath), Log::WARNING);
        }
    }
    else
    {
        // auto detect only when there are no preferred python path

        if (!pyInfo.isValid())
        {
            const QString exeNames[] = {u"python3"_s, u"python"_s};
            for (const QString &exeName : exeNames)
            {
                if (testPythonInstallation(exeName, pyInfo))
                {
                    if (pyInfo.isSupportedVersion())
                        return pyInfo;

                    LogMsg(invalidVersionMessage.arg(pyInfo.executableName, pyInfo.version.toString(), PythonInfo::MINIMUM_SUPPORTED_VERSION.toString()), Log::INFO);
                }
                else
                {
                    LogMsg(QCoreApplication::translate("Utils::ForeignApps", "Failed to find `%1` executable in PATH environment variable. PATH: \"%2\"")
                        .arg(exeName, qEnvironmentVariable("PATH")), Log::INFO);
                }
            }

#if defined(Q_OS_WIN)
            for (const QString &path : asConst(searchPythonPaths()))
            {
                if (testPythonInstallation(path, pyInfo))
                {
                    if (pyInfo.isSupportedVersion())
                        return pyInfo;

                    LogMsg(invalidVersionMessage.arg(pyInfo.executableName, pyInfo.version.toString(), PythonInfo::MINIMUM_SUPPORTED_VERSION.toString()), Log::INFO);
                }
            }
#endif

            LogMsg(QCoreApplication::translate("Utils::ForeignApps", "Failed to find Python executable"), Log::WARNING);
        }
    }

    return pyInfo;
}
