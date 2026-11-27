/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2023-2026  Vladimir Golovnev <glassez@yandex.ru>
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
#include <QPalette>
#include <QPixmapCache>
#include <QResource>
#include <QStyle>
#include <QStyleHints>

#include "base/logger.h"
#include "base/path.h"
#include "base/profile.h"
#include "base/preferences.h"
#include "uithemecommon.h"

#ifdef Q_OS_WIN
#include "base/utils/reg.h"
#endif

using namespace Qt::Literals::StringLiterals;

namespace
{
    bool isDarkTheme()
    {
        if (const auto scheme = qApp->styleHints()->colorScheme(); scheme != Qt::ColorScheme::Unknown)
            return (scheme == Qt::ColorScheme::Dark);
        const QPalette palette = qApp->palette();
        const QColor &color = palette.color(QPalette::Active, QPalette::Base);
        return (color.lightness() < 127);
    }

#ifdef Q_OS_MACOS
    QString modernMacStyleSheet(const bool dark)
    {
        const QString canvas = dark ? u"#1C1C1E"_s : u"#F5F5F7"_s;
        const QString card = dark ? u"#2C2C2E"_s : u"#FFFFFF"_s;
        const QString headerBg = dark ? u"#242426"_s : u"#FAFAFC"_s;
        const QString border = dark ? u"#3A3A3C"_s : u"#E5E5EA"_s;
        const QString inputBorder = dark ? u"#48484A"_s : u"#D1D1D6"_s;
        const QString text = dark ? u"#F5F5F7"_s : u"#1D1D1F"_s;
        const QString muted = dark ? u"#98989D"_s : u"#6E6E73"_s;
        const QString accent = dark ? u"#6366F1"_s : u"#5856D6"_s;
        const QString selBg = dark ? u"#313052"_s : u"#ECEEFF"_s;
        const QString selText = dark ? u"#E0E7FF"_s : u"#3B38C8"_s;
        const QString hoverBg = dark ? u"#38383A"_s : u"#F0F0F5"_s;

        return uR"(
            QMainWindow, QDialog {
                background-color: %1;
                color: %6;
            }
            QToolBar {
                background-color: %3;
                border: none;
                border-bottom: 1px solid %4;
                spacing: 6px;
                padding: 6px 18px 6px 8px;
                min-height: 46px;
            }
            QToolBar #macNavPill {
                background-color: %2;
                border: 1px solid %4;
                border-radius: 10px;
                min-height: 28px;
            }
            QToolBar #macNavPill QToolButton {
                border: none;
                border-radius: 7px;
                padding: 4px 6px;
                min-width: 22px;
                min-height: 22px;
            }
            QToolBar #macTitleLabel {
                color: %6;
                font-weight: 600;
                font-size: 13px;
                padding: 0 10px;
            }
            QToolButton {
                border: none;
                border-radius: 8px;
                padding: 5px;
                min-width: 24px;
                min-height: 24px;
                color: %6;
            }
            QToolButton:hover {
                background-color: %11;
            }
            QToolButton:checked {
                background-color: %9;
                color: %10;
            }
            QSplitter::handle {
                background-color: transparent;
            }
            QTreeView, QTableView, QListView {
                background-color: %2;
                alternate-background-color: %3;
                color: %6;
                border: 1px solid %4;
                border-radius: 10px;
                padding: 4px;
                outline: none;
            }
            QTreeView::item, QListView::item {
                border-radius: 6px;
                padding: 3px 6px;
            }
            QTreeView::item:hover, QListView::item:hover {
                background-color: %11;
            }
            QTreeView::item:selected, QListView::item:selected {
                background-color: %9;
                color: %10;
            }
            QHeaderView {
                background-color: transparent;
            }
            QHeaderView::section {
                background-color: %3;
                color: %7;
                border: none;
                border-bottom: 1px solid %4;
                border-right: 1px solid %4;
                padding: 6px 8px;
                font-weight: 600;
            }
            QHeaderView::section:last {
                border-right: none;
            }
            QTabWidget::pane {
                border: none;
                background-color: transparent;
            }
            QTabBar::tab {
                background-color: transparent;
                color: %7;
                border: 1px solid transparent;
                border-radius: 8px;
                padding: 6px 14px;
                margin: 2px 4px;
            }
            QTabBar::tab:hover:!selected {
                background-color: %11;
                color: %6;
            }
            QTabBar::tab:selected {
                background-color: %2;
                color: %6;
                border: 1px solid %4;
                font-weight: 600;
            }
            QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox {
                background-color: %2;
                color: %6;
                border: 1px solid %5;
                border-radius: 8px;
                padding: 4px 8px;
                selection-background-color: %8;
                selection-color: #FFFFFF;
            }
            QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus {
                border: 1.5px solid %8;
            }
            QPushButton {
                background-color: %2;
                color: %6;
                border: 1px solid %5;
                border-radius: 8px;
                padding: 5px 14px;
            }
            QPushButton:hover {
                background-color: %11;
            }
            QPushButton:default {
                background-color: %8;
                color: #FFFFFF;
                border: 1px solid %8;
            }
            QStatusBar {
                background-color: %1;
                color: %7;
                border-top: 1px solid %4;
            }
            QScrollBar:vertical {
                background: transparent;
                width: 8px;
                margin: 4px 1px;
            }
            QScrollBar::handle:vertical {
                background: rgba(142, 142, 147, 0.45);
                border-radius: 4px;
                min-height: 28px;
            }
            QScrollBar::handle:vertical:hover {
                background: rgba(142, 142, 147, 0.75);
            }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,
            QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
                background: none;
                height: 0px;
            }
            QScrollBar:horizontal {
                background: transparent;
                height: 8px;
                margin: 1px 4px;
            }
            QScrollBar::handle:horizontal {
                background: rgba(142, 142, 147, 0.45);
                border-radius: 4px;
                min-width: 28px;
            }
            QScrollBar::handle:horizontal:hover {
                background: rgba(142, 142, 147, 0.75);
            }
            QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal,
            QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
                background: none;
                width: 0px;
            }
        )"_s.arg(canvas, card, headerBg, border, inputBorder, text, muted, accent, selBg, selText, hoverBg);
    }
#endif

    Path resolveThemePath(const Path &themePath)
    {
        return (themePath.isAbsolute() ? themePath : (Profile::instance()->location(SpecialFolder::Config) / themePath));
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
    : m_useCustomTheme {Preferences::instance()->useCustomUITheme()}
#ifdef QBT_HAS_COLORSCHEME_OPTION
    , m_colorSchemeSetting {u"Appearance/ColorScheme"_s}
#endif
    , m_trayIconStyleSetting {u"Appearance/TrayIconStyle"_s}
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS))
    , m_useSystemIcons {Preferences::instance()->useSystemIcons()}
#endif
{
#ifdef Q_OS_WIN
    updateSystemColorMode();
#endif

    if (const QString styleName = Preferences::instance()->getStyle(); styleName.compare(u"system", Qt::CaseInsensitive) != 0)
    {
        if (!QApplication::setStyle(styleName))
            LogMsg(tr("Set app style failed. Unknown style: \"%1\"").arg(styleName), Log::WARNING);
    }

#ifdef QBT_HAS_COLORSCHEME_OPTION
    applyColorScheme();
#endif

    connect(QApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, &UIThemeManager::onColorSchemeChanged);

    if (m_useCustomTheme)
    {
        const Path themePath = resolveThemePath(Preferences::instance()->customUIThemePath());

        if (themePath.hasExtension(u".qbtheme"_s))
        {
            if (QResource::registerResource(themePath.data(), u"/uitheme"_s))
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
#ifdef Q_OS_MACOS
    else
    {
        qApp->setStyleSheet(modernMacStyleSheet(isDarkTheme()));
    }
#endif
}

UIThemeManager *UIThemeManager::instance()
{
    return m_instance;
}

#ifdef QBT_HAS_COLORSCHEME_OPTION
ColorScheme UIThemeManager::colorScheme() const
{
    return m_colorSchemeSetting.get(ColorScheme::System);
}

void UIThemeManager::setColorScheme(const ColorScheme value)
{
    if (value == colorScheme())
        return;

    m_colorSchemeSetting = value;
}

void UIThemeManager::applyColorScheme() const
{
    switch (colorScheme())
    {
    case ColorScheme::System:
    default:
        qApp->styleHints()->unsetColorScheme();
        break;
    case ColorScheme::Light:
        qApp->styleHints()->setColorScheme(Qt::ColorScheme::Light);
        break;
    case ColorScheme::Dark:
        qApp->styleHints()->setColorScheme(Qt::ColorScheme::Dark);
        break;
    }
}
#endif

TrayIconStyle UIThemeManager::trayIconStyle() const
{
    return m_trayIconStyleSetting.get(TrayIconStyle::Normal);
}

void UIThemeManager::setTrayIconStyle(const TrayIconStyle value)
{
    if (value == trayIconStyle())
        return;

    m_trayIconStyleSetting = value;
}

void UIThemeManager::applyStyleSheet() const
{
    qApp->setStyleSheet(QString::fromUtf8(m_themeSource->readStyleSheet()));
}

void UIThemeManager::onColorSchemeChanged()
{
    emit themeChanged();

    // workaround to refresh styled controls once color scheme is changed
    QApplication::setStyle(QApplication::style()->name());
#ifdef Q_OS_MACOS
    if (!m_useCustomTheme)
        qApp->setStyleSheet(modernMacStyleSheet(isDarkTheme()));
#endif
}

QIcon UIThemeManager::getIcon(const QString &iconId, [[maybe_unused]] const QString &fallback, const ColorMode colorMode) const
{
    auto &icons = (colorMode == ColorMode::Dark) ? m_darkModeIcons : m_icons;

    const auto iter = icons.find(iconId);
    if (iter != icons.end())
        return *iter;

#if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS))
    // Don't cache system icons because users might change them at run time
    if (m_useSystemIcons)
    {
        auto icon = QIcon::fromTheme(iconId);
        if (icon.isNull() || icon.availableSizes().isEmpty())
            icon = QIcon::fromTheme(fallback, QIcon(m_themeSource->getIconPath(iconId, colorMode).data()));
        return icon;
    }
#endif

    const QIcon icon {m_themeSource->getIconPath(iconId, colorMode).data()};
    icons[iconId] = icon;
    return icon;
}

QIcon UIThemeManager::getIcon(const QString &iconId, const QString &fallback) const
{
    const auto colorMode = isDarkTheme() ? ColorMode::Dark : ColorMode::Light;
    return getIcon(iconId, fallback, colorMode);
}

QIcon UIThemeManager::getSystrayIcon() const
{
#ifdef Q_OS_WIN
    const auto colorMode = m_systemColorMode;
#else
    const auto colorMode = (qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark)
            ? ColorMode::Dark : ColorMode::Light;
#endif
    const QString fallback = (colorMode == ColorMode::Light)
            ? u"qbittorrent-tray-light"_s : u"qbittorrent-tray-dark"_s;

    switch (trayIconStyle())
    {
    case TrayIconStyle::Normal:
        return getIcon(u"qbittorrent-tray"_s, {}, colorMode);

    case TrayIconStyle::Monochrome:
        return getIcon(u"qbittorrent-tray-mono"_s, fallback, colorMode);
    }

    Q_UNREACHABLE_RETURN({});
}

QIcon UIThemeManager::getFlagIcon(const QString &countryIsoCode) const
{
    if (countryIsoCode.isEmpty())
        return {};

    const QString key = countryIsoCode.toLower();
    const auto iter = m_flags.constFind(key);
    if (iter != m_flags.cend())
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
    return color;
}

#ifdef Q_OS_WIN
void UIThemeManager::updateSystemColorMode()
{
    const HKEY hkRoot = HKEY_CURRENT_USER;
    const REGSAM samDesired = KEY_READ;

    HKEY hkPersonalize = nullptr;
    const LSTATUS status = ::RegOpenKeyExW(hkRoot, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, samDesired, &hkPersonalize);
    if (status != ERROR_SUCCESS)
        return;

    [[maybe_unused]] const auto hkPersonalizeGuard = qScopeGuard([&hkPersonalize] { ::RegCloseKey(hkPersonalize); });

    const ulong val = Utils::Reg::getULongValue(hkPersonalize, u"SystemUsesLightTheme"_s);
    const auto systemUsesLightTheme = static_cast<bool>(val);
    const auto systemColorMode = systemUsesLightTheme ? ColorMode::Light : ColorMode::Dark;
    if (systemColorMode != m_systemColorMode)
    {
        m_systemColorMode = systemColorMode;
        onColorSchemeChanged();
    }
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
        {u"Palette.Window"_s, QPalette::Window, QPalette::Normal},
        {u"Palette.WindowText"_s, QPalette::WindowText, QPalette::Normal},
        {u"Palette.Base"_s, QPalette::Base, QPalette::Normal},
        {u"Palette.AlternateBase"_s, QPalette::AlternateBase, QPalette::Normal},
        {u"Palette.Text"_s, QPalette::Text, QPalette::Normal},
        {u"Palette.ToolTipBase"_s, QPalette::ToolTipBase, QPalette::All},
        {u"Palette.ToolTipText"_s, QPalette::ToolTipText, QPalette::All},
        {u"Palette.BrightText"_s, QPalette::BrightText, QPalette::Normal},
        {u"Palette.Highlight"_s, QPalette::Highlight, QPalette::Normal},
        {u"Palette.HighlightedText"_s, QPalette::HighlightedText, QPalette::Normal},
        {u"Palette.Button"_s, QPalette::Button, QPalette::Normal},
        {u"Palette.ButtonText"_s, QPalette::ButtonText, QPalette::Normal},
        {u"Palette.Link"_s, QPalette::Link, QPalette::Normal},
        {u"Palette.LinkVisited"_s, QPalette::LinkVisited, QPalette::Normal},
        {u"Palette.Light"_s, QPalette::Light, QPalette::Normal},
        {u"Palette.Midlight"_s, QPalette::Midlight, QPalette::Normal},
        {u"Palette.Mid"_s, QPalette::Mid, QPalette::Normal},
        {u"Palette.Dark"_s, QPalette::Dark, QPalette::Normal},
        {u"Palette.Shadow"_s, QPalette::Shadow, QPalette::Normal},
        {u"Palette.WindowTextDisabled"_s, QPalette::WindowText, QPalette::Disabled},
        {u"Palette.TextDisabled"_s, QPalette::Text, QPalette::Disabled},
        {u"Palette.ToolTipTextDisabled"_s, QPalette::ToolTipText, QPalette::Disabled},
        {u"Palette.BrightTextDisabled"_s, QPalette::BrightText, QPalette::Disabled},
        {u"Palette.HighlightedTextDisabled"_s, QPalette::HighlightedText, QPalette::Disabled},
        {u"Palette.ButtonTextDisabled"_s, QPalette::ButtonText, QPalette::Disabled}
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
