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
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>
#include <QResource>
#include <QSet>

#include "base/iconprovider.h"
#include "base/exceptions.h"
#include "base/logger.h"
#include "base/preferences.h"
#include "base/utils/fs.h"
#include "utils.h"

#if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS))
#define QBT_SYSTEMICONS
#endif

extern const QHash<QString, QString> iconConfig, systemIconConfig;

UIThemeManager *UIThemeManager::m_instance = nullptr;

namespace
{
    class ThemeError : public RuntimeError
    {
    public:
        using RuntimeError::RuntimeError;
    };

    /*
        Loads Json Icon Config file from `configFile`, and transforms it into a map
        function throws ThemeError if encountered with any error
     */
    QHash<QString, QString> loadIconConfig(const QString &configFile)
    {
        // load icon config from file
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

        QHash<QString, QString> config; // resultant icon-config
        config.reserve(jsonObject.size());

        // resolves the names of icons in theme
        // if system icon theme should be used, then check if given icon is available in icon
        for (auto i = jsonObject.begin(), e = jsonObject.end(); i != e; ++i) {
            if (!i.value().isString())
                throw ThemeError(QObject::tr("Error in iconconfig \"%1\", error: Provided value for %2, is not a string.")
                                 .arg(configFile, i.key()));

            QString value = i.value().toString();
            if (value.isEmpty())
                throw ThemeError(QObject::tr("Error in iconconfig \"%1\", error: Provided value for %2, is empty string")
                                 .arg(configFile, i.key()));

            config.insert(i.key(), value);
        }
        return config;
    }

    QHash<QString, QString> resolveDirs(QHash<QString, QString> h, const QString &dir)
    {
        for (auto i = h.begin(); i != h.end(); i++) {
            i.value() = dir + i.value();
            if (!QFile::exists(i.value()))
                throw ThemeError(QObject::tr("Error in iconconfig \"%1\", error: icon \"%2\" doesn't exists")
                                 .arg(":uitheme/icons/iconconfig.json", i.value()));
        }
        return h;
    }

    QString resolveIconId(const QString &iconId, const QHash<QString, QString> &iconMap)
    {
        QString iconPath = iconMap.value(iconId);
        QString iconIdPattern = iconId;
        while (iconPath.isEmpty() && !iconIdPattern.isEmpty()) {
            const int sepIndex = iconIdPattern.indexOf('.');
            iconIdPattern = "*" + ((sepIndex == -1) ? "" : iconIdPattern.right(iconIdPattern.size() - sepIndex));
            const auto patternIter = iconMap.find(iconIdPattern);
            if (patternIter != iconMap.end())
                iconPath = *patternIter;
            else
                iconIdPattern.remove(0, 2); // removes "*."
        }

        return iconPath;
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

#ifdef QBT_SYSTEMICONS
    m_useSystemTheme = pref->useSystemIconTheme();
    m_systemIconMap = systemIconConfig;
#endif

    QHash<QString, QString> customIconMap;

    if (useCustomUITheme) {
        try {
            if (!QResource::registerResource(pref->customUIThemePath(), "/uitheme"))
                throw ThemeError(tr("Failed to load UI theme from file: \"%1\".")
                                 .arg(pref->customUIThemePath()));
            m_useCustomStylesheet = QFile::exists(":uitheme/stylesheet.qss");
            m_flagsDir = ":uitheme/icons/flags/";

            // by chaining following functions, we ignore the erroed values in customIconMap
            customIconMap = resolveDirs(loadIconConfig(":uitheme/icons/iconconfig.json"), ":uitheme/icons/");

#ifdef QBT_SYSTEMICONS
            if (m_useSystemTheme)
                m_systemIconMap = loadIconConfig(":uitheme/icons/systemiconconfig.json");
#endif
        } catch (const ThemeError &err) {
            LogMsg(err.message(), Log::WARNING);
            LogMsg(tr("Encountered error while loading custom theme resources but qbittorrent will try to use as much resources from the custom theme package.")
                   , Log::WARNING);
            useCustomUITheme = false;
        }
    }

    // recheck since in case of error should fallback to system default
    if (!useCustomUITheme || !QDir(m_flagsDir).exists())
        m_flagsDir = ":icons/flags/";

    // merge defaultIconMap and customIconMap into m_iconMap
    m_iconMap = iconConfig;
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

QIcon UIThemeManager::getIcon(const QString &iconId, const QString &fallbackSysThemeIcon) const
{
#ifdef QBT_SYSTEMICONS
    if (m_useSystemTheme) {
        const QString themeIcon = resolveIconId(iconId, m_systemIconMap);

        // QIcon::fromTheme may return valid icons for not registerd system id as well,
        // for ex: edit-user-find
        QIcon icon = QIcon::fromTheme(themeIcon);
        if (icon.name().isEmpty())
            icon = QIcon::fromTheme(fallbackSysThemeIcon);
        if (!icon.name().isEmpty())
            return icon;
    }
#else
    Q_UNUSED(fallbackSysThemeIcon)
#endif

    // cache to avoid rescaling svg icons
    static QHash<QString, QIcon> iconCache;
    const QString iconPath = getIconPath(iconId);
    if (iconPath.isEmpty()) // error already reported by getIconPath
        return {};

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

    // cache to avoid rescaling svg icons
    static QHash<QString, QIcon> flagCache;
    const QString iconPath = m_flagsDir + countryIsoCode.toLower() + ".svg";
    if (!QFile::exists(iconPath)) {
        LogMsg(tr("No flag icon for country code - %1").arg(countryIsoCode), Log::WARNING);
        return {};
    }

    const auto iter = flagCache.find(iconPath);
    if (iter != flagCache.end())
        return *iter;

    const QIcon icon {iconPath};
    flagCache[iconPath] = icon;
    return icon;
}

QPixmap UIThemeManager::getScaledPixmap(const QString &iconId, const QWidget *widget, int baseHeight) const
{
    return Utils::Gui::scaledPixmapSvg(getIconPath(iconId), widget, baseHeight);
}

QString UIThemeManager::getIconPath(const QString &iconId) const
{
    QString iconPath = resolveIconId(iconId, m_iconMap);

    if (iconPath.isEmpty()) {
        LogMsg(tr("Can't resolve icon id - %1").arg(iconId), Log::WARNING);
        return {};
    }

    return iconPath;
}
