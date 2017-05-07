/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016 Eugene Shalygin
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

#include "plasmacolorscheme.h"

#include <stdexcept>

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>

#include "colorutils.h"

namespace
{
    class KDEColorThemeDecodingError: public std::runtime_error
    {
    public:
        KDEColorThemeDecodingError(const QString& entryName);
        const QString &entryName() const;

    private:
        QString m_entryName;
    };

    KDEColorThemeDecodingError::KDEColorThemeDecodingError(const QString& entryName)
        : std::runtime_error("Could not parse color '" + entryName.toStdString() + '\'')
        , m_entryName {entryName}
    {
    }

    const QString &KDEColorThemeDecodingError::entryName() const
    {
        return m_entryName;
    }

    static QString settingsGroupName(PlasmaColorScheme::ColorSet set)
    {
        switch (set) {
        case PlasmaColorScheme::View:
            return QLatin1String("Colors:View");
        case PlasmaColorScheme::Window:
            return QLatin1String("Colors:Window");
        case PlasmaColorScheme::Button:
            return QLatin1String("Colors:Button");
        case PlasmaColorScheme::Selection:
            return QLatin1String("Colors:Selection");
        case PlasmaColorScheme::Tooltip:
            return QLatin1String("Colors:Tooltip");
        default:
            throw std::logic_error("Unexpected value of PlasmaColorScheme::ColorSet");
        }
    }

    QRgb decodeKDEColor(QSettings &settings, const QString &colorName)
    {
        QVariantList components(settings.value(colorName).toList());
        if (components.size() == 3) {
            bool rOk, gOk, bOk;
            const int r = components[0].toString().toInt(&rOk);
            const int g = components[1].toString().toInt(&gOk);
            const int b = components[2].toString().toInt(&bOk);
            if (rOk && gOk && bOk) {
                return qRgb(r, g, b);
            }
        }
        throw KDEColorThemeDecodingError(colorName);
    }

    class SettingsGroupGuard
    {
    public:
        SettingsGroupGuard(QSettings &settings, const QString &groupName);
        ~SettingsGroupGuard();

    private:
        QSettings &m_settings;
    };

    SettingsGroupGuard::SettingsGroupGuard(QSettings& settings, const QString& groupName)
        : m_settings(settings)
    {
        m_settings.beginGroup(groupName);
    }

    SettingsGroupGuard::~SettingsGroupGuard()
    {
        m_settings.endGroup();
    }

    template <typename T>
    T readValue(QSettings &settings, const QString &key, const T &defaultValue = T())
    {
        QVariant v = settings.value(key, defaultValue);
        if (v.canConvert<T>()) {
            return v.value<T>();
        }
        throw KDEColorThemeDecodingError(key);
    }

    PlasmaColorScheme::Effect readColorEffect(QSettings &settings, const QString &name)
    {
        using Effect = PlasmaColorScheme::Effect;
        Effect res;

        SettingsGroupGuard group(settings, QLatin1String("ColorEffects:") + name);

        res.changeSelectionColor = settings.value(QLatin1String("ChangeSelectionColor"), false).toBool();
        res.color = decodeKDEColor(settings, QLatin1String("Color"));
        res.colorAmount = readValue<double>(settings, QLatin1String("ColorAmount"));
        res.colorEffect = static_cast<Effect::ColorEffect>(readValue<int>(settings, QLatin1String("ColorEffect")));
        res.contrastAmount = readValue<double>(settings, QLatin1String("ContrastAmount"));
        res.contrastEffect = static_cast<Effect::ContrastEffect>(readValue<int>(settings, QLatin1String("ContrastEffect")));
        res.enable = readValue<bool>(settings, QLatin1String("Enable"), false);
        res.intensityAmount = readValue<double>(settings, QLatin1String("IntensityAmount"));
        res.intensityEffect = static_cast<Effect::IntensityEffect>(readValue<int>(settings, QLatin1String("IntensityEffect")));
        return res;
    }

    QColor applyColorEffect(const QColor &color, const PlasmaColorScheme::Effect &effect)
    {
        using Effect = PlasmaColorScheme::Effect;
        QColor res(color);

        switch (effect.intensityEffect) {
        case Effect::IntensityEffectNone:
            break;
        case Effect::IntensityEffectShade:
            res = Utils::Color::shade(res, effect.intensityAmount);
            break;
        case Effect::IntensityEffectDarken:
            res = Utils::Color::darken(res, effect.intensityAmount);
            break;
        case Effect::IntensityEffectLighten:
            res = Utils::Color::lighten(res, effect.intensityAmount);
            break;
        }

        switch (effect.colorEffect) {
        case Effect::ColorEffectNone:
            break;
        case Effect::ColorEffectDesaturate:
            res = Utils::Color::darken(res, 0., 1.0 - effect.colorAmount);
            break;
        case Effect::ColorEffectFade:
            res = Utils::Color::mix(res, effect.color, effect.colorAmount);
            break;
        case Effect::ColorEffectTint:
            res = Utils::Color::tint(res, effect.color, effect.colorAmount);
            break;
        }

#if 0
        // shall we worry about contrast with background at all?
        switch (effect.contrastEffect) {
        case KDEColorEffect::ContrastEffectNone:
            break;
        case KDEColorEffect::ContrastEffectFade:
            res = Utils::Color::mix(res, bgColor, effect.contrastAmount);
            break;
        case KDEColorEffect::ContrastEffectTint:
            res = Utils::Color::tint(res, bgColor, effect.contrastAmount);
            break;
        }
#endif
        return res;
    }

    class ColorLoadingError: public std::runtime_error
    {
    public:
        ColorLoadingError() : std::runtime_error(""){}
    };
}

Q_GLOBAL_STATIC(PlasmaColorScheme, kdeColorScheme)

const PlasmaColorScheme *PlasmaColorScheme::instance()
{
    return kdeColorScheme;
}

PlasmaColorScheme::PlasmaColorScheme(const QProcessEnvironment& env)
{
    reload(env);
}

void PlasmaColorScheme::reload(const QProcessEnvironment& env)
{
    try {
        load(env);
        m_loadedSuccesfully = true;
    }
    catch (ColorLoadingError&) {
        qDebug() << "Could not load Plasma color theme. Falling back to QPalette";
        m_loadedSuccesfully = false;
        setFallbackColors();
    }
}

QColor PlasmaColorScheme::color(PlasmaColorScheme::ColorRole role, QPalette::ColorGroup group, PlasmaColorScheme::ColorSet set) const
{
    if (group > QPalette::Inactive)
        return Qt::black;

    return m_colors[group][set][role];
}

QColor PlasmaColorScheme::inactiveColor(const QColor& color) const
{
    return applyColorEffect(color, m_inactiveEffect);
}

QColor PlasmaColorScheme::disabledColor(const QColor& color) const
{
    return applyColorEffect(color, m_disabledEffect);
}

bool PlasmaColorScheme::wasLoadedSuccesfully() const
{
    return m_loadedSuccesfully;
}

void PlasmaColorScheme::load(const QProcessEnvironment& env)
{
    bool versionDecodedOk = false;
    int kdeVersion = env.value(QLatin1String("KDE_SESSION_VERSION"), QLatin1String("4"))
                     .toInt(&versionDecodedOk);
    if (!versionDecodedOk)
        throw ColorLoadingError();
    QString kdeCfgFile;
    switch (kdeVersion) {
    case 4:
        kdeCfgFile = QLatin1String(".kde4/share/config/kdeglobals");
        break;
    case 5:
        kdeCfgFile = QLatin1String(".config/kdeglobals");
        break;
    default:
        throw ColorLoadingError();
    }

    QString configPath = QDir(QDir::homePath()).absoluteFilePath(kdeCfgFile);
    if (!QFileInfo(configPath).exists())
        throw ColorLoadingError();

    QSettings kdeGlobals(configPath, QSettings::IniFormat);
    ColorsSet activeColors;
    try {
        activeColors[View] = readSet(kdeGlobals, View);
        activeColors[Window] = readSet(kdeGlobals, Window);
        activeColors[Button] = readSet(kdeGlobals, Button);
        activeColors[Selection] = readSet(kdeGlobals, Selection);
        activeColors[Tooltip] = readSet(kdeGlobals, Tooltip);
    }
    catch (KDEColorThemeDecodingError& ex) {
        qDebug("Could not parse KDE/Plasma color scheme: %s", ex.what());
        throw ColorLoadingError();
    }

    m_colors[QPalette::Active] = activeColors;

    try {
        m_inactiveEffect = readColorEffect(kdeGlobals, QLatin1String("Inactive"));
        m_disabledEffect = readColorEffect(kdeGlobals, QLatin1String("Disabled"));
    }
    catch (KDEColorThemeDecodingError&) {
        // perhaps we have recent enough Plasma and effects were not copied to .kdeglobals,
        // we have to load them from theme file
        const QString themeName = kdeGlobals.value(QLatin1String("ColorScheme"), QString()).toString();
        if (themeName.isEmpty()) {
            throw ColorLoadingError(); // we do not know theme name
        }
        const QString themeFileName = locateColorThemeFile(themeName);
        if (themeFileName.isEmpty()) {
            qDebug("Could not find file for Plasma color theme '%s'", qPrintable(themeName));
            throw ColorLoadingError();
        }
        QSettings theme(themeFileName, QSettings::IniFormat);
        try {
            m_inactiveEffect = readColorEffect(theme, QLatin1String("Inactive"));
            m_disabledEffect = readColorEffect(theme, QLatin1String("Disabled"));
        }
        catch (KDEColorThemeDecodingError& ex) {
            // no luck, we give up
            qDebug("Could not load effects from theme file '%s': %s", qPrintable(themeFileName), ex.what());
            throw ColorLoadingError();
        }
    }

    m_colors[QPalette::Inactive] = applyEffect(activeColors, m_inactiveEffect);
    m_colors[QPalette::Disabled] = applyEffect(activeColors, m_disabledEffect);
}


PlasmaColorScheme::ColorsMap PlasmaColorScheme::readSet(QSettings& settings, PlasmaColorScheme::ColorSet set)
{
    ColorsMap colors;

    try {
        SettingsGroupGuard group(settings, settingsGroupName(set));
        colors[BackgroundAlternate] = decodeKDEColor(settings, QLatin1String("BackgroundAlternate"));
        colors[BackgroundNormal] = decodeKDEColor(settings, QLatin1String("BackgroundNormal"));
        colors[DecorationFocus] = decodeKDEColor(settings, QLatin1String("DecorationFocus"));
        colors[ForegroundVisited] = decodeKDEColor(settings, QLatin1String("ForegroundVisited"));
        colors[ForegroundNormal] = decodeKDEColor(settings, QLatin1String("ForegroundNormal"));
        colors[DecorationHover] = decodeKDEColor(settings, QLatin1String("DecorationHover"));
        colors[ForegroundActive] = decodeKDEColor(settings, QLatin1String("ForegroundActive"));
        colors[ForegroundInactive] = decodeKDEColor(settings, QLatin1String("ForegroundInactive"));
        colors[ForegroundLink] = decodeKDEColor(settings, QLatin1String("ForegroundLink"));
        colors[ForegroundNegative] = decodeKDEColor(settings, QLatin1String("ForegroundNegative"));
        colors[ForegroundNeutral] = decodeKDEColor(settings, QLatin1String("ForegroundNeutral"));
        colors[ForegroundPositive] = decodeKDEColor(settings, QLatin1String("ForegroundPositive"));

        return colors;
    }
    catch (KDEColorThemeDecodingError &ex) {
        throw KDEColorThemeDecodingError(settingsGroupName(set) + QLatin1Char('/') + ex.entryName());
    }
}

void PlasmaColorScheme::setFallbackColors()
{
    m_colors[QPalette::Active] = fallbackColorSet(QPalette::Active);
    m_colors[QPalette::Disabled] = fallbackColorSet(QPalette::Disabled);
    m_colors[QPalette::Inactive] = fallbackColorSet(QPalette::Inactive);
}

PlasmaColorScheme::ColorsSet PlasmaColorScheme::fallbackColorSet(QPalette::ColorGroup group)
{
    QPalette palette = QApplication::palette();
    ColorsMap view;
    view[BackgroundAlternate] = palette.color(group, QPalette::AlternateBase);
    view[BackgroundNormal] = palette.color(group, QPalette::Background);
    view[DecorationFocus] = palette.color(group, QPalette::HighlightedText);
    view[DecorationHover] = palette.color(group, QPalette::Highlight);
    view[ForegroundActive] = palette.color(group, QPalette::Text);
    view[ForegroundInactive] = palette.color(group, QPalette::Text);
    view[ForegroundLink] = palette.color(group, QPalette::Link);
    view[ForegroundNegative] = palette.color(group, QPalette::Text);
    view[ForegroundNeutral] = palette.color(group, QPalette::Text);
    view[ForegroundNormal] = palette.color(group, QPalette::Text);
    view[ForegroundPositive] = palette.color(group, QPalette::Text);
    view[ForegroundVisited] = palette.color(group, QPalette::LinkVisited);

    ColorsMap window = {view};
    window[BackgroundNormal] = window[BackgroundAlternate] = palette.color(group, QPalette::Window);

    ColorsMap button = {view};
    button[BackgroundNormal] = button[BackgroundAlternate] = palette.color(group, QPalette::Button);
    button[ForegroundActive] = palette.color(group, QPalette::ButtonText);
    button[ForegroundInactive] = palette.color(group, QPalette::ButtonText);

    ColorsMap tooltip = {view};
    tooltip[BackgroundNormal] = tooltip[BackgroundAlternate] = palette.color(group, QPalette::ToolTipBase);
    tooltip[ForegroundActive] = tooltip[ForegroundInactive] = palette.color(group, QPalette::ToolTipText);
    tooltip[ForegroundNegative] = tooltip[ForegroundNeutral] = palette.color(group, QPalette::ToolTipText);
    tooltip[ForegroundNormal] = tooltip[ForegroundPositive] = palette.color(group, QPalette::ToolTipText);

    ColorsSet res;
    res.insert(View, view);
    res.insert(Window, window);
    res.insert(Button, button);
    res.insert(Tooltip, tooltip);
    res.insert(Selection, view);

    return res;
}

PlasmaColorScheme::ColorsSet PlasmaColorScheme::applyEffect(const PlasmaColorScheme::ColorsSet& set, const PlasmaColorScheme::Effect& effect)
{
    ColorsSet res;
    for (auto i = set.begin(); i != set.end(); ++i) {
        ColorsMap resMap;
        const ColorsMap &srcMap = i.value();
        for (auto j = srcMap.begin(); j != srcMap.end(); ++j)
            resMap[j.key()] = applyColorEffect(j.value(), effect);
        res.insert(i.key(), resMap);
    }

    return res;
}

QString PlasmaColorScheme::locateColorThemeFile(const QString &themeName)
{
    // Plasma themes are located in share/color-schemes
    // Usually file name is equal to ${themeName}.colors with
    // first character uppercased. However, it is not guaranteed
    // and we have to check Name key in the files

    QLatin1String colorShemesDirName("color-schemes");
    // let's go
    // 1. uppercase first letter of the theme name and try to find such file
    QString probableThemeFileName = themeName;
    probableThemeFileName[0] = probableThemeFileName[0].toUpper();
    const QLatin1String themeFileExtension(".colors");

    const QStringList probableThemes = QStandardPaths::locateAll(
        QStandardPaths::GenericDataLocation, colorShemesDirName + QLatin1Char('/') + probableThemeFileName + themeFileExtension);

    // 2. this is our test function to check "Name=" key of the file
    const auto checkThemeName = [&](const QString &file)
    {
        QSettings theme(file, QSettings::IniFormat);
        return theme.value(QLatin1String("Name"), QString()).toString() == themeName;
    };

    for (const QString &f: probableThemes) {
        if (checkThemeName(f)) {
            return f;
        }
    }

    // 3. we have no other choice but to try all the files
    const QStringList dirs = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    for (const QString &dir: dirs) {
        QDir colorShemesDir(dir + QLatin1Char('/') + colorShemesDirName);
        if (colorShemesDir.exists()) {
            const QStringList files = colorShemesDir.entryList({QLatin1Char('*') + themeFileExtension}, QDir::Files);
            for (const QString &f: files) {
                if (checkThemeName(colorShemesDir.absoluteFilePath(f))) {
                    return colorShemesDir.absoluteFilePath(f);
                }
            }
        }
    }
    // we failed to find theme file
    return {};
}
