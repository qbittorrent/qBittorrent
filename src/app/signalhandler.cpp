/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2022  Mike Tzou (Chocobo1)
 * Copyright (C) 2014  Vladimir Golovnev <glassez@yandex.ru>
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

#include "signalhandler.h"

#include <QtSystemDetection>

#include <algorithm>
#include <csignal>

#ifdef Q_OS_UNIX
#include <unistd.h>
#elif defined Q_OS_WIN
#include <io.h>
#endif

#include <QCoreApplication>
#include <QMetaObject>

#include "base/version.h"

#ifdef STACKTRACE
#include "stacktrace.h"

#ifndef DISABLE_GUI
#include "gui/stacktracedialog.h"
#endif
#endif //STACKTRACE

namespace
{
    // sys_signame[] is only defined in BSD
    const char *const sysSigName[] =
    {
#ifdef Q_OS_WIN
        "", "", "SIGINT", "", "SIGILL", "", "SIGABRT_COMPAT", "", "SIGFPE", "",
        "", "SIGSEGV", "", "", "", "SIGTERM", "", "", "", "",
        "", "SIGBREAK", "SIGABRT", "", "", "", "", "", "", "",
        "", ""
#else
        "", "SIGHUP", "SIGINT", "SIGQUIT", "SIGILL", "SIGTRAP", "SIGABRT", "SIGBUS", "SIGFPE", "SIGKILL",
        "SIGUSR1", "SIGSEGV", "SIGUSR2", "SIGPIPE", "SIGALRM", "SIGTERM", "SIGSTKFLT", "SIGCHLD", "SIGCONT", "SIGSTOP",
        "SIGTSTP", "SIGTTIN", "SIGTTOU", "SIGURG", "SIGXCPU", "SIGXFSZ", "SIGVTALRM", "SIGPROF", "SIGWINCH", "SIGIO",
        "SIGPWR", "SIGUNUSED"
#endif
    };

    void safePrint(const char *str)
    {
        const size_t strLen = strlen(str);
#ifdef Q_OS_WIN
        if (_write(_fileno(stderr), str, static_cast<unsigned int>(strLen)) < static_cast<int>(strLen))
            std::ignore = _write(_fileno(stdout), str, static_cast<unsigned int>(strLen));
#else
        if (write(STDERR_FILENO, str, strLen) < static_cast<ssize_t>(strLen))
            std::ignore = write(STDOUT_FILENO, str, strLen);
#endif
    }

    void normalExitHandler(const int signum)
    {
        const char *msgs[] = {"Catching signal: ", sysSigName[signum], "\nExiting cleanly\n"};
        std::for_each(std::begin(msgs), std::end(msgs), safePrint);
        signal(signum, SIG_DFL);
        QMetaObject::invokeMethod(qApp, [] { QCoreApplication::exit(); }, Qt::QueuedConnection);  // unsafe, but exit anyway
    }

#ifdef STACKTRACE
    void abnormalExitHandler(const int signum)
    {
        const char msg[] = "\n\n*************************************************************\n"
            "Please file a bug report at https://bug.qbittorrent.org and provide the following information:\n\n"
            "qBittorrent version: " QBT_VERSION "\n\n"
            "Caught signal: ";
        const char *sigName = sysSigName[signum];
        const std::string stacktrace = getStacktrace();

        const char *msgs[] = {msg, sigName, "\n```\n", stacktrace.c_str(), "```\n\n"};
        std::for_each(std::begin(msgs), std::end(msgs), safePrint);

#ifndef DISABLE_GUI
        StacktraceDialog dlg;  // unsafe
        dlg.setText(QString::fromLatin1(sigName), QString::fromStdString(stacktrace));
        dlg.exec();
#endif

        signal(signum, SIG_DFL);
        raise(signum);
    }
#endif // STACKTRACE
}

void registerSignalHandlers()
{
    signal(SIGINT, normalExitHandler);
    signal(SIGTERM, normalExitHandler);

#ifdef STACKTRACE
    signal(SIGABRT, abnormalExitHandler);
    signal(SIGSEGV, abnormalExitHandler);
#endif
}
