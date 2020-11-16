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
#include <QJsonDocument>
#include <QJsonObject>
#include <QPalette>
#include <QResource>

#include "base/logger.h"
#include "base/preferences.h"
#include "base/utils/fs.h"

namespace
{
    const QString ICONS_DIR = QStringLiteral(":icons/");
    const QString THEME_ICONS_DIR = QStringLiteral(":uitheme/icons/");
    const QString CONFIG_FILE_NAME = QStringLiteral(":uitheme/config.json");

    QString findIcon(const QString &iconId, const QString &dir)
    {
        const QString pathSvg = dir + iconId + QLatin1String(".svg");
        if (QFile::exists(pathSvg))
            return pathSvg;

        const QString pathPng = dir + iconId + QLatin1String(".png");
        if (QFile::exists(pathPng))
            return pathPng;

        return {};
    }
}

UIThemeManager *UIThemeManager::m_instance = nullptr;

void UIThemeManager::freeInstance()
{
    delete m_instance;
    m_instance = nullptr;
}

void UIThemeManager::initInstance()
{
    if (!m_instance)
        m_instance = new UIThemeManager;
}

UIThemeManager::UIThemeManager()
    : m_useCustomTheme(Preferences::instance()->useCustomUITheme())
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS))
    , m_useSystemTheme(Preferences::instance()->useSystemIconTheme())
#endif
{
    const Preferences *const pref = Preferences::instance();
    if (m_useCustomTheme)
    {
        if (!QResource::registerResource(pref->customUIThemePath(), "/uitheme"))
        {
            LogMsg(tr("Failed to load UI theme from file: \"%1\"").arg(pref->customUIThemePath()), Log::WARNING);
        }
        else
        {
            loadColorsFromJSONConfig();
            applyPalette();
            applyStyleSheet();
        }
    }
}

UIThemeManager *UIThemeManager::instance()
{
    return m_instance;
}

void UIThemeManager::applyStyleSheet() const
{
    QFile qssFile(":uitheme/stylesheet.qss");
    if (!qssFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qApp->setStyleSheet({});
        LogMsg(tr("Couldn't apply theme stylesheet. stylesheet.qss couldn't be opened. Reason: %1").arg(qssFile.errorString())
               , Log::WARNING);
        return;
    }

    qApp->setStyleSheet(qssFile.readAll());
}

QIcon UIThemeManager::getIcon(const QString &iconId, const QString &fallback) const
{
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS))
    if (m_useSystemTheme)
    {
        QIcon icon = QIcon::fromTheme(iconId);
        if (icon.name() != iconId)
            icon = QIcon::fromTheme(fallback, QIcon(getIconPathFromResources(iconId, fallback)));
        return icon;
    }
#endif

    // Cache to avoid rescaling svg icons
    // And don't cache system icons because users might change them at run time
    const auto iter = m_iconCache.find(iconId);
    if (iter != m_iconCache.end())
        return *iter;

    const QIcon icon {getIconPathFromResources(iconId, fallback)};
    m_iconCache[iconId] = icon;
    return icon;
}

QIcon UIThemeManager::getFlagIcon(const QString &countryIsoCode) const
{
    if (countryIsoCode.isEmpty()) return {};

    const QString key = countryIsoCode.toLower();
    const auto iter = m_flagCache.find(key);
    if (iter != m_flagCache.end())
        return *iter;

    const QIcon icon {QLatin1String(":/icons/flags/") + key + QLatin1String(".svg")};
    m_flagCache[key] = icon;
    return icon;
}

QColor UIThemeManager::getColor(const QString &id, const QColor &defaultColor) const
{
    return m_colors.value(id, defaultColor);
}

QString UIThemeManager::getIconPath(const QString &iconId) const
{
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS))
    if (m_useSystemTheme)
    {
        QString path = Utils::Fs::tempPath() + iconId + QLatin1String(".png");
        if (!QFile::exists(path))
        {
            const QIcon icon = QIcon::fromTheme(iconId);
            if (!icon.isNull())
                icon.pixmap(32).save(path);
            else
                path = getIconPathFromResources(iconId);
        }

        return path;
    }
#endif
    return getIconPathFromResources(iconId, {});
}

QString UIThemeManager::getIconPathFromResources(const QString &iconId, const QString &fallback) const
{
    if (m_useCustomTheme)
    {
        const QString customIcon = findIcon(iconId, THEME_ICONS_DIR);
        if (!customIcon.isEmpty())
            return customIcon;

        if (!fallback.isEmpty())
        {
            const QString fallbackIcon = findIcon(fallback, THEME_ICONS_DIR);
            if (!fallbackIcon.isEmpty())
                return fallbackIcon;
        }
    }

    return findIcon(iconId, ICONS_DIR);
}

void UIThemeManager::loadColorsFromJSONConfig()
{
    QFile configFile(CONFIG_FILE_NAME);
    if (!configFile.open(QIODevice::ReadOnly))
    {
        LogMsg(tr("Failed to open \"%1\". Reason: %2").arg(CONFIG_FILE_NAME, configFile.errorString()), Log::WARNING);
        return;
    }

    QJsonParseError jsonError;
    const QJsonDocument configJsonDoc = QJsonDocument::fromJson(configFile.readAll(), &jsonError);
    if (jsonError.error != QJsonParseError::NoError)
    {
        LogMsg(tr("\"%1\" has invalid format. Reason: %2").arg(CONFIG_FILE_NAME, jsonError.errorString()), Log::WARNING);
        return;
    }
    if (!configJsonDoc.isObject())
    {
        LogMsg(tr("\"%1\" has invalid format. Reason: %2").arg(CONFIG_FILE_NAME, tr("Root JSON value is not an object")), Log::WARNING);
        return;
    }

    const QJsonObject colors = configJsonDoc.object().value("colors").toObject();
    for (auto color = colors.constBegin(); color != colors.constEnd(); ++color)
    {
        const QColor providedColor(color.value().toString());
        if (!providedColor.isValid())
        {
            LogMsg(tr("Invalid color for ID \"%1\" is provided by theme").arg(color.key()), Log::WARNING);
            continue;
        }
        m_colors.insert(color.key(), providedColor);
    }
}

void UIThemeManager::applyPalette() const
{
    struct ColorDescriptor
    {
        QString id;
        QPalette::ColorRole colorRole;
        QPalette::ColorGroup colorGroup;
    };

    const ColorDescriptor paletteColorDescriptors[] =
    {
        {QLatin1String("Palette.Window"), QPalette::Window, QPalette::Normal},
        {QLatin1String("Palette.WindowText"), QPalette::WindowText, QPalette::Normal},
        {QLatin1String("Palette.Base"), QPalette::Base, QPalette::Normal},
        {QLatin1String("Palette.AlternateBase"), QPalette::AlternateBase, QPalette::Normal},
        {QLatin1String("Palette.Text"), QPalette::Text, QPalette::Normal},
        {QLatin1String("Palette.ToolTipBase"), QPalette::ToolTipBase, QPalette::Normal},
        {QLatin1String("Palette.ToolTipText"), QPalette::ToolTipText, QPalette::Normal},
        {QLatin1String("Palette.BrightText"), QPalette::BrightText, QPalette::Normal},
        {QLatin1String("Palette.Highlight"), QPalette::Highlight, QPalette::Normal},
        {QLatin1String("Palette.HighlightedText"), QPalette::HighlightedText, QPalette::Normal},
        {QLatin1String("Palette.Button"), QPalette::Button, QPalette::Normal},
        {QLatin1String("Palette.ButtonText"), QPalette::ButtonText, QPalette::Normal},
        {QLatin1String("Palette.Link"), QPalette::Link, QPalette::Normal},
        {QLatin1String("Palette.LinkVisited"), QPalette::LinkVisited, QPalette::Normal},
        {QLatin1String("Palette.Light"), QPalette::Light, QPalette::Normal},
        {QLatin1String("Palette.Midlight"), QPalette::Midlight, QPalette::Normal},
        {QLatin1String("Palette.Mid"), QPalette::Mid, QPalette::Normal},
        {QLatin1String("Palette.Dark"), QPalette::Dark, QPalette::Normal},
        {QLatin1String("Palette.Shadow"), QPalette::Shadow, QPalette::Normal},
        {QLatin1String("Palette.WindowTextDisabled"), QPalette::WindowText, QPalette::Disabled},
        {QLatin1String("Palette.TextDisabled"), QPalette::Text, QPalette::Disabled},
        {QLatin1String("Palette.ToolTipTextDisabled"), QPalette::ToolTipText, QPalette::Disabled},
        {QLatin1String("Palette.BrightTextDisabled"), QPalette::BrightText, QPalette::Disabled},
        {QLatin1String("Palette.HighlightedTextDisabled"), QPalette::HighlightedText, QPalette::Disabled},
        {QLatin1String("Palette.ButtonTextDisabled"), QPalette::ButtonText, QPalette::Disabled}
    };

    QPalette palette = qApp->palette();
    for (const ColorDescriptor &colorDescriptor : paletteColorDescriptors)
    {
        const QColor defaultColor = palette.color(colorDescriptor.colorGroup, colorDescriptor.colorRole);
        const QColor newColor = getColor(colorDescriptor.id, defaultColor);
        palette.setColor(colorDescriptor.colorGroup, colorDescriptor.colorRole, newColor);
    }
    qApp->setPalette(palette);
}
