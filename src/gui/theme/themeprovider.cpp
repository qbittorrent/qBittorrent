/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017  Eugene Shalygin <eugene.shalygin@gmail.com>
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

#include "themeprovider.h"

#include <algorithm>
#include <set>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QStandardPaths>

#include "base/logger.h"
#include "base/profile.h"
#include "base/settingsstorage.h"
#include "colorprovider_p.h"
#include "colortheme_impl.h"
#include "fonttheme_impl.h"
#include "themecommon.h"
#include "themeexceptions.h"
#include "themeinfo.h"

const QLatin1String Theme::ThemeProvider::colorThemeFileExtension(".colortheme");
const QLatin1String Theme::ThemeProvider::fontThemeFileExtension(".fonttheme");

namespace
{
    class ThemeProviderInstantiable : public Theme::ThemeProvider
    {
    public:
        using ThemeProvider::ThemeProvider;
    };

#define SETTINGS_KEY(name) "Appearance/" name
    const QLatin1String selectedColorThemeKey (SETTINGS_KEY("ColorTheme"));
    const QLatin1String selectedFontThemeKey (SETTINGS_KEY("FontTheme"));

    const QLatin1String defaultLightColorThemeName ("Default (Light)");
    const QLatin1String defaultDarkColorThemeName ("Default (Dark)");
    const QLatin1String defaultFontThemeName("Default");
}

Q_GLOBAL_STATIC(ThemeProviderInstantiable, themeProvider)

Theme::ThemeProvider &Theme::ThemeProvider::instance(){
    return *themeProvider();
}

Theme::ThemeProvider::ThemeProvider()
{
    connect(qGuiApp, &QGuiApplication::paletteChanged, this, &ThemeProvider::applicationPaletteChanged);
}

Theme::ThemeProvider::~ThemeProvider() = default;

void Theme::ThemeProvider::loadConfiguration()
{
    const QString colorThemeName = SettingsStorage::instance()->loadValue(selectedColorThemeKey, QString()).toString();
    try {
        setCurrentTheme(Kind::Color, colorThemeName);
    } catch (Serialization::DeserializationError &ex) {
        qCWarning(theme, "Color theme '%s' loading failed (%s), loading default theme instead",
                  qPrintable(colorThemeName), ex.what());
        Logger::instance()->addMessage(tr("Could not load color theme '%1'. Loading default theme.")
                                       .arg(colorThemeName));
        setCurrentTheme(Kind::Color, QString());
    }

    const QString fontThemeName = SettingsStorage::instance()->loadValue(selectedFontThemeKey, QString()).toString();
    try {
        setCurrentTheme(Kind::Font, fontThemeName);
    } catch (Serialization::DeserializationError &ex) {
        qCWarning(theme, "Font theme '%s' loading failed (%s), loading default theme instead",
                  qPrintable(fontThemeName), ex.what());
        Logger::instance()->addMessage(tr("Could not load font theme '%1'. Loading default theme.")
                                       .arg(fontThemeName));
        setCurrentTheme(Kind::Font, QString());
    }
}

std::map<Theme::ThemeInfo, QString> Theme::ThemeProvider::availableThemes(Kind kind) const
{
    QDir themeDir;
    switch (kind) {
    case Kind::Color:
        themeDir.setNameFilters({QStringLiteral("*") + colorThemeFileExtension});
        break;
    case Kind::Font:
        themeDir.setNameFilters({QStringLiteral("*") + fontThemeFileExtension});
        break;
    }
    QStringList themeSearchPaths = this->themeSearchPaths();
    std::map<Theme::ThemeInfo, QString> res;
    std::set<QString> loadedThemeNames;
    for (const QString &path : themeSearchPaths) {
        themeDir.cd(path);
        QStringList themeFiles = themeDir.entryList();
        for (const QString &themeFile : themeFiles) {
            const QString absThemePath = themeDir.absoluteFilePath(themeFile);
            QSettings theme(absThemePath, QSettings::IniFormat);
            ThemeInfo info(theme);
            if (!loadedThemeNames.count(info.name())) {
                res[info] = absThemePath;
                loadedThemeNames.insert(info.name());
            }
        }
    }

    return res;
}

QString Theme::ThemeProvider::defaultThemeName(Theme::Kind kind) const
{
    switch (kind) {
    case Kind::Color:
        return isDarkTheme() ? defaultDarkColorThemeName : defaultLightColorThemeName;
    case Kind::Font:
        return defaultFontThemeName;
    default:
        throw std::logic_error("Unexpected theme kind");
    }
}

QString Theme::ThemeProvider::locateTheme(Kind kind, const QString &themeName) const
{
    const auto themes = availableThemes(kind);
    ThemeInfo fake;
    fake.setName(themeName);
    auto i = themes.find(fake);
    if (i != themes.end())
        return i->second;

    throw ThemeNotFound(themeName);
}

void Theme::ThemeProvider::exportTheme(Theme::Kind kind, const QString &themeName,
                                       const QString &fileName, ExportOptios options)
{
    switch (kind) {
    case Kind::Color:
        SerializableColorTheme(themeName).save(fileName, options.testFlag(ExportOption::WriteExplicitValues));
        break;
    case Kind::Font:
        SerializableFontTheme(themeName).save(fileName, options.testFlag(ExportOption::WriteExplicitValues));
        break;
    }
}

QStringList Theme::ThemeProvider::themeSearchPaths() const
{
    const QString themeDirName = QStringLiteral("theme");
    QStringList res;
    // always support resources, they are first in the list for two reasons:
    // 1) themes from this dir serve as fallback
    // 2) user may not override them because of 1)
    res << QLatin1String(":/") + themeDirName;
    res << QDir(Profile::instance().location(SpecialFolder::Data)).absoluteFilePath(themeDirName);
    // WARNING QStandardPaths::locateAll() returns a list with entries from home directory at the
    // beginning, but this is not documented and might change. In that case this method will be incorrect.
    res << QStandardPaths::locateAll(QStandardPaths::AppLocalDataLocation, themeDirName, QStandardPaths::LocateDirectory);
#if defined Q_OS_UNIX && !defined Q_OS_MACOS
    QDir appDir = QCoreApplication::applicationDirPath();
    appDir.cdUp();
    const QString installPrefix = appDir.canonicalPath();
    const QString resourceDirInInstallPrefix = installPrefix + QLatin1String("/share/")
                                               + QCoreApplication::applicationName() + QDir::separator() + themeDirName;
    if (!res.contains(resourceDirInInstallPrefix))
        res << resourceDirInInstallPrefix;
#endif
    return res;
}

const Theme::ColorTheme &Theme::ThemeProvider::colorTheme() const
{
    return *m_currentColorTheme;
}

const Theme::FontTheme &Theme::ThemeProvider::fontTheme() const
{
    return *m_currentFontTheme;
}

void Theme::ThemeProvider::setCurrentTheme(Kind kind, const QString &themeName)
{
    switch (kind) {
    case Kind::Color:
        qCInfo(theme, "Loading color theme %s", qPrintable(themeName));
        try {
            decltype(m_currentColorTheme)newTheme(new SerializableColorTheme(themeName));
            std::swap(m_currentColorTheme, newTheme);
            emit colorThemeChanged();
            SettingsStorage::instance()->storeValue(selectedColorThemeKey, themeName);
        }
        catch (Serialization::DeserializationError &er) {
            qCInfo(theme) << "Could not load color theme '" << themeName << "': " << er.what();
            throw;
        }
        break;
    case Kind::Font:
        qCInfo(theme, "Loading font theme %s", qPrintable(themeName));
        try {
            decltype(m_currentFontTheme)newTheme(new SerializableFontTheme(themeName));
            std::swap(m_currentFontTheme, newTheme);
            emit fontThemeChanged();
            SettingsStorage::instance()->storeValue(selectedFontThemeKey, themeName);
        }
        catch (Serialization::DeserializationError &er) {
            qCInfo(theme) << "Could not load font theme '" << themeName << "': " << er.what();
            throw;
        }
        break;
    }
}

Theme::ThemeInfo Theme::ThemeProvider::currentTheme(Theme::Kind kind) const
{
    switch (kind) {
    case Kind::Color:
        return colorTheme().info();
    case Kind::Font:
        return fontTheme().info();
    }
    throw std::logic_error("Unexpected theme kind");
}

void Theme::ThemeProvider::applicationPaletteChanged(const QPalette &)
{
    Serialization::ColorsProviderRegistry::instance().applicationPaletteChanged();
    m_currentColorTheme->applicationPaletteChanged();
    emit colorThemeChanged();
}

void Theme::ThemeProvider::initInstance()
{
    instance().loadConfiguration();
}
