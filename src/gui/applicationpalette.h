/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2020  Prince Gupta <jagannatharjun11@gmail.com>
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

#pragma once

#include <QWidget>

class ApplicationPalette : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(QColor window READ window WRITE setWindow)
    Q_PROPERTY(QColor windowText READ windowText WRITE setWindowText)
    Q_PROPERTY(QColor windowTextDisabled READ windowTextDisabled WRITE setWindowTextDisabled)
    Q_PROPERTY(QColor base READ base WRITE setBase)
    Q_PROPERTY(QColor alternateBase READ alternateBase WRITE setAlternateBase)
    Q_PROPERTY(QColor text READ text WRITE setText)
    Q_PROPERTY(QColor textDisabled READ textDisabled WRITE setTextDisabled)
    Q_PROPERTY(QColor toolTipBase READ toolTipBase WRITE setToolTipBase)
    Q_PROPERTY(QColor toolTipText READ toolTipText WRITE setToolTipText)
    Q_PROPERTY(QColor toolTipTextDisabled READ toolTipTextDisabled WRITE setToolTipTextDisabled)
    Q_PROPERTY(QColor brightText READ brightText WRITE setBrightText)
    Q_PROPERTY(QColor brightTextDisabled READ brightTextDisabled WRITE setBrightTextDisabled)
    Q_PROPERTY(QColor highlight READ highlight WRITE setHighlight)
    Q_PROPERTY(QColor highlightedText READ highlightedText WRITE setHighlightedText)
    Q_PROPERTY(QColor highlightedTextDisabled READ highlightedTextDisabled WRITE setHighlightedTextDisabled)
    Q_PROPERTY(QColor button READ button WRITE setButton)
    Q_PROPERTY(QColor buttonText READ buttonText WRITE setButtonText)
    Q_PROPERTY(QColor buttonTextDisabled READ buttonTextDisabled WRITE setButtonTextDisabled)
    Q_PROPERTY(QColor link READ link WRITE setLink)
    Q_PROPERTY(QColor linkVisited READ linkVisited WRITE setLinkVisited)
    Q_PROPERTY(QColor light READ light WRITE setLight)
    Q_PROPERTY(QColor midlight READ midlight WRITE setMidlight)
    Q_PROPERTY(QColor mid READ mid WRITE setMid)
    Q_PROPERTY(QColor dark READ dark WRITE setDark)
    Q_PROPERTY(QColor shadow READ shadow WRITE setShadow)

public:
    explicit ApplicationPalette(QWidget *parent = nullptr);

private:
    QColor window() const;
    QColor windowText() const;
    QColor windowTextDisabled() const;
    QColor base() const;
    QColor alternateBase() const;
    QColor text() const;
    QColor textDisabled() const;
    QColor toolTipBase() const;
    QColor toolTipText() const;
    QColor toolTipTextDisabled() const;
    QColor brightText() const;
    QColor brightTextDisabled() const;
    QColor highlight() const;
    QColor highlightedText() const;
    QColor highlightedTextDisabled() const;
    QColor button() const;
    QColor buttonText() const;
    QColor buttonTextDisabled() const;
    QColor link() const;
    QColor linkVisited() const;
    QColor light() const;
    QColor midlight() const;
    QColor mid() const;
    QColor dark() const;
    QColor shadow() const;

    void setWindow(const QColor &color);
    void setWindowText(const QColor &color);
    void setWindowTextDisabled(const QColor &color);
    void setBase(const QColor &color);
    void setAlternateBase(const QColor &color);
    void setText(const QColor &color);
    void setTextDisabled(const QColor &color);
    void setToolTipBase(const QColor &color);
    void setToolTipText(const QColor &color);
    void setToolTipTextDisabled(const QColor &color);
    void setBrightText(const QColor &color);
    void setBrightTextDisabled(const QColor &color);
    void setHighlight(const QColor &color);
    void setHighlightedText(const QColor &color);
    void setHighlightedTextDisabled(const QColor &color);
    void setButton(const QColor &color);
    void setButtonText(const QColor &color);
    void setButtonTextDisabled(const QColor &color);
    void setLink(const QColor &color);
    void setLinkVisited(const QColor &color);
    void setLight(const QColor &color);
    void setMidlight(const QColor &color);
    void setMid(const QColor &color);
    void setDark(const QColor &color);
    void setShadow(const QColor &color);
};
