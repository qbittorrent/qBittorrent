/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2017  Zalewa
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
 *
 * Contact : zalewapl@gmail.com
 */
#ifndef FONTPICKER_H
#define FONTPICKER_H

#include <QFont>
#include <QPushButton>
#include <QWidget>

class FontButton;

/**
 * @brief Font picker widget with buttons to pick and reset font.
 *
 * This widget shows two buttons in horizontal layout - FontButton and
 * a normal push button to reset the picked custom font back to QFont().
 *
 * Whenever font is changed by user action (pick or reset), fontUpdated()
 * signal is emitted. Changing the font programmatically will *not* cause
 * the signal to be emitted.
 */
class FontPicker : public QWidget
{
Q_OBJECT

public:
    FontPicker(QWidget *parent = NULL);

    QFont selectedFont() const;
    void setSelectedFont(const QFont &font);

signals:
    void fontUpdated(QFont newFont, QFont oldFont);

private:
    FontButton *fontButton;
    QPushButton *resetButton;

private slots:
    void resetFont();
};

#endif
