//------------------------------------------------------------------------------
// fontbutton.cpp
//------------------------------------------------------------------------------
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
// 02110-1301, USA.
//
//------------------------------------------------------------------------------
// Copyright (C) 2010 "Zalewa" <zalewapl@gmail.com>
//------------------------------------------------------------------------------
#include "fontbutton.h"

#include <QFontDialog>

FontButton::FontButton(QWidget *parent)
: QPushButton(parent)
{
    connect(this, SIGNAL(clicked()), SLOT(showFontDialog()));
    this->updateAppearance();
}

void FontButton::resetFont()
{
    setSelectedFont(QFont());
}

const QFont &FontButton::selectedFont() const
{
    return this->currentFont;
}

void FontButton::setSelectedFont(const QFont& font)
{
    this->currentFont = font;
    this->updateAppearance();
}

void FontButton::showFontDialog()
{
    bool ok = false;
    QFont font = QFontDialog::getFont(&ok, this->currentFont, this->parentWidget());
    if (ok)
    {
        this->updateFont(font);
    }
}

void FontButton::updateAppearance()
{
    QString text = QString("%1, %2").arg(this->currentFont.family()).arg(
        this->currentFont.pointSize());
    this->setText(text);

    QFont buttonFont = this->currentFont;
    // Keep font size at default or else we'll get really large buttons.
    buttonFont.setPointSize(QFont().pointSize());
    this->setFont(buttonFont);
}

void FontButton::updateFont(const QFont &newFont)
{
    QFont oldFont = this->currentFont;
    this->currentFont = newFont;
    updateAppearance();

    emit fontUpdated(this->currentFont, oldFont);
}

