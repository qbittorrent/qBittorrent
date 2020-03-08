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

#include "applicationpalette.h"

#include <QApplication>
#include <QPalette>

namespace
{
    void applyColor(const QPalette::ColorRole colorRole, const QColor &color)
    {
        QPalette palette = qApp->palette();
        palette.setColor(colorRole, color);
        qApp->setPalette(palette);
    }

    void applyColor(const QPalette::ColorGroup colorGroup, const QPalette::ColorRole colorRole, const QColor &color)
    {
        QPalette palette = qApp->palette();
        palette.setColor(colorGroup, colorRole, color);
        qApp->setPalette(palette);
    }
} // namespace

ApplicationPalette::ApplicationPalette(QWidget *parent)
    : QWidget(parent)
{
}

QColor ApplicationPalette::window() const
{
    return qApp->palette().color(QPalette::Window);
}

QColor ApplicationPalette::windowText() const
{
    return qApp->palette().color(QPalette::WindowText);
}

QColor ApplicationPalette::windowTextDisabled() const
{
    return qApp->palette().color(QPalette::Disabled, QPalette::WindowText);
}

QColor ApplicationPalette::base() const
{
    return qApp->palette().color(QPalette::Base);
}

QColor ApplicationPalette::alternateBase() const
{
    return qApp->palette().color(QPalette::AlternateBase);
}

QColor ApplicationPalette::text() const
{
    return qApp->palette().color(QPalette::Text);
}

QColor ApplicationPalette::textDisabled() const
{
    return qApp->palette().color(QPalette::Disabled, QPalette::Text);
}

QColor ApplicationPalette::toolTipBase() const
{
    return qApp->palette().color(QPalette::ToolTipBase);
}

QColor ApplicationPalette::toolTipText() const
{
    return qApp->palette().color(QPalette::ToolTipText);
}

QColor ApplicationPalette::toolTipTextDisabled() const
{
    return qApp->palette().color(QPalette::Disabled, QPalette::ToolTipText);
}

QColor ApplicationPalette::brightText() const
{
    return qApp->palette().color(QPalette::BrightText);
}

QColor ApplicationPalette::brightTextDisabled() const
{
    return qApp->palette().color(QPalette::Disabled, QPalette::BrightText);
}

QColor ApplicationPalette::highlight() const
{
    return qApp->palette().color(QPalette::Highlight);
}

QColor ApplicationPalette::highlightedText() const
{
    return qApp->palette().color(QPalette::HighlightedText);
}

QColor ApplicationPalette::highlightedTextDisabled() const
{
    return qApp->palette().color(QPalette::Disabled, QPalette::HighlightedText);
}

QColor ApplicationPalette::button() const
{
    return qApp->palette().color(QPalette::Button);
}

QColor ApplicationPalette::buttonText() const
{
    return qApp->palette().color(QPalette::ButtonText);
}

QColor ApplicationPalette::buttonTextDisabled() const
{
    return qApp->palette().color(QPalette::Disabled, QPalette::ButtonText);
}

QColor ApplicationPalette::link() const
{
    return qApp->palette().color(QPalette::Link);
}

QColor ApplicationPalette::linkVisited() const
{
    return qApp->palette().color(QPalette::LinkVisited);
}

QColor ApplicationPalette::light() const
{
    return qApp->palette().color(QPalette::Light);
}

QColor ApplicationPalette::midlight() const
{
    return qApp->palette().color(QPalette::Midlight);
}

QColor ApplicationPalette::mid() const
{
    return qApp->palette().color(QPalette::Mid);
}

QColor ApplicationPalette::dark() const
{
    return qApp->palette().color(QPalette::Dark);
}

QColor ApplicationPalette::shadow() const
{
    return qApp->palette().color(QPalette::Shadow);
}

void ApplicationPalette::setWindow(const QColor &color)
{
    applyColor(QPalette::Window, color);
}

void ApplicationPalette::setWindowText(const QColor &color)
{
    applyColor(QPalette::WindowText, color);
}

void ApplicationPalette::setWindowTextDisabled(const QColor &color)
{
    applyColor(QPalette::Disabled, QPalette::WindowText, color);
}

void ApplicationPalette::setBase(const QColor &color)
{
    applyColor(QPalette::Base, color);
}

void ApplicationPalette::setAlternateBase(const QColor &color)
{
    applyColor(QPalette::AlternateBase, color);
}

void ApplicationPalette::setText(const QColor &color)
{
    applyColor(QPalette::Text, color);
}

void ApplicationPalette::setTextDisabled(const QColor &color)
{
    applyColor(QPalette::Disabled, QPalette::Text, color);
}

void ApplicationPalette::setToolTipBase(const QColor &color)
{
    applyColor(QPalette::ToolTipBase, color);
}

void ApplicationPalette::setToolTipText(const QColor &color)
{
    applyColor(QPalette::ToolTipText, color);
}

void ApplicationPalette::setToolTipTextDisabled(const QColor &color)
{
    applyColor(QPalette::Disabled, QPalette::ToolTipText, color);
}

void ApplicationPalette::setBrightText(const QColor &color)
{
    applyColor(QPalette::BrightText, color);
}

void ApplicationPalette::setBrightTextDisabled(const QColor &color)
{
    applyColor(QPalette::Disabled, QPalette::BrightText, color);
}

void ApplicationPalette::setHighlight(const QColor &color)
{
    applyColor(QPalette::Highlight, color);
}

void ApplicationPalette::setHighlightedText(const QColor &color)
{
    applyColor(QPalette::HighlightedText, color);
}

void ApplicationPalette::setHighlightedTextDisabled(const QColor &color)
{
    applyColor(QPalette::Disabled, QPalette::HighlightedText, color);
}

void ApplicationPalette::setButton(const QColor &color)
{
    applyColor(QPalette::Button, color);
}

void ApplicationPalette::setButtonText(const QColor &color)
{
    applyColor(QPalette::ButtonText, color);
}

void ApplicationPalette::setButtonTextDisabled(const QColor &color)
{
    applyColor(QPalette::Disabled, QPalette::ButtonText, color);
}

void ApplicationPalette::setLink(const QColor &color)
{
    applyColor(QPalette::Link, color);
}

void ApplicationPalette::setLinkVisited(const QColor &color)
{
    applyColor(QPalette::LinkVisited, color);
}

void ApplicationPalette::setLight(const QColor &color)
{
    applyColor(QPalette::Light, color);
}

void ApplicationPalette::setMidlight(const QColor &color)
{
    applyColor(QPalette::Midlight, color);
}

void ApplicationPalette::setMid(const QColor &color)
{
    applyColor(QPalette::Mid, color);
}

void ApplicationPalette::setDark(const QColor &color)
{
    applyColor(QPalette::Dark, color);
}

void ApplicationPalette::setShadow(const QColor &color)
{
    applyColor(QPalette::Shadow, color);
}
