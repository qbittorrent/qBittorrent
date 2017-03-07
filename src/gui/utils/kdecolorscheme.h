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

#ifndef KDECOLORSCHEME_H
#define KDECOLORSCHEME_H

#include <QColor>
#include <QPalette>
#include <QMap>
#include <QProcessEnvironment>

class QSettings;

class KDEColorScheme
{
public:
    struct Effect
    {
        enum ColorEfffect
        {
            ColorEfffectNone = 0,
            ColorEfffectDesaturate,
            ColorEfffectFade,
            ColorEfffectTint
        };
        enum IntensityEffect
        {
            IntensityEffectNone = 0,
            IntensityEffectShade,
            IntensityEffectDarken,
            IntensityEffectLighten
        };
        enum ContrastEffect
        {
            ContrastEffectNone = 0,
            ContrastEffectFade,
            ContrastEffectTint
        };
        bool changeSelectionColor;
        QColor color;
        qreal colorAmount; // [-1:1]
        ColorEfffect colorEffect;
        qreal contrastAmount; // [-1:1]
        ContrastEffect contrastEffect;
        bool enable;
        qreal intensityAmount; // [-1:1]
        IntensityEffect intensityEffect;
    };

    enum ColorRole
    {
        BackgroundAlternate,
        BackgroundNormal,
        DecorationFocus,
        DecorationHover,
        ForegroundActive,
        ForegroundInactive,
        ForegroundLink,
        ForegroundNegative,
        ForegroundNeutral,
        ForegroundNormal,
        ForegroundPositive,
        ForegroundVisited
    };

    enum ColorSet
    {
        /**
         * Views; for example, frames, input fields, etc.
         *
         * If it contains things that can be selected, it is probably a View.
         */
        View,
        /**
         * Non-editable window elements; for example, menus.
         *
         * If it isn't a Button, View, or Tooltip, it is probably a Window.
         */
        Window,
        /**
         * Buttons and button-like controls.
         *
         * In addition to buttons, "button-like" controls such as non-editable
         * dropdowns, scrollbar sliders, slider handles, etc. should also use
         * this role.
         */
        Button,
        /**
         * Selected items in views.
         *
         * Note that unfocused or disabled selections should use the Window
         * role. This makes it more obvious to the user that the view
         * containing the selection does not have input focus.
         */
        Selection,
        /**
         * Tooltips.
         *
         * The tooltip set can often be substituted for the view
         * set when editing is not possible, but the Window set is deemed
         * inappropriate. "What's This" help is an excellent example, another
         * might be pop-up notifications (depending on taste).
         */
        Tooltip
    };

    KDEColorScheme(const QProcessEnvironment& env = QProcessEnvironment::systemEnvironment());
    void reload(const QProcessEnvironment& env = QProcessEnvironment::systemEnvironment());

    static const KDEColorScheme* instance();

    QColor color(ColorRole role, QPalette::ColorGroup group = QPalette::Active, ColorSet set = View) const;
    QColor disabledColor(const QColor& color) const;
    QColor inactiveColor(const QColor& color) const;

    bool wasLoadedSuccesfuly() const;

private:
    using ColorsMap = QMap<ColorRole, QColor>;
    using ColorsSet = QMap<ColorSet, ColorsMap>;
    using Colors = QMap<QPalette::ColorGroup, ColorsSet>;

    void load(const QProcessEnvironment& env = QProcessEnvironment::systemEnvironment());
    void setFallbackColors();
    static ColorsSet fallbackColorSet(QPalette::ColorGroup group);

    static ColorsMap readSet(QSettings& settings, ColorSet set);
    static ColorsSet applyEffect(const ColorsSet &set, const Effect& effect);

    Colors m_colors;
    Effect m_disabledEffect;
    Effect m_inactiveEffect;
    bool m_loadedSuccesfully;
};

#endif
