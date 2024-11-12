/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016  Eugene Shalygin <eugene.shalygin@gmail.com>
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

#include "cmdoptions.h"

#include <cstdio>

#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QStringView>

#if defined(Q_OS_WIN) && !defined(DISABLE_GUI)
#include <QMessageBox>
#endif

#include "base/global.h"
#include "base/utils/fs.h"
#include "base/utils/misc.h"
#include "base/utils/string.h"

#ifndef DISABLE_GUI
#include "gui/utils.h"
#endif

namespace
{
    const int USAGE_INDENTATION = 4;
    const int USAGE_TEXT_COLUMN = 31;
    const int WRAP_AT_COLUMN = 80;

    // Base option class. Encapsulates name operations.
    class Option
    {
    protected:
        explicit constexpr Option(const QStringView name, const QChar shortcut = QChar::Null)
            : m_name {name}
            , m_shortcut {shortcut}
        {
        }

        QString fullParameter() const
        {
            return u"--" + m_name.toString();
        }

        QString shortcutParameter() const
        {
            return u"-" + m_shortcut;
        }

        bool hasShortcut() const
        {
            return !m_shortcut.isNull();
        }

        QString envVarName() const
        {
            return u"QBT_"
                   + m_name.toString().toUpper().replace(u'-', u'_');
        }

    public:
        static QString padUsageText(const QString &usage)
        {
            QString res = QString(USAGE_INDENTATION, u' ') + usage;

            if ((USAGE_TEXT_COLUMN - usage.length() - 4) > 0)
                return res + QString((USAGE_TEXT_COLUMN - usage.length() - 4), u' ');

            return res;
        }

    private:
        const QStringView m_name;
        const QChar m_shortcut;
    };

    // Boolean option.
    class BoolOption : protected Option
    {
    public:
        explicit constexpr BoolOption(const QStringView name, const QChar shortcut = QChar::Null)
            : Option {name, shortcut}
        {
        }

        bool value(const QProcessEnvironment &env) const
        {
            QString val = env.value(envVarName());
            // we accept "1" and "true" (upper or lower cased) as boolean 'true' values
            return ((val == u"1") || (val.toUpper() == u"TRUE"));
        }

        QString usage() const
        {
            QString res;
            if (hasShortcut())
                res += shortcutParameter() + u" | ";
            res += fullParameter();
            return padUsageText(res);
        }

        friend bool operator==(const BoolOption &option, const QString &arg)
        {
            return (option.hasShortcut() && ((arg.size() == 2) && (option.shortcutParameter() == arg)))
                   || (option.fullParameter() == arg);
        }
    };

    // Option with string value. May not have a shortcut
    struct StringOption : protected Option
    {
    public:
        explicit constexpr StringOption(const QStringView name)
            : Option {name, QChar::Null}
        {
        }

        QString value(const QString &arg) const
        {
            QStringList parts = arg.split(u'=');
            if (parts.size() == 2)
                return Utils::String::unquote(parts[1], u"'\""_s);
            throw CommandLineParameterError(QCoreApplication::translate("CMD Options", "Parameter '%1' must follow syntax '%1=%2'",
                                                        "e.g. Parameter '--webui-port' must follow syntax '--webui-port=value'")
                                            .arg(fullParameter(), u"<value>"_s));
        }

        QString value(const QProcessEnvironment &env, const QString &defaultValue = {}) const
        {
            QString val = env.value(envVarName());
            return val.isEmpty() ? defaultValue : Utils::String::unquote(val, u"'\""_s);
        }

        QString usage(const QString &valueName) const
        {
            return padUsageText(parameterAssignment() + u'<' + valueName + u'>');
        }

        friend bool operator==(const StringOption &option, const QString &arg)
        {
            return arg.startsWith(option.parameterAssignment());
        }

    private:
        QString parameterAssignment() const
        {
            return fullParameter() + u'=';
        }
    };

    // Option with integer value. May not have a shortcut
    class IntOption : protected StringOption
    {
    public:
        explicit constexpr IntOption(const QStringView name)
            : StringOption {name}
        {
        }

        using StringOption::usage;

        int value(const QString &arg) const
        {
            const QString val = StringOption::value(arg);
            bool ok = false;
            const int res = val.toInt(&ok);
            if (!ok)
            {
                throw CommandLineParameterError(QCoreApplication::translate("CMD Options", "Parameter '%1' must follow syntax '%1=%2'",
                                                            "e.g. Parameter '--webui-port' must follow syntax '--webui-port=<value>'")
                                                .arg(fullParameter(), u"<integer value>"_s));
            }
            return res;
        }

        int value(const QProcessEnvironment &env, int defaultValue) const
        {
            QString val = env.value(envVarName());
            if (val.isEmpty()) return defaultValue;

            bool ok;
            int res = val.toInt(&ok);
            if (!ok)
            {
                qDebug() << QCoreApplication::translate("CMD Options", "Expected integer number in environment variable '%1', but got '%2'")
                    .arg(envVarName(), val);
                return defaultValue;
            }
            return res;
        }

        friend bool operator==(const IntOption &option, const QString &arg)
        {
            return (static_cast<StringOption>(option) == arg);
        }
    };

    // Option that is explicitly set to true or false, and whose value is undefined when unspecified.
    // May not have a shortcut.
    class TriStateBoolOption : protected Option
    {
    public:
        constexpr TriStateBoolOption(const QStringView name, const bool defaultValue)
            : Option {name, QChar::Null}
            , m_defaultValue(defaultValue)
        {
        }

        QString usage() const
        {
            return padUsageText(fullParameter() + u"=<true|false>");
        }

        std::optional<bool> value(const QString &arg) const
        {
            QStringList parts = arg.split(u'=');

            if (parts.size() == 1)
            {
                return m_defaultValue;
            }
            if (parts.size() == 2)
            {
                QString val = parts[1];

                if ((val.toUpper() == u"TRUE") || (val == u"1"))
                {
                    return true;
                }
                if ((val.toUpper() == u"FALSE") || (val == u"0"))
                {
                    return false;
                }
            }

            throw CommandLineParameterError(QCoreApplication::translate("CMD Options", "Parameter '%1' must follow syntax '%1=%2'",
                                                        "e.g. Parameter '--add-stopped' must follow syntax "
                                                        "'--add-stopped=<true|false>'")
                                            .arg(fullParameter(), u"<true|false>"_s));
        }

        std::optional<bool> value(const QProcessEnvironment &env) const
        {
            const QString val = env.value(envVarName(), u"-1"_s);

            if (val.isEmpty())
            {
                return m_defaultValue;
            }
            if (val == u"-1")
            {
                return std::nullopt;
            }
            if ((val.toUpper() == u"TRUE") || (val == u"1"))
            {
                return true;
            }
            if ((val.toUpper() == u"FALSE") || (val == u"0"))
            {
                return false;
            }

            qDebug() << QCoreApplication::translate("CMD Options", "Expected %1 in environment variable '%2', but got '%3'")
                .arg(u"true|false"_s, envVarName(), val);
            return std::nullopt;
        }

        friend bool operator==(const TriStateBoolOption &option, const QString &arg)
        {
            return arg.section(u'=', 0, 0) == option.fullParameter();
        }

    private:
        bool m_defaultValue = false;
    };

    constexpr const BoolOption SHOW_HELP_OPTION {u"help", u'h'};
#if !defined(Q_OS_WIN) || defined(DISABLE_GUI)
    constexpr const BoolOption SHOW_VERSION_OPTION {u"version", u'v'};
#endif
    constexpr const BoolOption CONFIRM_LEGAL_NOTICE {u"confirm-legal-notice"};
#if defined(DISABLE_GUI) && !defined(Q_OS_WIN)
    constexpr const BoolOption DAEMON_OPTION {u"daemon", u'd'};
#else
    constexpr const BoolOption NO_SPLASH_OPTION {u"no-splash"};
#endif
    constexpr const IntOption WEBUI_PORT_OPTION {u"webui-port"};
    constexpr const IntOption TORRENTING_PORT_OPTION {u"torrenting-port"};
    constexpr const StringOption PROFILE_OPTION {u"profile"};
    constexpr const StringOption CONFIGURATION_OPTION {u"configuration"};
    constexpr const BoolOption RELATIVE_FASTRESUME {u"relative-fastresume"};
    constexpr const StringOption SAVE_PATH_OPTION {u"save-path"};
    constexpr const TriStateBoolOption STOPPED_OPTION {u"add-stopped", true};
    constexpr const BoolOption SKIP_HASH_CHECK_OPTION {u"skip-hash-check"};
    constexpr const StringOption CATEGORY_OPTION {u"category"};
    constexpr const BoolOption SEQUENTIAL_OPTION {u"sequential"};
    constexpr const BoolOption FIRST_AND_LAST_OPTION {u"first-and-last"};
    constexpr const TriStateBoolOption SKIP_DIALOG_OPTION {u"skip-dialog", true};
}

QBtCommandLineParameters::QBtCommandLineParameters(const QProcessEnvironment &env)
    : confirmLegalNotice(CONFIRM_LEGAL_NOTICE.value(env))
    , relativeFastresumePaths(RELATIVE_FASTRESUME.value(env))
#ifndef DISABLE_GUI
    , noSplash(NO_SPLASH_OPTION.value(env))
#elif !defined(Q_OS_WIN)
    , shouldDaemonize(DAEMON_OPTION.value(env))
#endif
    , webUIPort(WEBUI_PORT_OPTION.value(env, -1))
    , torrentingPort(TORRENTING_PORT_OPTION.value(env, -1))
    , skipDialog(SKIP_DIALOG_OPTION.value(env))
    , profileDir(Utils::Fs::toAbsolutePath(Path(PROFILE_OPTION.value(env))))
    , configurationName(CONFIGURATION_OPTION.value(env))
{
    addTorrentParams.savePath = Path(SAVE_PATH_OPTION.value(env));
    addTorrentParams.category = CATEGORY_OPTION.value(env);
    addTorrentParams.skipChecking = SKIP_HASH_CHECK_OPTION.value(env);
    addTorrentParams.sequential = SEQUENTIAL_OPTION.value(env);
    addTorrentParams.firstLastPiecePriority = FIRST_AND_LAST_OPTION.value(env);
    addTorrentParams.addStopped = STOPPED_OPTION.value(env);
}

QBtCommandLineParameters parseCommandLine(const QStringList &args)
{
    QBtCommandLineParameters result {QProcessEnvironment::systemEnvironment()};

    for (int i = 1; i < args.count(); ++i)
    {
        const QString &arg = args[i];

        if ((arg.startsWith(u"--") && !arg.endsWith(u".torrent"))
            || (arg.startsWith(u'-') && (arg.size() == 2)))
        {
            // Parse known parameters
            if (arg == SHOW_HELP_OPTION)
            {
                result.showHelp = true;
            }
#if !defined(Q_OS_WIN) || defined(DISABLE_GUI)
            else if (arg == SHOW_VERSION_OPTION)
            {
                result.showVersion = true;
            }
#endif
            else if (arg == CONFIRM_LEGAL_NOTICE)
            {
                result.confirmLegalNotice = true;
            }
            else if (arg == WEBUI_PORT_OPTION)
            {
                result.webUIPort = WEBUI_PORT_OPTION.value(arg);
                if ((result.webUIPort < 1) || (result.webUIPort > 65535))
                    throw CommandLineParameterError(QCoreApplication::translate("CMD Options", "%1 must specify a valid port (1 to 65535).")
                                                    .arg(u"--webui-port"_s));
            }
            else if (arg == TORRENTING_PORT_OPTION)
            {
                result.torrentingPort = TORRENTING_PORT_OPTION.value(arg);
                if ((result.torrentingPort < 1) || (result.torrentingPort > 65535))
                {
                    throw CommandLineParameterError(QCoreApplication::translate("CMD Options", "%1 must specify a valid port (1 to 65535).")
                                                    .arg(u"--torrenting-port"_s));
                }
            }
#ifndef DISABLE_GUI
            else if (arg == NO_SPLASH_OPTION)
            {
                result.noSplash = true;
            }
#elif !defined(Q_OS_WIN)
            else if (arg == DAEMON_OPTION)
            {
                result.shouldDaemonize = true;
            }
#endif
            else if (arg == PROFILE_OPTION)
            {
                result.profileDir = Utils::Fs::toAbsolutePath(Path(PROFILE_OPTION.value(arg)));
            }
            else if (arg == RELATIVE_FASTRESUME)
            {
                result.relativeFastresumePaths = true;
            }
            else if (arg == CONFIGURATION_OPTION)
            {
                result.configurationName = CONFIGURATION_OPTION.value(arg);
            }
            else if (arg == SAVE_PATH_OPTION)
            {
                result.addTorrentParams.savePath = Path(SAVE_PATH_OPTION.value(arg));
            }
            else if (arg == STOPPED_OPTION)
            {
                result.addTorrentParams.addStopped = STOPPED_OPTION.value(arg);
            }
            else if (arg == SKIP_HASH_CHECK_OPTION)
            {
                result.addTorrentParams.skipChecking = true;
            }
            else if (arg == CATEGORY_OPTION)
            {
                result.addTorrentParams.category = CATEGORY_OPTION.value(arg);
            }
            else if (arg == SEQUENTIAL_OPTION)
            {
                result.addTorrentParams.sequential = true;
            }
            else if (arg == FIRST_AND_LAST_OPTION)
            {
                result.addTorrentParams.firstLastPiecePriority = true;
            }
            else if (arg == SKIP_DIALOG_OPTION)
            {
                result.skipDialog = SKIP_DIALOG_OPTION.value(arg);
            }
            else
            {
                // Unknown argument
                result.unknownParameter = arg;
                break;
            }
        }
        else
        {
            QFileInfo torrentPath;
            torrentPath.setFile(arg);

            if (torrentPath.exists())
                result.torrentSources += torrentPath.absoluteFilePath();
            else
                result.torrentSources += arg;
        }
    }

    return result;
}

QString wrapText(const QString &text, int initialIndentation = USAGE_TEXT_COLUMN, int wrapAtColumn = WRAP_AT_COLUMN)
{
    QStringList words = text.split(u' ');
    QStringList lines = {words.first()};
    int currentLineMaxLength = wrapAtColumn - initialIndentation;

    for (const QString &word : asConst(words.mid(1)))
    {
        if (lines.last().length() + word.length() + 1 < currentLineMaxLength)
        {
            lines.last().append(u' ' + word);
        }
        else
        {
            lines.append(QString(initialIndentation, u' ') + word);
            currentLineMaxLength = wrapAtColumn;
        }
    }

    return lines.join(u'\n');
}

QString makeUsage(const QString &prgName)
{
    const QString indentation {USAGE_INDENTATION, u' '};

    const QString text = QCoreApplication::translate("CMD Options", "Usage:") + u'\n'
        + indentation + prgName + u' ' + QCoreApplication::translate("CMD Options", "[options] [(<filename> | <url>)...]") + u'\n'

        + QCoreApplication::translate("CMD Options", "Options:") + u'\n'
        + SHOW_HELP_OPTION.usage() + wrapText(QCoreApplication::translate("CMD Options", "Display this help message and exit")) + u'\n'
#if !defined(Q_OS_WIN) || defined(DISABLE_GUI)
        + SHOW_VERSION_OPTION.usage() + wrapText(QCoreApplication::translate("CMD Options", "Display program version and exit")) + u'\n'
#endif
        + CONFIRM_LEGAL_NOTICE.usage() + wrapText(QCoreApplication::translate("CMD Options", "Confirm the legal notice")) + u'\n'
        + WEBUI_PORT_OPTION.usage(QCoreApplication::translate("CMD Options", "port"))
        + wrapText(QCoreApplication::translate("CMD Options", "Change the WebUI port"))
        + u'\n'
        + TORRENTING_PORT_OPTION.usage(QCoreApplication::translate("CMD Options", "port"))
        + wrapText(QCoreApplication::translate("CMD Options", "Change the torrenting port"))
        + u'\n'
#ifndef DISABLE_GUI
        + NO_SPLASH_OPTION.usage() + wrapText(QCoreApplication::translate("CMD Options", "Disable splash screen")) + u'\n'
#elif !defined(Q_OS_WIN)
        + DAEMON_OPTION.usage() + wrapText(QCoreApplication::translate("CMD Options", "Run in daemon-mode (background)")) + u'\n'
#endif
    //: Use appropriate short form or abbreviation of "directory"
        + PROFILE_OPTION.usage(QCoreApplication::translate("CMD Options", "dir"))
        + wrapText(QCoreApplication::translate("CMD Options", "Store configuration files in <dir>")) + u'\n'
        + CONFIGURATION_OPTION.usage(QCoreApplication::translate("CMD Options", "name"))
        + wrapText(QCoreApplication::translate("CMD Options", "Store configuration files in directories qBittorrent_<name>")) + u'\n'
        + RELATIVE_FASTRESUME.usage()
        + wrapText(QCoreApplication::translate("CMD Options", "Hack into libtorrent fastresume files and make file paths relative "
                                "to the profile directory")) + u'\n'
        + Option::padUsageText(QCoreApplication::translate("CMD Options", "files or URLs"))
        + wrapText(QCoreApplication::translate("CMD Options", "Download the torrents passed by the user")) + u'\n'
        + u'\n'

        + wrapText(QCoreApplication::translate("CMD Options", "Options when adding new torrents:"), 0) + u'\n'
        + SAVE_PATH_OPTION.usage(QCoreApplication::translate("CMD Options", "path")) + wrapText(QCoreApplication::translate("CMD Options", "Torrent save path")) + u'\n'
                         + STOPPED_OPTION.usage() + wrapText(QCoreApplication::translate("CMD Options", "Add torrents as running or stopped")) + u'\n'
        + SKIP_HASH_CHECK_OPTION.usage() + wrapText(QCoreApplication::translate("CMD Options", "Skip hash check")) + u'\n'
        + CATEGORY_OPTION.usage(QCoreApplication::translate("CMD Options", "name"))
        + wrapText(QCoreApplication::translate("CMD Options", "Assign torrents to category. If the category doesn't exist, it will be "
                                "created.")) + u'\n'
        + SEQUENTIAL_OPTION.usage() + wrapText(QCoreApplication::translate("CMD Options", "Download files in sequential order")) + u'\n'
        + FIRST_AND_LAST_OPTION.usage()
        + wrapText(QCoreApplication::translate("CMD Options", "Download first and last pieces first")) + u'\n'
        + SKIP_DIALOG_OPTION.usage()
        + wrapText(QCoreApplication::translate("CMD Options", "Specify whether the \"Add New Torrent\" dialog opens when adding a "
                                "torrent.")) + u'\n'
        + u'\n'

        + wrapText(QCoreApplication::translate("CMD Options", "Option values may be supplied via environment variables. For option named "
                                "'parameter-name', environment variable name is 'QBT_PARAMETER_NAME' (in upper "
                                "case, '-' replaced with '_'). To pass flag values, set the variable to '1' or "
                                "'TRUE'. For example, to disable the splash screen: "), 0) + u'\n'
        + u"QBT_NO_SPLASH=1 " + prgName + u'\n'
        + wrapText(QCoreApplication::translate("CMD Options", "Command line parameters take precedence over environment variables"), 0) + u'\n';

    return text;
}

void displayUsage(const QString &prgName)
{
#if defined(Q_OS_WIN) && !defined(DISABLE_GUI)
    QMessageBox msgBox(QMessageBox::Information, QCoreApplication::translate("CMD Options", "Help"), makeUsage(prgName), QMessageBox::Ok);
    msgBox.show(); // Need to be shown or to moveToCenter does not work
    msgBox.move(Utils::Gui::screenCenter(&msgBox));
    msgBox.exec();
#else
    printf("%s\n", qUtf8Printable(makeUsage(prgName)));
#endif
}
