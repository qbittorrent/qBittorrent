/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015 Eugene Shalygin
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

#ifndef TORRENTMODEL_P_H
#define TORRENTMODEL_P_H

#include <QObject>
#include <QColor>
#include <QMap>

#include "base/bittorrent/torrenthandle.h"

class QProcessEnvironment;
class QSettings;

typedef QMap<BitTorrent::TorrentState, QColor> TorrentStateColorsMap;

class StateColorsProvider
{
public:
    virtual ~StateColorsProvider(){}
    virtual void setColors(TorrentStateColorsMap& m) = 0;
protected:
    static bool isDarkTheme();
};

class DefaultColorsProvider: public StateColorsProvider
{
    // StateColorsProvider interface

public:
    virtual void setColors(TorrentStateColorsMap &m);
};

#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC))
struct KDEColorEffect
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


class KDEColorsProvider: public StateColorsProvider
{
public:
    // StateColorsProvider interface
    virtual void setColors(TorrentStateColorsMap &m);
};

#endif

class TorrentStateColors: public QObject
{
    Q_OBJECT

public:
    static TorrentStateColors* instance();
    TorrentStateColors();
    QColor color(BitTorrent::TorrentState state) const
    {
        return stateColors.value(state, Qt::red);
    }

private slots:
    void setupColors();

private:
    QMap<BitTorrent::TorrentState, QColor> stateColors;
};

#endif // TORRENTMODEL_P_H

