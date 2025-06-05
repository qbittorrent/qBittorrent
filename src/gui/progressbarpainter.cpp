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

#include "progressbarpainter.h"

#include <QPainter>
#include <QPalette>
#include <QStyleOptionProgressBar>
#include <QStyleOptionViewItem>

#if (defined(Q_OS_WIN) || defined(Q_OS_MACOS))
#include <QProxyStyle>
#endif

#include "base/global.h"

ProgressBarPainter::ProgressBarPainter()
{
#if (defined(Q_OS_WIN) || defined(Q_OS_MACOS))
    auto *fusionStyle = new QProxyStyle {u"fusion"_s};
    fusionStyle->setParent(&m_dummyProgressBar);
    m_dummyProgressBar.setStyle(fusionStyle);
#endif
}

void ProgressBarPainter::paint(QPainter *painter, const QStyleOptionViewItem &option, const QString &text, const int progress) const
{
    QStyleOptionProgressBar styleOption;
    styleOption.initFrom(&m_dummyProgressBar);
    // QStyleOptionProgressBar fields
    styleOption.maximum = 100;
    styleOption.minimum = 0;
    styleOption.progress = progress;
    styleOption.text = text;
    styleOption.textVisible = true;
    // QStyleOption fields
    styleOption.rect = option.rect;
    // Qt 6 requires QStyle::State_Horizontal to be set for correctly drawing horizontal progress bar
    styleOption.state = option.state | QStyle::State_Horizontal;

    const bool isEnabled = option.state.testFlag(QStyle::State_Enabled);
    styleOption.palette.setCurrentColorGroup(isEnabled ? QPalette::Active : QPalette::Disabled);

    painter->save();
    const QStyle *style = m_dummyProgressBar.style();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);
    style->drawControl(QStyle::CE_ProgressBar, &styleOption, painter, &m_dummyProgressBar);
    painter->restore();
}
