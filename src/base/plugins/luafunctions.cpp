/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Vladimir Golovnev <glassez@yandex.ru>
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

#include "luafunctions.h"

#include <QtSystemDetection>
#if defined(Q_OS_WIN)
#include <windows.h>
#include <shellapi.h>
#endif

#include <LuaBridge/LuaBridge.h>

#include <QDebug>
#include <QProcess>
#include <QString>
#include <QStringList>

#include "base/logger.h"
#include "base/net/smtpclient.h"
#include "base/plugins/pluginsengine.h"
#include "base/preferences.h"
#include "base/utils/misc.h"
#include "luanamespace.h"
#include "luastack.h"

#ifndef Q_OS_WIN
#include "base/utils/string.h"
#endif

using namespace Qt::Literals::StringLiterals;

namespace LuaFunctions
{
    void debug(const QString &message)
    {
        qDebug() << message;
    }

    void log(const QString &message)
    {
        LogMsg(message);
    }

    bool exec(const QString &command)
    {
    // The processing sequence is different for Windows and other OS, this is intentional
    #ifdef Q_OS_WIN
        const std::wstring programWStr = command.toStdWString();

        // Need to split arguments manually because QProcess::startDetached(QString)
        // will strip off empty parameters.
        // E.g. `python.exe "1" "" "3"` will become `python.exe "1" "3"`
        int argCount = 0;
        std::unique_ptr<LPWSTR[], decltype(&::LocalFree)> args {::CommandLineToArgvW(programWStr.c_str(), &argCount), ::LocalFree};

        if (argCount <= 0)
            return false;

        QStringList argList;
        for (int i = 1; i < argCount; ++i)
            argList += QString::fromWCharArray(args[i]);

        QProcess proc;
        proc.setProgram(QString::fromWCharArray(args[0]));
        proc.setArguments(argList);
        proc.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args)
        {
            if (true/*Preferences::instance()->isAutoRunConsoleEnabled()*/)
            {
                args->flags |= CREATE_NEW_CONSOLE;
                args->flags &= ~(CREATE_NO_WINDOW | DETACHED_PROCESS);
            }
            else
            {
                args->flags |= CREATE_NO_WINDOW;
                args->flags &= ~(CREATE_NEW_CONSOLE | DETACHED_PROCESS);
            }
            args->inheritHandles = false;
            args->startupInfo->dwFlags &= ~STARTF_USESTDHANDLES;
            ::CloseHandle(args->startupInfo->hStdInput);
            ::CloseHandle(args->startupInfo->hStdOutput);
            ::CloseHandle(args->startupInfo->hStdError);
            args->startupInfo->hStdInput = nullptr;
            args->startupInfo->hStdOutput = nullptr;
            args->startupInfo->hStdError = nullptr;
        });

        if (proc.startDetached())
            return true;
    #else // Q_OS_WIN
        QStringList args = Utils::String::splitCommand(command);

        if (args.isEmpty())
            return false;

        for (QString &arg : args)
        {
            // strip redundant quotes
            if (arg.startsWith(u'"') && arg.endsWith(u'"'))
                arg = arg.mid(1, (arg.size() - 2));
        }

        const QString exe = args.takeFirst();
        QProcess proc;
        proc.setProgram(exe);
        proc.setArguments(args);

        if (proc.startDetached())
            return true;
    #endif
        return false;
    }

    void sendMail(const QString &subject, const QString &body)
    {
        const auto *pref = Preferences::instance();
        Net::SMTPClient::sendMail(pref->getMailNotificationSender(), pref->getMailNotificationEmail()
                , subject, body, PluginsEngine::instance());
    }

    QString friendlySizeUnit1(const qint64 bytes)
    {
        return Utils::Misc::friendlyUnit(bytes, false, -1);
    }

    QString friendlySizeUnit2(const qint64 bytes, const int precision)
    {
        return Utils::Misc::friendlyUnit(bytes, false, precision);
    }

    QString friendlySpeedUnit1(const qint64 bytes)
    {
        return Utils::Misc::friendlyUnit(bytes, true, -1);
    }

    QString friendlySpeedUnit2(const qint64 bytes, const int precision)
    {
        return Utils::Misc::friendlyUnit(bytes, true, precision);
    }

    QString friendlyDuration(const qint64 seconds)
    {
        return Utils::Misc::userFriendlyDuration(seconds);
    }
}

void registerLuaFunctions(lua_State *luaState)
{
    auto qBittorrentNS = luabridge::getGlobalNamespace(luaState).beginNamespace(QBT_NAMESPACE);
    qBittorrentNS.addFunction("debug", LuaFunctions::debug);
    qBittorrentNS.addFunction("log", LuaFunctions::log);
    qBittorrentNS.addFunction("exec", LuaFunctions::exec);
    qBittorrentNS.addFunction("sendMail", LuaFunctions::sendMail);
    qBittorrentNS.addFunction("friendlySizeUnit", LuaFunctions::friendlySizeUnit1, LuaFunctions::friendlySizeUnit2);
    qBittorrentNS.addFunction("friendlySpeedUnit", LuaFunctions::friendlySpeedUnit1, LuaFunctions::friendlySpeedUnit2);
    qBittorrentNS.addFunction("friendlyDuration", LuaFunctions::friendlyDuration);
}
