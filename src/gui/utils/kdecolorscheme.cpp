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

#include "kdecolorscheme.h"
#include "colorutils.h"
#include <QDir>
#include <QApplication>
#include <QSettings>

namespace {
    static QString settingsGroupName(KDEColorScheme::ColorSet set)
    {
        switch(set) {
        case KDEColorScheme::View:
            return QLatin1String("Colors:View");
        case KDEColorScheme::Window:
            return QLatin1String("Colors:Window");
        case KDEColorScheme::Button:
            return QLatin1String("Colors:Button");
        case KDEColorScheme::Selection:
            return QLatin1String("Colors:Selection");
        case KDEColorScheme::Tooltip:
            return QLatin1String("Colors:Tooltip");
        }
    }

    QRgb decodeKDEColor(const QVariant &v, bool &ok)
    {
        QVariantList components(v.toList());
        if (components.size() == 3) {
            bool rOk, gOk, bOk;
            const int r = components[0].toString().toInt(&rOk);
            const int g = components[1].toString().toInt(&gOk);
            const int b = components[2].toString().toInt(&bOk);
            if (rOk && gOk && bOk) {
                ok = true;
                return qRgb(r, g, b);
            }
        }
        ok = false;
        return qRgb(0, 0, 0);
    }

    KDEColorScheme::Effect readColorEffect(QSettings &settings, const QString &name, bool &ok)
    {
        using Effect = KDEColorScheme::Effect;
        Effect res;
        bool allReadsOK = true;
        bool singleReadOK = false;
        settings.beginGroup(QLatin1String("ColorEffects:") + name);

        res.changeSelectionColor = settings.value(QLatin1String("ChangeSelectionColor"), false).toBool();

        res.color = decodeKDEColor(settings.value(QLatin1String("Color")), singleReadOK);
        allReadsOK &= singleReadOK;

        res.colorAmount = settings.value(QLatin1String("ColorAmount"), 0.).toDouble(&ok);
        allReadsOK &= singleReadOK;

        res.colorEffect = static_cast<Effect::ColorEfffect>(
            settings.value(QLatin1String("ColorEffect"), 0).toInt(&singleReadOK));
        allReadsOK &= singleReadOK;

        res.contrastAmount = settings.value(QLatin1String("ContrastAmount"), 0.).toDouble(&singleReadOK);
        allReadsOK &= singleReadOK;

        res.contrastEffect = static_cast<Effect::ContrastEffect>(
            settings.value(QLatin1String("ContrastEffect"), 0).toInt(&singleReadOK));
        allReadsOK &= singleReadOK;

        res.enable = settings.value(QLatin1String("Enable"), false).toBool();

        res.intensityAmount = settings.value(QLatin1String("IntensityAmount"), 0.).toDouble(&singleReadOK);
        allReadsOK &= singleReadOK;

        res.intensityEffect = static_cast<Effect::IntensityEffect>(
            settings.value(QLatin1String("IntensityEffect"), 0).toInt(&singleReadOK));
        allReadsOK &= singleReadOK;

        settings.endGroup();
        ok = allReadsOK;
        return res;
    }

    QColor applyColorEffect(const QColor &color, const KDEColorScheme::Effect &effect)
    {
        using Effect = KDEColorScheme::Effect;
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
        case Effect::ColorEfffectNone:
            break;
        case Effect::ColorEfffectDesaturate:
            res = Utils::Color::darken(res, 0., 1.0 - effect.colorAmount);
            break;
        case Effect::ColorEfffectFade:
            res = Utils::Color::mix(res, effect.color, effect.colorAmount);
            break;
        case Effect::ColorEfffectTint:
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

Q_GLOBAL_STATIC(KDEColorScheme, kdeColorScheme)

const KDEColorScheme * KDEColorScheme::instance(){
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    return kdeColorScheme;
#else
    return kdeColorScheme();
#endif
}

KDEColorScheme::KDEColorScheme(const QProcessEnvironment& env)
{
    reload(env);
}

void KDEColorScheme::reload(const QProcessEnvironment& env)
{
    try {
        load(env);
        m_loadedSuccesfully = true;
    }
    catch (ColorLoadingError&) {
        m_loadedSuccesfully = false;
        setFallbackColors();
    }
}

QColor KDEColorScheme::color(KDEColorScheme::ColorRole role, QPalette::ColorGroup group, KDEColorScheme::ColorSet set) const
{
    if (group > QPalette::Inactive)
        return Qt::black;

    return m_colors[group][set][role];
}

QColor KDEColorScheme::inactiveColor(const QColor& color) const
{
    return applyColorEffect(color, m_inactiveEffect);
}

QColor KDEColorScheme::disabledColor(const QColor& color) const
{
    return applyColorEffect(color, m_disabledEffect);
}

bool KDEColorScheme::wasLoadedSuccesfuly() const
{
    return m_loadedSuccesfully;
}

void KDEColorScheme::load(const QProcessEnvironment& env)
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
    activeColors[View] = readSet(kdeGlobals, View);
    activeColors[Window] = readSet(kdeGlobals, Window);
    activeColors[Button] = readSet(kdeGlobals, Button);
    activeColors[Selection] = readSet(kdeGlobals, Selection);
    activeColors[Tooltip] = readSet(kdeGlobals, Tooltip);

    m_colors[QPalette::Active] = activeColors;

    bool inactiveEffectReadOK, disabledEffectReadOK;
    m_inactiveEffect = readColorEffect(kdeGlobals, QLatin1String("Inactive"), inactiveEffectReadOK);
    m_disabledEffect = readColorEffect(kdeGlobals, QLatin1String("Disabled"), disabledEffectReadOK);
    if(!inactiveEffectReadOK || !disabledEffectReadOK)
        throw ColorLoadingError();

    m_colors[QPalette::Inactive] = applyEffect(activeColors, m_inactiveEffect);
    m_colors[QPalette::Disabled] = applyEffect(activeColors, m_disabledEffect);
}


KDEColorScheme::ColorsMap KDEColorScheme::readSet(QSettings& settings, KDEColorScheme::ColorSet set)
{
    ColorsMap colors;

    settings.beginGroup(settingsGroupName(set));
    bool allColorsWereReadOk = true;
    bool colorReadOk = false;
    colors[BackgroundAlternate] = decodeKDEColor(settings.value(QLatin1String("BackgroundAlternate")), colorReadOk);
    allColorsWereReadOk &= colorReadOk;
    colors[BackgroundNormal] = decodeKDEColor(settings.value(QLatin1String("BackgroundNormal")), colorReadOk);
    allColorsWereReadOk &= colorReadOk;
    colors[DecorationFocus] = decodeKDEColor(settings.value(QLatin1String("DecorationFocus")), colorReadOk);
    allColorsWereReadOk &= colorReadOk;
    colors[ForegroundVisited] = decodeKDEColor(settings.value(QLatin1String("ForegroundVisited")), colorReadOk);
    allColorsWereReadOk &= colorReadOk;
    colors[ForegroundNormal] = decodeKDEColor(settings.value(QLatin1String("ForegroundNormal")), colorReadOk);
    allColorsWereReadOk &= colorReadOk;
    colors[DecorationHover] = decodeKDEColor(settings.value(QLatin1String("DecorationHover")), colorReadOk);
    allColorsWereReadOk &= colorReadOk;
    colors[ForegroundActive] = decodeKDEColor(settings.value(QLatin1String("ForegroundActive")), colorReadOk);
    allColorsWereReadOk &= colorReadOk;
    colors[ForegroundInactive] = decodeKDEColor(settings.value(QLatin1String("ForegroundInactive")), colorReadOk);
    allColorsWereReadOk &= colorReadOk;
    colors[ForegroundLink] = decodeKDEColor(settings.value(QLatin1String("ForegroundLink")), colorReadOk);
    allColorsWereReadOk &= colorReadOk;
    colors[ForegroundNegative] = decodeKDEColor(settings.value(QLatin1String("ForegroundNegative")), colorReadOk);
    allColorsWereReadOk &= colorReadOk;
    colors[ForegroundNeutral] = decodeKDEColor(settings.value(QLatin1String("ForegroundNeutral")), colorReadOk);
    allColorsWereReadOk &= colorReadOk;
    colors[ForegroundPositive] = decodeKDEColor(settings.value(QLatin1String("ForegroundPositive")), colorReadOk);
    allColorsWereReadOk &= colorReadOk;
    settings.endGroup();

    return colors;
}

void KDEColorScheme::setFallbackColors()
{
    m_colors[QPalette::Active] = fallbackColorSet(QPalette::Active);
    m_colors[QPalette::Disabled] = fallbackColorSet(QPalette::Disabled);
    m_colors[QPalette::Inactive] = fallbackColorSet(QPalette::Inactive);
}

KDEColorScheme::ColorsSet KDEColorScheme::fallbackColorSet(QPalette::ColorGroup group)
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

KDEColorScheme::ColorsSet KDEColorScheme::applyEffect(const KDEColorScheme::ColorsSet& set, const KDEColorScheme::Effect& effect)
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
