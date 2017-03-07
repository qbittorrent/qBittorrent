/* This is the stripped version of the file kcolorutils.h
 * from KDE framework KGuiAddons.
 *
 * This file is part of the KDE project
 * Copyright (C) 2007 Matthew Woehlke <mw_triad@users.sourceforge.net>
 * Copyright (C) 2007 Thomas Zander <zander@kde.org>
 * Copyright (C) 2007 Zack Rusin <zack@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef COLORUTILS_H
#define COLORUTILS_H

#include <QPainter>

class QColor;

/**
 * A set of methods used to work with colors.
 */
namespace Utils
{
    namespace Color
    {
        /**
         * Calculate the luma of a color. Luma is weighted sum of gamma-adjusted
         * R'G'B' components of a color. The result is similar to qGray. The range
         * is from 0.0 (black) to 1.0 (white).
         *
         * Utils::Color::darken(), Utils::Color::lighten() and Utils::Color::shade()
         * operate on the luma of a color.
         *
         * @see http://en.wikipedia.org/wiki/Luma_(video)
         */
        qreal luma(const QColor &);

        /**
         * Calculate hue, chroma and luma of a color in one call.
         */
        void getHcy(const QColor &, qreal *hue, qreal *chroma, qreal *luma, qreal *alpha = 0);

        /**
         * Calculate the contrast ratio between two colors, according to the
         * W3C/WCAG2.0 algorithm, (Lmax + 0.05)/(Lmin + 0.05), where Lmax and Lmin
         * are the luma values of the lighter color and the darker color,
         * respectively.
         *
         * A contrast ration of 5:1 (result == 5.0) is the minimum for "normal"
         * text to be considered readable (large text can go as low as 3:1). The
         * ratio ranges from 1:1 (result == 1.0) to 21:1 (result == 21.0).
         *
         * @see Utils::Color::luma
         */
        qreal contrastRatio(const QColor &, const QColor &);

        /**
         * Adjust the luma of a color by changing its distance from white.
         *
         * @li amount == 1.0 gives white
         * @li amount == 0.5 results in a color whose luma is halfway between 1.0
         * and that of the original color
         * @li amount == 0.0 gives the original color
         * @li amount == -1.0 gives a color that is 'twice as far from white' as
         * the original color, that is luma(result) == 1.0 - 2*(1.0 - luma(color))
         *
         * @param amount factor by which to adjust the luma component of the color
         * @param chromaInverseGain (optional) factor by which to adjust the chroma
         * component of the color; 1.0 means no change, 0.0 maximizes chroma
         * @see Utils::Color::shade
         */
        QColor lighten(const QColor &, qreal amount = 0.5, qreal chromaInverseGain = 1.0);

        /**
         * Adjust the luma of a color by changing its distance from black.
         *
         * @li amount == 1.0 gives black
         * @li amount == 0.5 results in a color whose luma is halfway between 0.0
         * and that of the original color
         * @li amount == 0.0 gives the original color
         * @li amount == -1.0 gives a color that is 'twice as far from black' as
         * the original color, that is luma(result) == 2*luma(color)
         *
         * @param amount factor by which to adjust the luma component of the color
         * @param chromaGain (optional) factor by which to adjust the chroma
         * component of the color; 1.0 means no change, 0.0 minimizes chroma
         * @see Utils::Color::shade
         */
        QColor darken(const QColor &, qreal amount = 0.5, qreal chromaGain = 1.0);

        /**
         * Adjust the luma and chroma components of a color. The amount is added
         * to the corresponding component.
         *
         * @param lumaAmount amount by which to adjust the luma component of the
         * color; 0.0 results in no change, -1.0 turns anything black, 1.0 turns
         * anything white
         * @param chromaAmount (optional) amount by which to adjust the chroma
         * component of the color; 0.0 results in no change, -1.0 minimizes chroma,
         * 1.0 maximizes chroma
         * @see Utils::Color::luma
         */
        QColor shade(const QColor &, qreal lumaAmount, qreal chromaAmount = 0.0);

        /**
         * Create a new color by tinting one color with another. This function is
         * meant for creating additional colors withings the same class (background,
         * foreground) from colors in a different class. Therefore when @p amount
         * is low, the luma of @p base is mostly preserved, while the hue and
         * chroma of @p color is mostly inherited.
         *
         * @param base color to be tinted
         * @param color color with which to tint
         * @param amount how strongly to tint the base; 0.0 gives @p base,
         * 1.0 gives @p color
         */
        QColor tint(const QColor &base, const QColor &color, qreal amount = 0.3);

        /**
         * Blend two colors into a new color by linear combination.
         * @code
            QColor lighter = Utils::Color::mix(myColor, Qt::white)
         * @endcode
         * @param c1 first color.
         * @param c2 second color.
         * @param bias weight to be used for the mix. @p bias <= 0 gives @p c1,
         * @p bias >= 1 gives @p c2. @p bias == 0.5 gives a 50% blend of @p c1
         * and @p c2.
         */
        QColor mix(const QColor &c1, const QColor &c2,
                   qreal bias = 0.5);

        /**
         * Blend two colors into a new color by painting the second color over the
         * first using the specified composition mode.
         * @code
            QColor white(Qt::white);
            white.setAlphaF(0.5);
            QColor lighter = Utils::Color::overlayColors(myColor, white);
           @endcode
         * @param base the base color (alpha channel is ignored).
         * @param paint the color to be overlayed onto the base color.
         * @param comp the CompositionMode used to do the blending.
         */
        QColor overlayColors(const QColor &base, const QColor &paint,
                             QPainter::CompositionMode comp = QPainter::CompositionMode_SourceOver);

    }
}

#endif // COLORUTILS_H
