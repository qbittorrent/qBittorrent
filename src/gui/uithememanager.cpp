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

#include "uithememanager.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPalette>
#include <QPixmapCache>
#include <QResource>

#include "base/algorithm.h"
#include "base/global.h"
#include "base/logger.h"
#include "base/path.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/utils/fs.h"
#include "color.h"

namespace
{
    const QString CONFIG_FILE_NAME = u"config.json"_qs;
    const QString STYLESHEET_FILE_NAME = u"stylesheet.qss"_qs;

    bool isDarkTheme()
    {
        const QPalette palette = qApp->palette();
        const QColor &color = palette.color(QPalette::Active, QPalette::Base);
        return (color.lightness() < 127);
    }

    QByteArray readFile(const Path &filePath)
    {
        QFile file {filePath.data()};
        if (!file.exists())
            return {};

        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
            return file.readAll();

        LogMsg(UIThemeManager::tr("UITheme - Failed to open \"%1\". Reason: %2")
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
            LogMsg(UIThemeManager::tr("Couldn't parse UI Theme configuration file. Reason: %1")
                   .arg(jsonError.errorString()), Log::WARNING);
            return {};
        }

        if (!configJsonDoc.isObject())
        {
            LogMsg(UIThemeManager::tr("UI Theme configuration file has invalid format. Reason: %1")
                   .arg(UIThemeManager::tr("Root JSON value is not an object")), Log::WARNING);
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
                LogMsg(UIThemeManager::tr("Invalid color for ID \"%1\" is provided by theme")
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

    class DefaultThemeSource final : public UIThemeSource
    {
    public:
        DefaultThemeSource()
        {
            loadColors();
        }

        QByteArray readStyleSheet() override
        {
            return {};
        }

        QColor getColor(const QString &colorId, const ColorMode colorMode) const override
        {
            if (colorMode == ColorMode::Dark)
            {
                if (const QColor color = m_darkModeColors.value(colorId)
                        ; color.isValid())
                {
                    return color;
                }
            }

            return m_colors.value(colorId);
        }

        Path getIconPath(const QString &iconId, const ColorMode colorMode) const override
        {
            const Path iconsPath {u"icons"_qs};
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

            if (const Path iconPath = findIcon(iconId, (m_userPath / iconsPath))
                    ; !iconPath.isEmpty())
            {
                return iconPath;
            }

            return findIcon(iconId, (m_defaultPath / iconsPath));
        }

    private:
        void loadColors()
        {
            m_colors = {
                {u"Log.TimeStamp"_qs, Color::Primer::Light::fgSubtle},
                {u"Log.Normal"_qs, QApplication::palette().color(QPalette::Active, QPalette::WindowText)},
                {u"Log.Info"_qs, Color::Primer::Light::accentFg},
                {u"Log.Warning"_qs, Color::Primer::Light::severeFg},
                {u"Log.Critical"_qs, Color::Primer::Light::dangerFg},
                {u"Log.BannedPeer"_qs, Color::Primer::Light::dangerFg},

                {u"RSS.ReadArticle"_qs, QApplication::palette().color(QPalette::Inactive, QPalette::WindowText)},
                {u"RSS.UnreadArticle"_qs, QApplication::palette().color(QPalette::Active, QPalette::Link)},

                {u"TransferList.Downloading"_qs, Color::Primer::Light::successFg},
                {u"TransferList.StalledDownloading"_qs, Color::Primer::Light::successEmphasis},
                {u"TransferList.DownloadingMetadata"_qs, Color::Primer::Light::successFg},
                {u"TransferList.ForcedDownloadingMetadata"_qs, Color::Primer::Light::successFg},
                {u"TransferList.ForcedDownloading"_qs, Color::Primer::Light::successFg},
                {u"TransferList.Uploading"_qs, Color::Primer::Light::accentFg},
                {u"TransferList.StalledUploading"_qs, Color::Primer::Light::accentEmphasis},
                {u"TransferList.ForcedUploading"_qs, Color::Primer::Light::accentFg},
                {u"TransferList.QueuedDownloading"_qs, Color::Primer::Light::scaleYellow6},
                {u"TransferList.QueuedUploading"_qs, Color::Primer::Light::scaleYellow6},
                {u"TransferList.CheckingDownloading"_qs, Color::Primer::Light::successFg},
                {u"TransferList.CheckingUploading"_qs, Color::Primer::Light::successFg},
                {u"TransferList.CheckingResumeData"_qs, Color::Primer::Light::successFg},
                {u"TransferList.PausedDownloading"_qs, Color::Primer::Light::fgMuted},
                {u"TransferList.PausedUploading"_qs, Color::Primer::Light::doneFg},
                {u"TransferList.Moving"_qs, Color::Primer::Light::successFg},
                {u"TransferList.MissingFiles"_qs, Color::Primer::Light::dangerFg},
                {u"TransferList.Error"_qs, Color::Primer::Light::dangerFg}
            };

            m_darkModeColors = {
                {u"Log.TimeStamp"_qs, Color::Primer::Dark::fgSubtle},
                {u"Log.Normal"_qs, QApplication::palette().color(QPalette::Active, QPalette::WindowText)},
                {u"Log.Info"_qs, Color::Primer::Dark::accentFg},
                {u"Log.Warning"_qs, Color::Primer::Dark::severeFg},
                {u"Log.Critical"_qs, Color::Primer::Dark::dangerFg},
                {u"Log.BannedPeer"_qs, Color::Primer::Dark::dangerFg},

                {u"RSS.ReadArticle"_qs, QApplication::palette().color(QPalette::Inactive, QPalette::WindowText)},
                {u"RSS.UnreadArticle"_qs, QApplication::palette().color(QPalette::Active, QPalette::Link)},

                {u"TransferList.Downloading"_qs, Color::Primer::Dark::successFg},
                {u"TransferList.StalledDownloading"_qs, Color::Primer::Dark::successEmphasis},
                {u"TransferList.DownloadingMetadata"_qs, Color::Primer::Dark::successFg},
                {u"TransferList.ForcedDownloadingMetadata"_qs, Color::Primer::Dark::successFg},
                {u"TransferList.ForcedDownloading"_qs, Color::Primer::Dark::successFg},
                {u"TransferList.Uploading"_qs, Color::Primer::Dark::accentFg},
                {u"TransferList.StalledUploading"_qs, Color::Primer::Dark::accentEmphasis},
                {u"TransferList.ForcedUploading"_qs, Color::Primer::Dark::accentFg},
                {u"TransferList.QueuedDownloading"_qs, Color::Primer::Dark::scaleYellow6},
                {u"TransferList.QueuedUploading"_qs, Color::Primer::Dark::scaleYellow6},
                {u"TransferList.CheckingDownloading"_qs, Color::Primer::Dark::successFg},
                {u"TransferList.CheckingUploading"_qs, Color::Primer::Dark::successFg},
                {u"TransferList.CheckingResumeData"_qs, Color::Primer::Dark::successFg},
                {u"TransferList.PausedDownloading"_qs, Color::Primer::Dark::fgMuted},
                {u"TransferList.PausedUploading"_qs, Color::Primer::Dark::doneFg},
                {u"TransferList.Moving"_qs, Color::Primer::Dark::successFg},
                {u"TransferList.MissingFiles"_qs, Color::Primer::Dark::dangerFg},
                {u"TransferList.Error"_qs, Color::Primer::Dark::dangerFg}
            };

            const QByteArray configData = readFile(m_userPath / Path(CONFIG_FILE_NAME));
            if (configData.isEmpty())
                return;

            const QJsonObject config = parseThemeConfig(configData);

            auto colorOverrides = colorsFromJSON(config.value(u"colors").toObject());
            // Overriding Palette colors is not allowed in the default theme
            Algorithm::removeIf(colorOverrides, [](const QString &colorId, [[maybe_unused]] const QColor &color)
            {
                return colorId.startsWith(u"Palette.");
            });
            m_colors.insert(colorOverrides);

            auto darkModeColorOverrides = colorsFromJSON(config.value(u"colors.dark").toObject());
            // Overriding Palette colors is not allowed in the default theme
            Algorithm::removeIf(darkModeColorOverrides, [](const QString &colorId, [[maybe_unused]] const QColor &color)
            {
                return colorId.startsWith(u"Palette.");
            });
            m_darkModeColors.insert(darkModeColorOverrides);
        }

        const Path m_defaultPath {u":"_qs};
        const Path m_userPath = specialFolderLocation(SpecialFolder::Config) / Path(u"themes/default"_qs);
        QHash<QString, QColor> m_colors;
        QHash<QString, QColor> m_darkModeColors;
    };

    class CustomThemeSource : public UIThemeSource
    {
    public:
        QColor getColor(const QString &colorId, const ColorMode colorMode) const override
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

        Path getIconPath(const QString &iconId, const ColorMode colorMode) const override
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

        QByteArray readStyleSheet() override
        {
            return readFile(themeRootPath() / Path(STYLESHEET_FILE_NAME));
        }

    protected:
        virtual Path themeRootPath() const = 0;

        DefaultThemeSource *defaultThemeSource() const
        {
            return m_defaultThemeSource.get();
        }

    private:
        void loadColors()
        {
            const QByteArray configData = readFile(themeRootPath() / Path(CONFIG_FILE_NAME));
            if (configData.isEmpty())
                return;

            const QJsonObject config = parseThemeConfig(configData);

            m_colors.insert(colorsFromJSON(config.value(u"colors").toObject()));
            m_darkModeColors.insert(colorsFromJSON(config.value(u"colors.dark").toObject()));
        }

        const std::unique_ptr<DefaultThemeSource> m_defaultThemeSource = std::make_unique<DefaultThemeSource>();
        QHash<QString, QColor> m_colors;
        QHash<QString, QColor> m_darkModeColors;
    };

    class QRCThemeSource final : public CustomThemeSource
    {
    private:
        Path themeRootPath() const override
        {
            return Path(u":/uitheme"_qs);
        }
    };

    class FolderThemeSource : public CustomThemeSource
    {
    public:
        explicit FolderThemeSource(const Path &folderPath)
            : m_folder {folderPath}
        {
        }

        QByteArray readStyleSheet() override
        {
            // Directory used by stylesheet to reference internal resources
            // for example `icon: url(:/uitheme/file.svg)` will be expected to
            // point to a file `file.svg` in root directory of CONFIG_FILE_NAME
            const QString stylesheetResourcesDir = u":/uitheme"_qs;

            QByteArray styleSheetData = CustomThemeSource::readStyleSheet();
            return styleSheetData.replace(stylesheetResourcesDir.toUtf8(), themeRootPath().data().toUtf8());
        }

    private:
        Path themeRootPath() const override
        {
            return m_folder;
        }

        const Path m_folder;
    };
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
    : m_useCustomTheme {Preferences::instance()->useCustomUITheme()}
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS))
    , m_useSystemIcons {Preferences::instance()->useSystemIcons()}
#endif
{
    if (m_useCustomTheme)
    {
        const Path themePath = Preferences::instance()->customUIThemePath();

        if (themePath.hasExtension(u".qbtheme"_qs))
        {
            if (QResource::registerResource(themePath.data(), u"/uitheme"_qs))
                m_themeSource = std::make_unique<QRCThemeSource>();
            else
                LogMsg(tr("Failed to load UI theme from file: \"%1\"").arg(themePath.toString()), Log::WARNING);
        }
        else if (themePath.filename() == CONFIG_FILE_NAME)
        {
            m_themeSource = std::make_unique<FolderThemeSource>(themePath.parentPath());
        }
    }

    if (!m_themeSource)
        m_themeSource = std::make_unique<DefaultThemeSource>();

    if (m_useCustomTheme)
    {
        applyPalette();
        applyStyleSheet();
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

QIcon UIThemeManager::getIcon(const QString &iconId, [[maybe_unused]] const QString &fallback) const
{
    const auto colorMode = isDarkTheme() ? ColorMode::Dark : ColorMode::Light;
    auto &icons = (colorMode == ColorMode::Dark) ? m_darkModeIcons : m_icons;

    const auto iter = icons.find(iconId);
    if (iter != icons.end())
        return *iter;

#if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS))
    // Don't cache system icons because users might change them at run time
    if (m_useSystemIcons)
    {
        auto icon = QIcon::fromTheme(iconId);
        if (icon.name() != iconId)
            icon = QIcon::fromTheme(fallback, QIcon(m_themeSource->getIconPath(iconId, colorMode).data()));
        return icon;
    }
#endif

    const QIcon icon {m_themeSource->getIconPath(iconId, colorMode).data()};
    icons[iconId] = icon;
    return icon;
}

QIcon UIThemeManager::getFlagIcon(const QString &countryIsoCode) const
{
    if (countryIsoCode.isEmpty())
        return {};

    const QString key = countryIsoCode.toLower();
    const auto iter = m_flags.find(key);
    if (iter != m_flags.end())
        return *iter;

    const QIcon icon {u":/icons/flags/" + key + u".svg"};
    m_flags[key] = icon;
    return icon;
}

QPixmap UIThemeManager::getScaledPixmap(const QString &iconId, const int height) const
{
    // (workaround) svg images require the use of `QIcon()` to load and scale losslessly,
    // otherwise other image classes will convert it to pixmap first and follow-up scaling will become lossy.

    Q_ASSERT(height > 0);

    const QString cacheKey = iconId + u'@' + QString::number(height);

    QPixmap pixmap;
    if (!QPixmapCache::find(cacheKey, &pixmap))
    {
        pixmap = getIcon(iconId).pixmap(height);
        QPixmapCache::insert(cacheKey, pixmap);
    }

    return pixmap;
}

QColor UIThemeManager::getColor(const QString &id) const
{
    const QColor color = m_themeSource->getColor(id, (isDarkTheme() ? ColorMode::Dark : ColorMode::Light));
    Q_ASSERT(color.isValid());

    return color;
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
        // For backward compatibility, the palette color overrides are read from the section of the "light mode" colors
        const QColor newColor = m_themeSource->getColor(colorDescriptor.id, ColorMode::Light);
        if (newColor.isValid())
            palette.setColor(colorDescriptor.colorGroup, colorDescriptor.colorRole, newColor);
    }

    qApp->setPalette(palette);
}
