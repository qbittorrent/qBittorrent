/*
 * Bittorrent Client using Qt and libtorrent.
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

#include "uithememanager.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPalette>
#include <QResource>

#include "base/global.h"
#include "base/logger.h"
#include "base/path.h"
#include "base/preferences.h"
#include "base/utils/fs.h"

namespace
{
    const Path DEFAULT_ICONS_DIR {u":icons"_qs};
    const QString CONFIG_FILE_NAME = u"config.json"_qs;
    const QString STYLESHEET_FILE_NAME = u"stylesheet.qss"_qs;

    // Directory used by stylesheet to reference internal resources
    // for example `icon: url(:/uitheme/file.svg)` will be expected to
    // point to a file `file.svg` in root directory of CONFIG_FILE_NAME
    const QString STYLESHEET_RESOURCES_DIR = u":/uitheme"_qs;

    const Path THEME_ICONS_DIR {u"icons"_qs};

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

    QByteArray readFile(const Path &filePath)
    {
        QFile file {filePath.data()};
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            LogMsg(UIThemeManager::tr("UITheme - Failed to open \"%1\". Reason: %2")
                    .arg(filePath.filename(), file.errorString())
                   , Log::WARNING);
            return {};
        }

        return file.readAll();
    }

    class QRCThemeSource final : public UIThemeSource
    {
    public:
        QByteArray readStyleSheet() override
        {
            return readFile(m_qrcThemeDir / Path(STYLESHEET_FILE_NAME));
        }

        QByteArray readConfig() override
        {
            return readFile(m_qrcThemeDir / Path(CONFIG_FILE_NAME));
        }

        Path iconPath(const QString &iconId) const override
        {
            return findIcon(iconId, m_qrcIconsDir);
        }

    private:
        const Path m_qrcThemeDir {u":/uitheme"_qs};
        const Path m_qrcIconsDir = m_qrcThemeDir / THEME_ICONS_DIR;
    };

    class FolderThemeSource final : public UIThemeSource
    {
    public:
        explicit FolderThemeSource(const Path &folderPath)
            : m_folder {folderPath}
            , m_iconsDir {m_folder / THEME_ICONS_DIR}
        {
        }

        QByteArray readStyleSheet() override
        {
            QByteArray styleSheetData = readFile(m_folder / Path(STYLESHEET_FILE_NAME));
            return styleSheetData.replace(STYLESHEET_RESOURCES_DIR.toUtf8(), m_folder.data().toUtf8());
        }

        QByteArray readConfig() override
        {
            return readFile(m_folder / Path(CONFIG_FILE_NAME));
        }

        Path iconPath(const QString &iconId) const override
        {
            return findIcon(iconId, m_iconsDir);
        }

    private:
        const Path m_folder;
        const Path m_iconsDir;
    };


    std::unique_ptr<UIThemeSource> createUIThemeSource(const Path &themePath)
    {
        if (themePath.filename() == CONFIG_FILE_NAME)
            return std::make_unique<FolderThemeSource>(themePath);

        if ((themePath.hasExtension(u".qbtheme"_qs))
                && QResource::registerResource(themePath.data(), u"/uitheme"_qs))
        {
            return std::make_unique<QRCThemeSource>();
        }

        return nullptr;
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
{
    if (m_useCustomTheme)
    {
        const Path themePath = Preferences::instance()->customUIThemePath();
        m_themeSource = createUIThemeSource(themePath);
        if (!m_themeSource)
        {
            LogMsg(tr("Failed to load UI theme from file: \"%1\"").arg(themePath.toString()), Log::WARNING);
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
    qApp->setStyleSheet(QString::fromUtf8(m_themeSource->readStyleSheet()));
}

QIcon UIThemeManager::getIcon(const QString &iconId, const QString &fallback) const
{
    // Cache to avoid rescaling svg icons
    const auto iter = m_iconCache.find(iconId);
    if (iter != m_iconCache.end())
        return *iter;

    const QIcon icon {getIconPathFromResources(iconId, fallback).data()};
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

    const QIcon icon {u":/icons/flags/" + key + u".svg"};
    m_flagCache[key] = icon;
    return icon;
}

QColor UIThemeManager::getColor(const QString &id, const QColor &defaultColor) const
{
    return m_colors.value(id, defaultColor);
}

#ifndef Q_OS_MACOS
QIcon UIThemeManager::getSystrayIcon() const
{
    const TrayIcon::Style style = Preferences::instance()->trayIconStyle();
    switch (style)
    {
#if defined(Q_OS_UNIX)
    case TrayIcon::Style::Normal:
        return QIcon::fromTheme(u"qbittorrent-tray"_qs);
    case TrayIcon::Style::MonoDark:
        return QIcon::fromTheme(u"qbittorrent-tray-dark"_qs);
    case TrayIcon::Style::MonoLight:
        return QIcon::fromTheme(u"qbittorrent-tray-light"_qs);
#else
    case TrayIcon::Style::Normal:
        return getIcon(u"qbittorrent-tray"_qs);
    case TrayIcon::Style::MonoDark:
        return getIcon(u"qbittorrent-tray-dark"_qs);
    case TrayIcon::Style::MonoLight:
        return getIcon(u"qbittorrent-tray-light"_qs);
#endif
    default:
        break;
    }

    // As a failsafe in case the enum is invalid
    return getIcon(u"qbittorrent-tray"_qs);
}
#endif

Path UIThemeManager::getIconPath(const QString &iconId) const
{
    return getIconPathFromResources(iconId, {});
}

Path UIThemeManager::getIconPathFromResources(const QString &iconId, const QString &fallback) const
{
    if (m_useCustomTheme && m_themeSource)
    {
        const Path customIcon = m_themeSource->iconPath(iconId);
        if (!customIcon.isEmpty())
            return customIcon;

        if (!fallback.isEmpty())
        {
            const Path fallbackIcon = m_themeSource->iconPath(fallback);
            if (!fallbackIcon.isEmpty())
                return fallbackIcon;
        }
    }

    return findIcon(iconId, DEFAULT_ICONS_DIR);
}

void UIThemeManager::loadColorsFromJSONConfig()
{
    const QByteArray config = m_themeSource->readConfig();
    if (config.isEmpty())
        return;

    QJsonParseError jsonError;
    const QJsonDocument configJsonDoc = QJsonDocument::fromJson(config, &jsonError);
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

    const QJsonObject colors = configJsonDoc.object().value(u"colors").toObject();
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
        {u"Palette.Window"_qs, QPalette::Window, QPalette::Normal},
        {u"Palette.WindowText"_qs, QPalette::WindowText, QPalette::Normal},
        {u"Palette.Base"_qs, QPalette::Base, QPalette::Normal},
        {u"Palette.AlternateBase"_qs, QPalette::AlternateBase, QPalette::Normal},
        {u"Palette.Text"_qs, QPalette::Text, QPalette::Normal},
        {u"Palette.ToolTipBase"_qs, QPalette::ToolTipBase, QPalette::Normal},
        {u"Palette.ToolTipText"_qs, QPalette::ToolTipText, QPalette::Normal},
        {u"Palette.BrightText"_qs, QPalette::BrightText, QPalette::Normal},
        {u"Palette.Highlight"_qs, QPalette::Highlight, QPalette::Normal},
        {u"Palette.HighlightedText"_qs, QPalette::HighlightedText, QPalette::Normal},
        {u"Palette.Button"_qs, QPalette::Button, QPalette::Normal},
        {u"Palette.ButtonText"_qs, QPalette::ButtonText, QPalette::Normal},
        {u"Palette.Link"_qs, QPalette::Link, QPalette::Normal},
        {u"Palette.LinkVisited"_qs, QPalette::LinkVisited, QPalette::Normal},
        {u"Palette.Light"_qs, QPalette::Light, QPalette::Normal},
        {u"Palette.Midlight"_qs, QPalette::Midlight, QPalette::Normal},
        {u"Palette.Mid"_qs, QPalette::Mid, QPalette::Normal},
        {u"Palette.Dark"_qs, QPalette::Dark, QPalette::Normal},
        {u"Palette.Shadow"_qs, QPalette::Shadow, QPalette::Normal},
        {u"Palette.WindowTextDisabled"_qs, QPalette::WindowText, QPalette::Disabled},
        {u"Palette.TextDisabled"_qs, QPalette::Text, QPalette::Disabled},
        {u"Palette.ToolTipTextDisabled"_qs, QPalette::ToolTipText, QPalette::Disabled},
        {u"Palette.BrightTextDisabled"_qs, QPalette::BrightText, QPalette::Disabled},
        {u"Palette.HighlightedTextDisabled"_qs, QPalette::HighlightedText, QPalette::Disabled},
        {u"Palette.ButtonTextDisabled"_qs, QPalette::ButtonText, QPalette::Disabled}
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
