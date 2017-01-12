//------------------------------------------------------------------------------
// fontbutton.h
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
#ifndef FONTBUTTON_H
#define FONTBUTTON_H

#include <QFont>
#include <QPushButton>

/**
 * @brief A button widget allowing to select a font.
 *
 * Widget will display the font name and size using the selected font
 * to print the text (but defaulting to QFont()'s size). When user
 * picks a new font by hand, fontUpdated() signal is emitted. This signal
 * is not emitted if new font is set programmatically through setSelectedFont()
 * or resetFont().
 */
class FontButton : public QPushButton
{
Q_OBJECT

public:
    FontButton(QWidget *parent = NULL);

    /**
     * @brief Currently selected font.
     *
     * @return QFont as set by the user or by the setter or by resetFont().
     */
    const QFont &selectedFont() const;

    /**
     * @brief Sets font on this button to QFont().
     *
     * Button appearance will be updated.
     * fontUpdated() signal will *not* be emitted.
     */
    void setSelectedFont(const QFont &font);

public slots:
    /**
     * @brief Resets font to QFont().
     *
     * Button appearance will be updated.
     * fontUpdated() signal will *not* be emitted.
     */
    void resetFont();

signals:
    /**
     * @brief Emitted when user changes the font.
     *
     * @param newFont Newly set font.
     * @param oldFont Previous font.
     */
    void fontUpdated(QFont newFont, QFont oldFont);

private:
    QFont currentFont;

    void updateAppearance();

    /**
     * Will always emit fontUpdated() signal.
     */
    void updateFont(const QFont &newFont);

private slots:
    void showFontDialog();
};

#endif
