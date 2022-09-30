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

#include <QDebug>
#include <QFileInfo>
#include <QProcessEnvironment>

#if defined(Q_OS_WIN) && !defined(DISABLE_GUI)
#include <QMessageBox>
#endif

#include "base/global.h"
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
        explicit constexpr Option(const char *name, char shortcut = 0)
            : m_name {name}
            , m_shortcut {shortcut}
        {
        }

        QString fullParameter() const
        {
            return u"--" + QString::fromLatin1(m_name);
        }

        QString shortcutParameter() const
        {
            return u"-" + QChar::fromLatin1(m_shortcut);
        }

        bool hasShortcut() const
        {
            return m_shortcut != 0;
        }

        QString envVarName() const
        {
            return u"QBT_"
                   + QString::fromLatin1(m_name).toUpper().replace(u'-', u'_');
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
        const char *m_name = nullptr;
        const char m_shortcut;
    };

    // Boolean option.
    class BoolOption : protected Option
    {
    public:
        explicit constexpr BoolOption(const char *name, char shortcut = 0)
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

    bool operator==(const QString &arg, const BoolOption &option)
    {
        return (option == arg);
    }

    // Option with string value. May not have a shortcut
    struct StringOption : protected Option
    {
    public:
        explicit constexpr StringOption(const char *name)
            : Option {name, 0}
        {
        }

        QString value(const QString &arg) const
        {
            QStringList parts = arg.split(u'=');
            if (parts.size() == 2)
                return Utils::String::unquote(parts[1], u"'\""_qs);
            throw CommandLineParameterError(QObject::tr("Parameter '%1' must follow syntax '%1=%2'",
                                                        "e.g. Parameter '--webui-port' must follow syntax '--webui-port=value'")
                                            .arg(fullParameter(), u"<value>"_qs));
        }

        QString value(const QProcessEnvironment &env, const QString &defaultValue = {}) const
        {
            QString val = env.value(envVarName());
            return val.isEmpty() ? defaultValue : Utils::String::unquote(val, u"'\""_qs);
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

    bool operator==(const QString &arg, const StringOption &option)
    {
        return (option == arg);
    }

    // Option with integer value. May not have a shortcut
    class IntOption : protected StringOption
    {
    public:
        explicit constexpr IntOption(const char *name)
            : StringOption {name}
        {
        }

        using StringOption::usage;

        int value(const QString &arg) const
        {
            QString val = StringOption::value(arg);
            bool ok = false;
            int res = val.toInt(&ok);
            if (!ok)
                throw CommandLineParameterError(QObject::tr("Parameter '%1' must follow syntax '%1=%2'",
                                                            "e.g. Parameter '--webui-port' must follow syntax '--webui-port=<value>'")
                                                .arg(fullParameter(), u"<integer value>"_qs));
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
                qDebug() << QObject::tr("Expected integer number in environment variable '%1', but got '%2'")
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

    bool operator==(const QString &arg, const IntOption &option)
    {
        return (option == arg);
    }

    // Option that is explicitly set to true or false, and whose value is undefined when unspecified.
    // May not have a shortcut.
    class TriStateBoolOption : protected Option
    {
    public:
        constexpr TriStateBoolOption(const char *name, bool defaultValue)
            : Option {name, 0}
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

            throw CommandLineParameterError(QObject::tr("Parameter '%1' must follow syntax '%1=%2'",
                                                        "e.g. Parameter '--add-paused' must follow syntax "
                                                        "'--add-paused=<true|false>'")
                                            .arg(fullParameter(), u"<true|false>"_qs));
        }

        std::optional<bool> value(const QProcessEnvironment &env) const
        {
            const QString val = env.value(envVarName(), u"-1"_qs);

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

            qDebug() << QObject::tr("Expected %1 in environment variable '%2', but got '%3'")
                .arg(u"true|false"_qs, envVarName(), val);
            return std::nullopt;
        }

        friend bool operator==(const TriStateBoolOption &option, const QString &arg)
        {
            return arg.section(u'=', 0, 0) == option.fullParameter();
        }

        bool m_defaultValue;
    };

    bool operator==(const QString &arg, const TriStateBoolOption &option)
    {
        return (option == arg);
    }

    constexpr const BoolOption SHOW_HELP_OPTION {"help", 'h'};
    constexpr const BoolOption SHOW_VERSION_OPTION {"version", 'v'};
#if defined(DISABLE_GUI) && !defined(Q_OS_WIN)
    constexpr const BoolOption DAEMON_OPTION {"daemon", 'd'};
#else
    constexpr const BoolOption NO_SPLASH_OPTION {"no-splash"};
#endif
    constexpr const IntOption WEBUI_PORT_OPTION {"webui-port"};
    constexpr const StringOption PROFILE_OPTION {"profile"};
    constexpr const StringOption CONFIGURATION_OPTION {"configuration"};
    constexpr const BoolOption RELATIVE_FASTRESUME {"relative-fastresume"};
    constexpr const StringOption SAVE_PATH_OPTION {"save-path"};
    constexpr const TriStateBoolOption PAUSED_OPTION {"add-paused", true};
    constexpr const BoolOption SKIP_HASH_CHECK_OPTION {"skip-hash-check"};
    constexpr const StringOption CATEGORY_OPTION {"category"};
    constexpr const BoolOption SEQUENTIAL_OPTION {"sequential"};
    constexpr const BoolOption FIRST_AND_LAST_OPTION {"first-and-last"};
    constexpr const TriStateBoolOption SKIP_DIALOG_OPTION {"skip-dialog", true};
}

QBtCommandLineParameters::QBtCommandLineParameters(const QProcessEnvironment &env)
    : showHelp(false)
    , relativeFastresumePaths(RELATIVE_FASTRESUME.value(env))
    , skipChecking(SKIP_HASH_CHECK_OPTION.value(env))
    , sequential(SEQUENTIAL_OPTION.value(env))
    , firstLastPiecePriority(FIRST_AND_LAST_OPTION.value(env))
#if !defined(Q_OS_WIN) || defined(DISABLE_GUI)
    , showVersion(false)
#endif
#ifndef DISABLE_GUI
    , noSplash(NO_SPLASH_OPTION.value(env))
#elif !defined(Q_OS_WIN)
    , shouldDaemonize(DAEMON_OPTION.value(env))
#endif
    , webUiPort(WEBUI_PORT_OPTION.value(env, -1))
    , addPaused(PAUSED_OPTION.value(env))
    , skipDialog(SKIP_DIALOG_OPTION.value(env))
    , profileDir(PROFILE_OPTION.value(env))
    , configurationName(CONFIGURATION_OPTION.value(env))
    , savePath(SAVE_PATH_OPTION.value(env))
    , category(CATEGORY_OPTION.value(env))
{
}

QStringList QBtCommandLineParameters::paramList() const
{
    QStringList result;
    // Because we're passing a string list to the currently running
    // qBittorrent process, we need some way of passing along the options
    // the user has specified. Here we place special strings that are
    // almost certainly not going to collide with a file path or URL
    // specified by the user, and placing them at the beginning of the
    // string list so that they will be processed before the list of
    // torrent paths or URLs.

    if (!savePath.isEmpty())
        result.append(u"@savePath=" + savePath.data());

    if (addPaused.has_value())
        result.append(*addPaused ? u"@addPaused=1"_qs : u"@addPaused=0"_qs);

    if (skipChecking)
        result.append(u"@skipChecking"_qs);

    if (!category.isEmpty())
        result.append(u"@category=" + category);

    if (sequential)
        result.append(u"@sequential"_qs);

    if (firstLastPiecePriority)
        result.append(u"@firstLastPiecePriority"_qs);

    if (skipDialog.has_value())
        result.append(*skipDialog ? u"@skipDialog=1"_qs : u"@skipDialog=0"_qs);

    result += torrents;
    return result;
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
            else if (arg == WEBUI_PORT_OPTION)
            {
                result.webUiPort = WEBUI_PORT_OPTION.value(arg);
                if ((result.webUiPort < 1) || (result.webUiPort > 65535))
                    throw CommandLineParameterError(QObject::tr("%1 must specify a valid port (1 to 65535).")
                                                    .arg(u"--webui-port"_qs));
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
                result.profileDir = Path(PROFILE_OPTION.value(arg));
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
                result.savePath = Path(SAVE_PATH_OPTION.value(arg));
            }
            else if (arg == PAUSED_OPTION)
            {
                result.addPaused = PAUSED_OPTION.value(arg);
            }
            else if (arg == SKIP_HASH_CHECK_OPTION)
            {
                result.skipChecking = true;
            }
            else if (arg == CATEGORY_OPTION)
            {
                result.category = CATEGORY_OPTION.value(arg);
            }
            else if (arg == SEQUENTIAL_OPTION)
            {
                result.sequential = true;
            }
            else if (arg == FIRST_AND_LAST_OPTION)
            {
                result.firstLastPiecePriority = true;
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
                result.torrents += torrentPath.absoluteFilePath();
            else
                result.torrents += arg;
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

    const QString text = QObject::tr("Usage:") + u'\n'
        + indentation + prgName + u' ' + QObject::tr("[options] [(<filename> | <url>)...]") + u'\n'

        + QObject::tr("Options:") + u'\n'
#if !defined(Q_OS_WIN) || defined(DISABLE_GUI)
        + SHOW_VERSION_OPTION.usage() + wrapText(QObject::tr("Display program version and exit")) + u'\n'
#endif
        + SHOW_HELP_OPTION.usage() + wrapText(QObject::tr("Display this help message and exit")) + u'\n'
        + WEBUI_PORT_OPTION.usage(QObject::tr("port"))
        + wrapText(QObject::tr("Change the Web UI port"))
        + u'\n'
#ifndef DISABLE_GUI
        + NO_SPLASH_OPTION.usage() + wrapText(QObject::tr("Disable splash screen")) + u'\n'
#elif !defined(Q_OS_WIN)
        + DAEMON_OPTION.usage() + wrapText(QObject::tr("Run in daemon-mode (background)")) + u'\n'
#endif
    //: Use appropriate short form or abbreviation of "directory"
        + PROFILE_OPTION.usage(QObject::tr("dir"))
        + wrapText(QObject::tr("Store configuration files in <dir>")) + u'\n'
        + CONFIGURATION_OPTION.usage(QObject::tr("name"))
        + wrapText(QObject::tr("Store configuration files in directories qBittorrent_<name>")) + u'\n'
        + RELATIVE_FASTRESUME.usage()
        + wrapText(QObject::tr("Hack into libtorrent fastresume files and make file paths relative "
                                "to the profile directory")) + u'\n'
        + Option::padUsageText(QObject::tr("files or URLs"))
        + wrapText(QObject::tr("Download the torrents passed by the user")) + u'\n'
        + u'\n'

        + wrapText(QObject::tr("Options when adding new torrents:"), 0) + u'\n'
        + SAVE_PATH_OPTION.usage(QObject::tr("path")) + wrapText(QObject::tr("Torrent save path")) + u'\n'
        + PAUSED_OPTION.usage() + wrapText(QObject::tr("Add torrents as started or paused")) + u'\n'
        + SKIP_HASH_CHECK_OPTION.usage() + wrapText(QObject::tr("Skip hash check")) + u'\n'
        + CATEGORY_OPTION.usage(QObject::tr("name"))
        + wrapText(QObject::tr("Assign torrents to category. If the category doesn't exist, it will be "
                                "created.")) + u'\n'
        + SEQUENTIAL_OPTION.usage() + wrapText(QObject::tr("Download files in sequential order")) + u'\n'
        + FIRST_AND_LAST_OPTION.usage()
        + wrapText(QObject::tr("Download first and last pieces first")) + u'\n'
        + SKIP_DIALOG_OPTION.usage()
        + wrapText(QObject::tr("Specify whether the \"Add New Torrent\" dialog opens when adding a "
                                "torrent.")) + u'\n'
        + u'\n'

        + wrapText(QObject::tr("Option values may be supplied via environment variables. For option named "
                                "'parameter-name', environment variable name is 'QBT_PARAMETER_NAME' (in upper "
                                "case, '-' replaced with '_'). To pass flag values, set the variable to '1' or "
                                "'TRUE'. For example, to disable the splash screen: "), 0) + u'\n'
        + u"QBT_NO_SPLASH=1 " + prgName + u'\n'
        + wrapText(QObject::tr("Command line parameters take precedence over environment variables"), 0) + u'\n';

    return text;
}

void displayUsage(const QString &prgName)
{
#if defined(Q_OS_WIN) && !defined(DISABLE_GUI)
    QMessageBox msgBox(QMessageBox::Information, QObject::tr("Help"), makeUsage(prgName), QMessageBox::Ok);
    msgBox.show(); // Need to be shown or to moveToCenter does not work
    msgBox.move(Utils::Gui::screenCenter(&msgBox));
    msgBox.exec();
#else
    printf("%s\n", qUtf8Printable(makeUsage(prgName)));
#endif
}
