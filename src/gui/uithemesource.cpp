/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2023  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2019, 2021  Prince Gupta <jagannatharjun11@gmail.com>
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

#include "uithemesource.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include "base/global.h"
#include "base/logger.h"
#include "base/profile.h"

namespace
{
    QByteArray readFile(const Path &filePath)
    {
        QFile file {filePath.data()};
        if (!file.exists())
            return {};

        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
            return file.readAll();

        LogMsg(UIThemeSource::tr("UITheme - Failed to open \"%1\". Reason: %2")
               .arg(filePath.filename(), file.errorString())
               , Log::WARNING);
        return {};
    }

    QJsonObject parseThemeConfig(const QByteArray &data)
    {
        if (data.isEmpty())
            return {};

        QJsonParseError jsonError;
        const QJsonDocument configJsonDoc = QJsonDocument::fromJson(data, &jsonError);
        if (jsonError.error != QJsonParseError::NoError)
        {
            LogMsg(UIThemeSource::tr("Couldn't parse UI Theme configuration file. Reason: %1")
                   .arg(jsonError.errorString()), Log::WARNING);
            return {};
        }

        if (!configJsonDoc.isObject())
        {
            LogMsg(UIThemeSource::tr("UI Theme configuration file has invalid format. Reason: %1")
                   .arg(UIThemeSource::tr("Root JSON value is not an object")), Log::WARNING);
            return {};
        }

        return configJsonDoc.object();
    }

    QHash<QString, QColor> colorsFromJSON(const QJsonObject &jsonObj)
    {
        QHash<QString, QColor> colors;
        for (auto colorNode = jsonObj.constBegin(); colorNode != jsonObj.constEnd(); ++colorNode)
        {
            const QColor color {colorNode.value().toString()};
            if (!color.isValid())
            {
                LogMsg(UIThemeSource::tr("Invalid color for ID \"%1\" is provided by theme")
                       .arg(colorNode.key()), Log::WARNING);
                continue;
            }

            colors.insert(colorNode.key(), color);
        }

        return colors;
    }

    Path findIcon(const QString &iconId, const Path &dir)
    {
        const Path pathSvg = dir / Path(iconId + u".svg");
        if (pathSvg.exists())
            return pathSvg;

        const Path pathPng = dir / Path(iconId + u".png");
        if (pathPng.exists())
            return pathPng;

        return {};
    }
}

DefaultThemeSource::DefaultThemeSource()
    : m_defaultPath {u":"_qs}
    , m_userPath {specialFolderLocation(SpecialFolder::Config) / Path(u"themes/default"_qs)}
    , m_colors {defaultUIThemeColors()}
{
    loadColors();
}

QByteArray DefaultThemeSource::readStyleSheet()
{
    return {};
}

QColor DefaultThemeSource::getColor(const QString &colorId, const ColorMode colorMode) const
{
    return (colorMode == ColorMode::Light)
            ? m_colors.value(colorId).light : m_colors.value(colorId).dark;
}

Path DefaultThemeSource::getIconPath(const QString &iconId, const ColorMode colorMode) const
{
    const Path iconsPath {u"icons"_qs};
    const Path lightModeIconsPath = iconsPath / Path(u"light"_qs);
    const Path darkModeIconsPath = iconsPath / Path(u"dark"_qs);

    if (colorMode == ColorMode::Dark)
    {
        if (const Path iconPath = findIcon(iconId, (m_userPath / darkModeIconsPath))
                ; !iconPath.isEmpty())
        {
            return iconPath;
        }

        if (const Path iconPath = findIcon(iconId, (m_defaultPath / darkModeIconsPath))
                ; !iconPath.isEmpty())
        {
            return iconPath;
        }
    }
    else
    {
        if (const Path iconPath = findIcon(iconId, (m_userPath / lightModeIconsPath))
                ; !iconPath.isEmpty())
        {
            return iconPath;
        }
    }

    return findIcon(iconId, (m_defaultPath / iconsPath));
}

void DefaultThemeSource::loadColors()
{
    const QByteArray configData = readFile(m_userPath / Path(CONFIG_FILE_NAME));
    if (configData.isEmpty())
        return;

    const QJsonObject config = parseThemeConfig(configData);

    QHash<QString, QColor> lightModeColorOverrides = colorsFromJSON(config.value(KEY_COLORS_LIGHT).toObject());
    for (auto overridesIt = lightModeColorOverrides.cbegin(); overridesIt != lightModeColorOverrides.cend(); ++overridesIt)
    {
        auto it = m_colors.find(overridesIt.key());
        if (it != m_colors.end())
            it.value().light = overridesIt.value();
    }

    QHash<QString, QColor> darkModeColorOverrides = colorsFromJSON(config.value(KEY_COLORS_DARK).toObject());
    for (auto overridesIt = darkModeColorOverrides.cbegin(); overridesIt != darkModeColorOverrides.cend(); ++overridesIt)
    {
        auto it = m_colors.find(overridesIt.key());
        if (it != m_colors.end())
            it.value().dark = overridesIt.value();
    }
}

QColor CustomThemeSource::getColor(const QString &colorId, const ColorMode colorMode) const
{
    if (colorMode == ColorMode::Dark)
    {
        if (const QColor color = m_darkModeColors.value(colorId)
                ; color.isValid())
        {
            return color;
        }
    }

    if (const QColor color = m_colors.value(colorId)
            ; color.isValid())
    {
        return color;
    }

    return defaultThemeSource()->getColor(colorId, colorMode);
}

Path CustomThemeSource::getIconPath(const QString &iconId, const ColorMode colorMode) const
{
    const Path iconsPath {u"icons"_qs};
    const Path darkModeIconsPath = iconsPath / Path(u"dark"_qs);

    if (colorMode == ColorMode::Dark)
    {
        if (const Path iconPath = findIcon(iconId, (themeRootPath() / darkModeIconsPath))
                ; !iconPath.isEmpty())
        {
            return iconPath;
        }
    }

    if (const Path iconPath = findIcon(iconId, (themeRootPath() / iconsPath))
            ; !iconPath.isEmpty())
    {
        return iconPath;
    }

    return defaultThemeSource()->getIconPath(iconId, colorMode);
}

QByteArray CustomThemeSource::readStyleSheet()
{
    return readFile(themeRootPath() / Path(STYLESHEET_FILE_NAME));
}

DefaultThemeSource *CustomThemeSource::defaultThemeSource() const
{
    return m_defaultThemeSource.get();
}

void CustomThemeSource::loadColors()
{
    const QByteArray configData = readFile(themeRootPath() / Path(CONFIG_FILE_NAME));
    if (configData.isEmpty())
        return;

    const QJsonObject config = parseThemeConfig(configData);

    m_colors.insert(colorsFromJSON(config.value(KEY_COLORS).toObject()));
    m_darkModeColors.insert(colorsFromJSON(config.value(KEY_COLORS_DARK).toObject()));
}

Path QRCThemeSource::themeRootPath() const
{
    return Path(u":/uitheme"_qs);
}

FolderThemeSource::FolderThemeSource(const Path &folderPath)
    : m_folder {folderPath}
{
}

QByteArray FolderThemeSource::readStyleSheet()
{
    // Directory used by stylesheet to reference internal resources
    // for example `icon: url(:/uitheme/file.svg)` will be expected to
    // point to a file `file.svg` in root directory of CONFIG_FILE_NAME
    const QString stylesheetResourcesDir = u":/uitheme"_qs;

    QByteArray styleSheetData = CustomThemeSource::readStyleSheet();
    return styleSheetData.replace(stylesheetResourcesDir.toUtf8(), themeRootPath().data().toUtf8());
}

Path FolderThemeSource::themeRootPath() const
{
    return m_folder;
}
