/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016  Eugene Shalygin <eugene.shalygin@gmail.com>
 * Copyright (C) 2014  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez
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
 *
 * Contact : chris@qbittorrent.org
 */

#include "options.h"

#include <iostream>
#include <QFileInfo>

#ifdef Q_OS_WIN
#include <QMessageBox>
#endif

#include "base/utils/misc.h"

QBtCommandLineParameters::QBtCommandLineParameters()
    : showHelp(false)
#ifndef Q_OS_WIN
    , showVersion(false)
#endif
#ifndef DISABLE_GUI
    , noSplash(false)
#else
    , shouldDaemonize(false)
#endif
    , webUiPort(-1)
    , profileDir()
    , portableMode(false)
    , configurationName()
{
}

QBtCommandLineParameters parseCommandLine(const QStringList &args)
{
    QBtCommandLineParameters result;

    for (int i = 1; i < args.count(); ++i) {
        const QString &arg = args[i];

        if ((arg.startsWith("--") && !arg.endsWith(".torrent"))
            || (arg.startsWith("-") && (arg.size() == 2))) {
            // Parse known parameters
            if ((arg == QLatin1String("-h")) || (arg == QLatin1String("--help"))) {
                result.showHelp = true;
            }
#ifndef Q_OS_WIN
            else if ((arg == QLatin1String("-v")) || (arg == QLatin1String("--version"))) {
                result.showVersion = true;
            }
#endif
            else if (arg.startsWith(QLatin1String("--webui-port="))) {
                QStringList parts = arg.split(QLatin1Char('='));
                if (parts.size() == 2) {
                    bool ok = false;
                    result.webUiPort = parts.last().toInt(&ok);
                    if (!ok || (result.webUiPort < 1) || (result.webUiPort > 65535))
                        throw CommandLineParameterError(QObject::tr("%1 must specify the correct port (1 to 65535).")
                                                        .arg(QLatin1String("--webui-port")));
                }
            }
#ifndef DISABLE_GUI
            else if (arg == QLatin1String("--no-splash")) {
                result.noSplash = true;
            }
#else
            else if ((arg == QLatin1String("-d")) || (arg == QLatin1String("--daemon"))) {
                result.shouldDaemonize = true;
            }
#endif
            else if (arg == QLatin1String("--profile")) {
                QStringList parts = arg.split(QLatin1Char('='));
                if (parts.size() == 2)
                    result.profileDir = parts.last();
            }
            else if (arg == QLatin1String("--portable")) {
                result.portableMode = true;
            }
            else if (arg == QLatin1String("--configuration")) {
                QStringList parts = arg.split(QLatin1Char('='));
                if (parts.size() == 2)
                    result.configurationName = parts.last();
            }
            else {
                // Unknown argument
                result.unknownParameter = arg;
                break;
            }
        }
        else {
            QFileInfo torrentPath;
            torrentPath.setFile(arg);

            if (torrentPath.exists())
                result.torrents += torrentPath.absoluteFilePath();
            else
                result.torrents += arg;
        }
    }

    return result;
}

CommandLineParameterError::CommandLineParameterError(const QString &messageForUser)
    : std::runtime_error(messageForUser.toLocal8Bit().data())
    , m_messageForUser(messageForUser)
{
}

const QString& CommandLineParameterError::messageForUser() const
{
    return m_messageForUser;
}

QString makeUsage(const QString &prgName)
{
    QString text;

    text += QObject::tr("Usage:") + QLatin1Char('\n');
#ifndef Q_OS_WIN
    text += QLatin1Char('\t') + prgName + QLatin1String(" (-v | --version)") + QLatin1Char('\n');
#endif
    text += QLatin1Char('\t') + prgName + QLatin1String(" (-h | --help)") + QLatin1Char('\n');
    text += QLatin1Char('\t') + prgName
            + QLatin1String(" [--webui-port=<port>]")
#ifndef DISABLE_GUI
            + QLatin1String(" [--no-splash]")
#else
            + QLatin1String(" [-d | --daemon]")
#endif
            + QLatin1String("[(<filename> | <url>)...]") + QLatin1Char('\n');
    text += QObject::tr("Options:") + QLatin1Char('\n');
#ifndef Q_OS_WIN
    text += QLatin1String("\t-v | --version\t\t") + QObject::tr("Displays program version") + QLatin1Char('\n');
#endif
    text += QLatin1String("\t-h | --help\t\t") + QObject::tr("Displays this help message") + QLatin1Char('\n');
    text += QLatin1String("\t--webui-port=<port>\t")
            + QObject::tr("Changes the Web UI port")
            + QLatin1Char('\n');
#ifndef DISABLE_GUI
    text += QLatin1String("\t--no-splash\t\t") + QObject::tr("Disable splash screen") + QLatin1Char('\n');
#else
    text += QLatin1String("\t-d | --daemon\t\t") + QObject::tr("Run in daemon-mode (background)") + QLatin1Char('\n');
#endif
    text += QLatin1String("\t--profile=<dir>\t\t") + QObject::tr("Store configuration files in <dir>") + QLatin1Char('\n');
    text += QLatin1String("\t--portable\t\t") + QObject::tr("Shortcut for --profile=<exe dir>/profile") + QLatin1Char('\n');
    text += QLatin1String("\t--configuration=<name>\t\t") + QObject::tr("Store configuration files in directories qBittorrent_<name>")
            + QLatin1Char('\n');
    text += QLatin1String("\tfiles or urls\t\t") + QObject::tr("Downloads the torrents passed by the user");

    return text;
}

void displayUsage(const QString& prgName)
{
#ifndef Q_OS_WIN
    std::cout << qPrintable(makeUsage(prgName)) << std::endl;
#else
    QMessageBox msgBox(QMessageBox::Information, QObject::tr("Help"), makeUsage(prgName), QMessageBox::Ok);
    msgBox.show(); // Need to be shown or to moveToCenter does not work
    msgBox.move(Utils::Misc::screenCenter(&msgBox));
    msgBox.exec();
#endif
}

