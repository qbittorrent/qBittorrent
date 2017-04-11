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

#include <QDebug>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QTextStream>

#ifdef Q_OS_WIN
#include <QMessageBox>
#endif

#include "base/utils/misc.h"

namespace
{
    // Base option class. Encapsulates name operations.
    class Option
    {
    protected:
        constexpr Option(const char *name, char shortcut = 0)
            : m_name {name}
            , m_shortcut {shortcut}
        {
        }

        QString fullParameter() const
        {
            return QLatin1String("--") + QLatin1String(m_name);
        }

        QString shortcutParameter() const
        {
            return QLatin1String("-") + QLatin1Char(m_shortcut);
        }

        bool hasShortcut() const
        {
            return m_shortcut != 0;
        }

        QString envVarName() const
        {
            return QLatin1String("QBT_")
                   + QString(QLatin1String(m_name)).toUpper().replace(QLatin1Char('-'), QLatin1Char('_'));
        }

        static QString padUsageText(const QString &usage)
        {
            const int TAB_WIDTH = 8;
            QString res = QLatin1String("\t") + usage;
            if (usage.size() < 2 * TAB_WIDTH)
                return res + QLatin1String("\t\t");
            else
                return res + QLatin1String("\t");
        }

    private:
        const char *m_name;
        const char m_shortcut;
    };

    // Boolean option.
    class BoolOption: protected Option
    {
    public:
        constexpr BoolOption(const char *name, char shortcut = 0)
            : Option {name, shortcut}
        {
        }

        bool operator==(const QString &arg) const
        {
            return (hasShortcut() && ((arg.size() == 2) && (arg == shortcutParameter())))
                   || (arg == fullParameter());
        }

        bool value(const QProcessEnvironment &env) const
        {
            QString val = env.value(envVarName());
            // we accept "1" and "true" (upper or lower cased) as boolean 'true' values
            return (val == QLatin1String("1") || val.toUpper() == QLatin1String("TRUE"));
        }

        QString usage() const
        {
            QString res;
            if (hasShortcut())
                res += shortcutParameter() + QLatin1String(" | ");
            res += fullParameter();
            return padUsageText(res);
        }
    };

    inline bool operator==(const QString &s, const BoolOption &o)
    {
        return o == s;
    }

    // Option with string value. May not have a shortcut
    struct StringOption: protected Option
    {
    public:
        constexpr StringOption(const char *name)
            : Option {name, 0}
        {
        }

        bool operator==(const QString &arg) const
        {
            return arg.startsWith(parameterAssignment());
        }

        QString value(const QString &arg) const
        {
            QStringList parts = arg.split(QLatin1Char('='));
            if (parts.size() == 2)
                return unquote(parts[1]);
            throw CommandLineParameterError(QObject::tr("Parameter '%1' must follow syntax '%1=%2'",
                                                        "e.g. Parameter '--webui-port' must follow syntax '--webui-port=value'")
                                            .arg(fullParameter()).arg(QLatin1String("<value>")));
        }

        QString value(const QProcessEnvironment &env, const QString &defaultValue = QString()) const
        {
            QString val = env.value(envVarName());
            return val.isEmpty() ? defaultValue : unquote(val);
        }

        QString usage(const QString &valueName) const
        {
            return padUsageText(parameterAssignment() + QLatin1Char('<') + valueName + QLatin1Char('>'));
        }

    private:
        QString parameterAssignment() const
        {
            return fullParameter() + QLatin1Char('=');
        }

        static QString unquote(const QString &s)
        {
            auto isStringQuoted =
                [](const QString &s, QChar quoteChar)
                {
                    return (s.startsWith(quoteChar) && s.endsWith(quoteChar));
                };

            if ((s.size() >= 2) && (isStringQuoted(s, QLatin1Char('\'')) || isStringQuoted(s, QLatin1Char('"'))))
                return s.mid(1, s.size() - 2);
            return s;
        }
    };

    inline bool operator==(const QString &s, const StringOption &o)
    {
        return o == s;
    }

    // Option with integer value. May not have a shortcut
    class IntOption: protected StringOption
    {
    public:
        constexpr IntOption(const char *name)
            : StringOption {name}
        {
        }

        using StringOption::operator==;
        using StringOption::usage;

        int value(const QString &arg) const
        {
            QString val = StringOption::value(arg);
            bool ok = false;
            int res = val.toInt(&ok);
            if (!ok)
                throw CommandLineParameterError(QObject::tr("Parameter '%1' must follow syntax '%1=%2'",
                                                            "e.g. Parameter '--webui-port' must follow syntax '--webui-port=<value>'")
                                                .arg(fullParameter()).arg(QLatin1String("<integer value>")));
            return res;
        }

        int value(const QProcessEnvironment &env, int defaultValue) const
        {
            QString val = env.value(envVarName());
            if (val.isEmpty()) return defaultValue;

            bool ok;
            int res = val.toInt(&ok);
            if (!ok) {
                qDebug() << QObject::tr("Expected integer number in environment variable '%1', but got '%2'")
                    .arg(envVarName()).arg(val);
                return defaultValue;
            }
            return res;
        }
    };

    inline bool operator==(const QString &s, const IntOption &o)
    {
        return o == s;
    }

    constexpr const BoolOption SHOW_HELP_OPTION = {"help", 'h'};
    constexpr const BoolOption SHOW_VERSION_OPTION = {"version", 'v'};
#ifdef DISABLE_GUI
    constexpr const BoolOption DAEMON_OPTION = {"daemon", 'd'};
#else
    constexpr const BoolOption NO_SPLASH_OPTION = {"no-splash"};
#endif
    constexpr const IntOption WEBUI_PORT_OPTION = {"webui-port"};
    constexpr const StringOption PROFILE_OPTION = {"profile"};
    constexpr const StringOption CONFIGURATION_OPTION = {"configuration"};
    constexpr const BoolOption PORTABLE_OPTION = {"portable"};
    constexpr const BoolOption RELATIVE_FASTRESUME = {"relative-fastresume"};
}

QBtCommandLineParameters::QBtCommandLineParameters(const QProcessEnvironment &env)
    : showHelp(false)
#ifndef Q_OS_WIN
    , showVersion(false)
#endif
#ifndef DISABLE_GUI
    , noSplash(NO_SPLASH_OPTION.value(env))
#else
    , shouldDaemonize(DAEMON_OPTION.value(env))
#endif
    , webUiPort(WEBUI_PORT_OPTION.value(env, -1))
    , profileDir(PROFILE_OPTION.value(env))
    , relativeFastresumePaths(RELATIVE_FASTRESUME.value(env))
    , portableMode(PORTABLE_OPTION.value(env))
    , configurationName(CONFIGURATION_OPTION.value(env))
{
}

QBtCommandLineParameters parseCommandLine(const QStringList &args)
{
    QBtCommandLineParameters result {QProcessEnvironment::systemEnvironment()};

    for (int i = 1; i < args.count(); ++i) {
        const QString &arg = args[i];

        if ((arg.startsWith("--") && !arg.endsWith(".torrent"))
            || (arg.startsWith("-") && (arg.size() == 2))) {
            // Parse known parameters
            if ((arg == SHOW_HELP_OPTION)) {
                result.showHelp = true;
            }
#ifndef Q_OS_WIN
            else if (arg == SHOW_VERSION_OPTION) {
                result.showVersion = true;
            }
#endif
            else if (arg == WEBUI_PORT_OPTION) {
                result.webUiPort = WEBUI_PORT_OPTION.value(arg);
                if ((result.webUiPort < 1) || (result.webUiPort > 65535))
                    throw CommandLineParameterError(QObject::tr("%1 must specify the correct port (1 to 65535).")
                                                    .arg(QLatin1String("--webui-port")));
            }
#ifndef DISABLE_GUI
            else if (arg == NO_SPLASH_OPTION) {
                result.noSplash = true;
            }
#else
            else if (arg == DAEMON_OPTION) {
                result.shouldDaemonize = true;
            }
#endif
            else if (arg == PROFILE_OPTION) {
                result.profileDir = PROFILE_OPTION.value(arg);
            }
            else if (arg == RELATIVE_FASTRESUME) {
                result.relativeFastresumePaths = true;
            }
            else if (arg == PORTABLE_OPTION) {
                result.portableMode = true;
            }
            else if (arg == CONFIGURATION_OPTION) {
                result.configurationName = CONFIGURATION_OPTION.value(arg);
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
    QTextStream stream(&text, QIODevice::WriteOnly);

    stream << QObject::tr("Usage:") << '\n';
#ifndef Q_OS_WIN
    stream << '\t' << prgName << " [options] [(<filename> | <url>)...]" << '\n';
#endif

    stream << QObject::tr("Options:") << '\n';
#ifndef Q_OS_WIN
    stream << SHOW_VERSION_OPTION.usage() << QObject::tr("Displays program version and exit") << '\n';
#endif
    stream << SHOW_HELP_OPTION.usage() << QObject::tr("Displays this help message and exit") << '\n';
    stream << WEBUI_PORT_OPTION.usage(QLatin1String("port"))
           << QObject::tr("Changes the Web UI port")
           << '\n';
#ifndef DISABLE_GUI
    stream << NO_SPLASH_OPTION.usage() << QObject::tr("Disable splash screen") << '\n';
#else
    stream << DAEMON_OPTION.usage() << QObject::tr("Run in daemon-mode (background)") << '\n';
#endif
    stream << PROFILE_OPTION.usage(QLatin1String("dir")) << QObject::tr("Store configuration files in <dir>") << '\n';
    stream << CONFIGURATION_OPTION.usage(QLatin1String("name")) << QObject::tr("Store configuration files in directories qBittorrent_<name>") << '\n';
    stream << RELATIVE_FASTRESUME.usage() << QObject::tr("Hack into libtorrent fastresume files and make file paths relative to the profile directory") << '\n';
    stream << PORTABLE_OPTION.usage() << QObject::tr("Shortcut for --profile=<exe dir>/profile --relative-fastresume") << '\n';
    stream << "\tfiles or urls\t\t" << QObject::tr("Downloads the torrents passed by the user") << '\n'
           << '\n';

    stream << QObject::tr("Option values may be supplied via environment variables.") << '\n'
           << QObject::tr("For option named 'parameter-name', environment variable name is 'QBT_PARAMETER_NAME' (in upper case, '-' replaced with '_')") << '\n'
           << QObject::tr("To pass flag values, set the variable to '1' or 'TRUE'.") << '\n'
           << QObject::tr("For example, to disable the splash screen: ")
           << "QBT_NO_SPLASH=1 " << prgName << '\n'
           << '\n'
           << QObject::tr("Command line parameters take precedence over environment variables") << '\n';

    stream << flush;
    return text;
}

void displayUsage(const QString &prgName)
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
