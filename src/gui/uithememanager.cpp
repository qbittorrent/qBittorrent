/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2019  Prince Gupta <jagannatharjun11@gmail.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If
 * you modify file(s), you may extend this exception to your version of the
 * file(s), but you are not obligated to do so. If you do not wish to do so,
 * delete this exception statement from your version.
 */

#include "uithememanager.h"

#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>
#include <QResource>

#include "base/iconprovider.h"
#include "base/exceptions.h"
#include "base/logger.h"
#include "base/preferences.h"
#include "base/utils/fs.h"
#include "qfile.h"
#include "utils.h"

UIThemeManager *UIThemeManager::m_instance = nullptr;
static const QString allowedExts[] = {".svg", ".png"};

namespace
{
    class ThemeError : public RuntimeError
    {
    public:
        using RuntimeError::RuntimeError;
    };

    UIThemeManager::IconMap loadIconConfig(const QString &configFile, const QString &iconDir)
    {
        QFile iconJsonMap(configFile);
        if (!iconJsonMap.open(QIODevice::ReadOnly))
            throw ThemeError(QObject::tr("Failed to open \"%1\", error: %2.")
                             .arg(iconJsonMap.fileName(), iconJsonMap.errorString()));

        QJsonObject jsonObject;
        QJsonParseError err;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(iconJsonMap.readAll(), &err);
        if (err.error != QJsonParseError::NoError)
            throw ThemeError(QObject::tr("Error occurred while parsing \"%1\", error: %2.")
                             .arg(iconJsonMap.fileName(), err.errorString()));
        else if (!jsonDoc.isObject())
            throw ThemeError(QObject::tr("Invalid icon configuration file format in \"%1\". JSON object is expected.")
                             .arg(iconJsonMap.fileName()));
        else
            jsonObject = jsonDoc.object();

        UIThemeManager::IconMap config;
        config.reserve(jsonObject.size());
        for (auto i = jsonObject.begin(), e = jsonObject.end(); i != e; i++) {
            if (!i.value().isString())
                throw ThemeError(QObject::tr("Error in iconconfig \"%1\", error: Provided value for %2, is not a string.")
                                 .arg(configFile, i.key()));
            if (i.value().toString().isEmpty())
                throw ThemeError(QObject::tr("Error in iconconfig \"%1\", error: Provided value for %2, is empty string")
                                 .arg(configFile, i.key()));

            QString iconPath = iconDir + i.value().toString();

            QString ext;
            if (Utils::Fs::fileExtension(iconPath).isEmpty()) {
                for (const QString allowedExt : allowedExts) {
                    if (QFile::exists(iconPath + allowedExt)) {
                        ext = allowedExt;
                        break;
                    }
                }
            }
            iconPath.append(ext);

            if (!QFile::exists(iconPath))
                throw ThemeError(QObject::tr(R"(Error in iconconfig "%1", error: Can't find file "%2" required in {'%3': '%4'})")
                                 .arg(configFile, iconPath, i.key(), i.value().toString()));

            config.insert(i.key(), iconPath);
        }
        return config;
    }
}

void UIThemeManager::freeInstance()
{
    if (m_instance) {
        delete m_instance;
        m_instance = nullptr;
    }
}

void UIThemeManager::initInstance()
{
    if (!m_instance)
        m_instance = new UIThemeManager;
}

UIThemeManager::UIThemeManager() : m_useCustomStylesheet(false)
{
    const Preferences *const pref = Preferences::instance();
    bool useCustomUITheme = pref->useCustomUITheme();

#if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS))
    m_useSystemTheme = pref->useSystemIconTheme();
    const QString iconConfigFile = m_useSystemTheme ? "systemiconconfig.json" : "iconconfig.json";
#else
    const QString iconConfigFile = "iconconfig.json";
#endif

    IconMap defaultIconMap, customIconMap;

    if (useCustomUITheme) {
        try {
            if (!QResource::registerResource(pref->customUIThemePath(), "/uitheme"))
                throw ThemeError(tr("Failed to load UI theme from file: \"%1\".")
                                 .arg(pref->customUIThemePath()));
            m_useCustomStylesheet = QFile::exists(":uitheme/stylesheet.qss");
            customIconMap = loadIconConfig(":uitheme/icons/" + iconConfigFile, ":uitheme/icons/");
            m_flagsDir = ":uitheme/icons/flags/";
        } catch (const ThemeError &err) {
            LogMsg(err.message(), Log::WARNING);
            LogMsg("Encountered error in loading custom icon theme, falling back to system default", Log::WARNING);
            useCustomUITheme = false;
        }
    }

    // recheck since in case of error should fallback to system default
    if (!useCustomUITheme)
        m_flagsDir = ":icons/flags/";

    // always load defaultIconMap to resolve missing entries in customIconMap
    try {
        defaultIconMap = loadIconConfig(":icons/" + iconConfigFile, ":icons/");
    } catch (const ThemeError &err) {
        // only show the error, don't take any action but also system theme should not have any error
        LogMsg(err.message(), Log::WARNING);
    }

    // merge defaultIconMap and customIconMap into m_iconMap
    m_iconMap.swap(defaultIconMap);
    for (auto i = customIconMap.constBegin(), e = customIconMap.constEnd(); i != e; i++)
        m_iconMap.insert(i.key(), i.value()); // overwrite existing values or insert
}

UIThemeManager *UIThemeManager::instance()
{
    return m_instance;
}

void UIThemeManager::applyStyleSheet() const
{
    if (!m_useCustomStylesheet) {
        qApp->setStyleSheet({});
        return;
    }

    QFile qssFile(":uitheme/stylesheet.qss");
    if (!qssFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qApp->setStyleSheet({});
        LogMsg(tr("Couldn't apply theme stylesheet. stylesheet.qss couldn't be opened. Reason: %1").arg(qssFile.errorString())
               , Log::WARNING);
        return;
    }

    qApp->setStyleSheet(qssFile.readAll());
}

QIcon UIThemeManager::getIcon(const QString &iconId) const
{
    return getIcon(iconId, iconId);
}

QIcon UIThemeManager::getIcon(const QString &iconId, const QString &fallback) const
{
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS))
    if (m_useSystemTheme) {
        QString themeIcon = m_iconMap.value(iconId);
        QIcon icon = QIcon::fromTheme(themeIcon);
        if (icon.name() != themeIcon)
            icon = QIcon::fromTheme(fallback, QIcon(getIconPath(iconId)));
        return icon;
    }
#else
    Q_UNUSED(fallback)
#endif
    // cache to avoid rescaling svg icons
    static QHash<QString, QIcon> iconCache;
    const QString iconPath = getIconPath(iconId);
    const auto iter = iconCache.find(iconPath);
    if (iter != iconCache.end())
        return *iter;

    const QIcon icon {iconPath};
    iconCache[iconPath] = icon;
    return icon;
}

QIcon UIThemeManager::getFlagIcon(const QString &countryIsoCode) const
{
    if (countryIsoCode.isEmpty()) return {};
    return QIcon(m_flagsDir + countryIsoCode.toLower() + ".svg");
}

QPixmap UIThemeManager::getScaledPixmap(const QString &iconId, const QWidget *widget, int baseHeight) const
{
    return Utils::Gui::scaledPixmapSvg(getIconPath(iconId), widget, baseHeight);
}

QString UIThemeManager::getIconPath(const QString &iconId) const
{
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS))
    if (m_useSystemTheme) {
        QString path = Utils::Fs::tempPath() + iconId + ".png";
        if (!QFile::exists(path)) {
            const QIcon icon = QIcon::fromTheme(iconId);
            if (!icon.isNull())
                icon.pixmap(32).save(path);
            else
                path = IconProvider::instance()->getIconPath(iconId);
        }

        return path;
    }
#endif

    QString iconPath = m_iconMap.value(iconId);
    QString iconIdPattern = iconId;
    while (iconPath.isEmpty() && !iconIdPattern.isEmpty()) {
        const int sepIndex = iconIdPattern.indexOf('.');
        iconIdPattern = "*" + ((sepIndex == -1) ? "" : iconIdPattern.right(iconIdPattern.size() - sepIndex));
        const auto patternIter = m_iconMap.find(iconIdPattern);
        if (patternIter != m_iconMap.end())
            iconPath = *patternIter;
        else
            iconIdPattern.remove(0, 2); // removes "*."
    }

    if (iconPath.isEmpty()) {
        LogMsg(tr("Can't resolve icon id - %1").arg(iconId), Log::WARNING);
        return {};
    }

    return iconPath;
}
