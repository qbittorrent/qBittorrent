/* This file is composed from stripped versions of
 * kcolorutils.cpp, kguiaddons_colorhelpers_p.h,
 * kcolorspaces_p.h, and kcolorspaces.cpp
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
#include "colorutils.h"

#include <QColor>
#include <QImage>

#include <QtNumeric> // qIsNaN

#include <math.h>

// BEGIN internal helper functions
namespace {
    // normalize: like qBound(a, 0.0, 1.0) but without needing the args and with
    // "safer" behavior on NaN (isnan(a) -> return 0.0)
    inline qreal normalize(qreal a)
    {
        return (a < 1.0 ? (a > 0.0 ? a : 0.0) : 1.0);
    }

    inline qreal mixQreal(qreal a, qreal b, qreal bias)
    {
        return a + (b - a) * bias;
    }

    inline qreal wrap(qreal a, qreal d = 1.0)
    {
        qreal r = fmod(a, d);
        return (r < 0.0 ? d + r : (r > 0.0 ? r : 0.0));
    }

    // END internal helper functions

    class HCY
    {
    public:
        explicit HCY(const QColor &);
        explicit HCY(qreal h_, qreal c_, qreal y_, qreal a_ = 1.0);
        QColor qColor() const;
        qreal h, c, y, a;
        static qreal luma(const QColor &);
    private:
        static qreal gamma(qreal);
        static qreal igamma(qreal);
        static qreal lumag(qreal, qreal, qreal);
    };

    ///////////////////////////////////////////////////////////////////////////////
    // HCY color space

#define HCY_REC 709 // use 709 for now
#if   HCY_REC == 601
    static const qreal yc[3] = { 0.299, 0.587, 0.114 };
#elif HCY_REC == 709
    static const qreal yc[3] = {0.2126, 0.7152, 0.0722};
#else // use Qt values
    static const qreal yc[3] = { 0.34375, 0.5, 0.15625 };
#endif

    qreal HCY::gamma(qreal n)
    {
        return pow(normalize(n), 2.2);
    }

    qreal HCY::igamma(qreal n)
    {
        return pow(normalize(n), 1.0 / 2.2);
    }

    qreal HCY::lumag(qreal r, qreal g, qreal b)
    {
        return r * yc[0] + g * yc[1] + b * yc[2];
    }

#if 0
    HCY::HCY(qreal h_, qreal c_, qreal y_, qreal a_)
    {
        h = h_;
        c = c_;
        y = y_;
        a = a_;
    }
#endif

    HCY::HCY(const QColor &color)
    {
        qreal r = gamma(color.redF());
        qreal g = gamma(color.greenF());
        qreal b = gamma(color.blueF());
        a = color.alphaF();

        // luma component
        y = lumag(r, g, b);

        // hue component
        qreal p = qMax(qMax(r, g), b);
        qreal n = qMin(qMin(r, g), b);
        qreal d = 6.0 * (p - n);
        if (n == p)
            h = 0.0;
        else if (r == p)
            h = ((g - b) / d);
        else if (g == p)
            h = ((b - r) / d) + (1.0 / 3.0);
        else
            h = ((r - g) / d) + (2.0 / 3.0);

        // chroma component
        if (( r == g) && ( g == b) )
            c = 0.0;
        else
            c = qMax((y - n) / y, (p - y) / (1 - y));
    }

    QColor HCY::qColor() const
    {
        // start with sane component values
        qreal _h = wrap(h);
        qreal _c = normalize(c);
        qreal _y = normalize(y);

        // calculate some needed variables
        qreal _hs = _h * 6.0, th, tm;
        if (_hs < 1.0) {
            th = _hs;
            tm = yc[0] + yc[1] * th;
        }
        else if (_hs < 2.0) {
            th = 2.0 - _hs;
            tm = yc[1] + yc[0] * th;
        }
        else if (_hs < 3.0) {
            th = _hs - 2.0;
            tm = yc[1] + yc[2] * th;
        }
        else if (_hs < 4.0) {
            th = 4.0 - _hs;
            tm = yc[2] + yc[1] * th;
        }
        else if (_hs < 5.0) {
            th = _hs - 4.0;
            tm = yc[2] + yc[0] * th;
        }
        else {
            th = 6.0 - _hs;
            tm = yc[0] + yc[2] * th;
        }

        // calculate RGB channels in sorted order
        qreal tn, to, tp;
        if (tm >= _y) {
            tp = _y + _y * _c * (1.0 - tm) / tm;
            to = _y + _y * _c * (th - tm) / tm;
            tn = _y - (_y * _c);
        }
        else {
            tp = _y + (1.0 - _y) * _c;
            to = _y + (1.0 - _y) * _c * (th - tm) / (1.0 - tm);
            tn = _y - (1.0 - _y) * _c * tm / (1.0 - tm);
        }

        // return RGB channels in appropriate order
        if (_hs < 1.0)
            return QColor::fromRgbF(igamma(tp), igamma(to), igamma(tn), a);
        else if (_hs < 2.0)
            return QColor::fromRgbF(igamma(to), igamma(tp), igamma(tn), a);
        else if (_hs < 3.0)
            return QColor::fromRgbF(igamma(tn), igamma(tp), igamma(to), a);
        else if (_hs < 4.0)
            return QColor::fromRgbF(igamma(tn), igamma(to), igamma(tp), a);
        else if (_hs < 5.0)
            return QColor::fromRgbF(igamma(to), igamma(tn), igamma(tp), a);
        else
            return QColor::fromRgbF(igamma(tp), igamma(tn), igamma(to), a);
    }

    qreal HCY::luma(const QColor &color)
    {
        return lumag(gamma(color.redF()),
                     gamma(color.greenF()),
                     gamma(color.blueF()));
    }

}

qreal Utils::Color::luma(const QColor &color)
{
    return HCY::luma(color);
}

void Utils::Color::getHcy(const QColor &color, qreal *h, qreal *c, qreal *y, qreal *a)
{
    if (!c || !h || !y)
        return;
    HCY khcy(color);
    *c = khcy.c;
    *h = khcy.h;
    *y = khcy.y;
    if (a)
        *a = khcy.a;
}

static qreal contrastRatioForLuma(qreal y1, qreal y2)
{
    if (y1 > y2)
        return (y1 + 0.05) / (y2 + 0.05);
    else
        return (y2 + 0.05) / (y1 + 0.05);
}

qreal Utils::Color::contrastRatio(const QColor &c1, const QColor &c2)
{
    return contrastRatioForLuma(luma(c1), luma(c2));
}

QColor Utils::Color::lighten(const QColor &color, qreal ky, qreal kc)
{
    HCY c(color);
    c.y = 1.0 - normalize((1.0 - c.y) * (1.0 - ky));
    c.c = 1.0 - normalize((1.0 - c.c) * kc);
    return c.qColor();
}

QColor Utils::Color::darken(const QColor &color, qreal ky, qreal kc)
{
    HCY c(color);
    c.y = normalize(c.y * (1.0 - ky));
    c.c = normalize(c.c * kc);
    return c.qColor();
}

QColor Utils::Color::shade(const QColor &color, qreal ky, qreal kc)
{
    HCY c(color);
    c.y = normalize(c.y + ky);
    c.c = normalize(c.c + kc);
    return c.qColor();
}

static QColor tintHelper(const QColor &base, qreal baseLuma, const QColor &color, qreal amount)
{
    HCY result(Utils::Color::mix(base, color, pow(amount, 0.3)));
    result.y = mixQreal(baseLuma, result.y, amount);

    return result.qColor();
}

QColor Utils::Color::tint(const QColor &base, const QColor &color, qreal amount)
{
    if (amount <= 0.0)
        return base;
    if (amount >= 1.0)
        return color;
    if (qIsNaN(amount))
        return base;

    qreal baseLuma = luma(base); //cache value because luma call is expensive
    double ri = contrastRatioForLuma(baseLuma, luma(color));
    double rg = 1.0 + ((ri + 1.0) * amount * amount * amount);
    double u = 1.0, l = 0.0;
    QColor result;
    for (int i = 12; i; --i) {
        double a = 0.5 * (l + u);
        result = tintHelper(base, baseLuma, color, a);
        double ra = contrastRatioForLuma(baseLuma, luma(result));
        if (ra > rg)
            u = a;
        else
            l = a;
    }
    return result;
}

QColor Utils::Color::mix(const QColor &c1, const QColor &c2, qreal bias)
{
    if (bias <= 0.0)
        return c1;
    if (bias >= 1.0)
        return c2;
    if (qIsNaN(bias))
        return c1;

    qreal r = mixQreal(c1.redF(),   c2.redF(),   bias);
    qreal g = mixQreal(c1.greenF(), c2.greenF(), bias);
    qreal b = mixQreal(c1.blueF(),  c2.blueF(),  bias);
    qreal a = mixQreal(c1.alphaF(), c2.alphaF(), bias);

    return QColor::fromRgbF(r, g, b, a);
}

QColor Utils::Color::overlayColors(const QColor &base, const QColor &paint,
                                   QPainter::CompositionMode comp)
{
    // This isn't the fastest way, but should be "fast enough".
    // It's also the only safe way to use QPainter::CompositionMode
    QImage img(1, 1, QImage::Format_ARGB32_Premultiplied);
    QPainter p(&img);
    QColor start = base;
    start.setAlpha(255); // opaque
    p.fillRect(0, 0, 1, 1, start);
    p.setCompositionMode(comp);
    p.fillRect(0, 0, 1, 1, paint);
    p.end();
    return img.pixel(0, 0);
}

