/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2025  Vladimir Golovnev <glassez@yandex.ru>
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

#include <QApplication>
#include <QColorSpace>
#include <QColorTransform>
#include <QPainter>
#include <QPalette>
#include <QStyleOptionProgressBar>
#include <QStyleOptionViewItem>

#if (defined(Q_OS_WIN) || defined(Q_OS_MACOS))
#include <QProxyStyle>
#endif

#include "base/global.h"
#include "base/preferences.h"
#include "gui/uithememanager.h"

ProgressBarPainter::ProgressBarPainter(QObject *parent)
    : QObject(parent)
    , m_useCustomTheme {Preferences::instance()->useCustomUITheme()}
{
#if (defined(Q_OS_WIN) || defined(Q_OS_MACOS))
    auto *fusionStyle = new QProxyStyle(u"fusion"_s);
    fusionStyle->setParent(&m_dummyProgressBar);
    m_dummyProgressBar.setStyle(fusionStyle);
#endif

    applyUITheme();
    connect(UIThemeManager::instance(), &UIThemeManager::themeChanged, this, &ProgressBarPainter::applyUITheme);
}

void ProgressBarPainter::paint(QPainter *painter, const QStyleOptionViewItem &option, const QString &text, const qreal progress, const QColor &color) const
{
    if (!m_useCustomTheme)
    {
        painter->save();
        if (option.state.testFlag(QStyle::State_Selected))
            painter->fillRect(option.rect, option.palette.highlight());
        else
            m_dummyProgressBar.style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);
        const QRect cell = option.rect.adjusted(3, 1, -3, -1);
        if (cell.isEmpty())
        {
            painter->restore();
            return;
        }
        const int height = qMin(cell.height(), option.fontMetrics.height());
        const QRectF track(cell.x(), cell.y() + (cell.height() - height) / 2, cell.width(), height);
        QPalette palette = option.palette;
        palette.setCurrentColorGroup(option.state.testFlag(QStyle::State_Enabled) ? QPalette::Active : QPalette::Disabled);
        QColor fill = color.isValid() ? color : m_chunkColor;
        if (!fill.isValid())
            fill = QApplication::palette().color(QPalette::Active, QPalette::Highlight);
        painter->fillRect(track, palette.base());
        QColor trackTint = palette.text().color();
        trackTint.setAlphaF(0.12);
        painter->fillRect(track, trackTint);
        const qreal width = track.width() * qBound(0.0, progress, 100.0) / 100.0;
        const QRectF filled((option.direction == Qt::RightToLeft) ? track.right() - width : track.left(),
            track.top(), width, track.height());
        painter->fillRect(filled, fill);
        painter->setFont(option.font);
        const QRectF remaining((option.direction == Qt::RightToLeft) ? track.left() : filled.right(),
            track.top(), track.width() - width, track.height());
        painter->save();
        painter->setClipRect(remaining, Qt::IntersectClip);
        painter->setPen(palette.text().color());
        painter->drawText(track, Qt::AlignCenter, text);
        painter->restore();
        painter->save();
        painter->setClipRect(filled, Qt::IntersectClip);
        // Status-coloured fills can be very light even when the theme uses white text.
        static const auto toLinear = QColorSpace(QColorSpace::SRgb).transformationToColorSpace(QColorSpace::SRgbLinear);
        const QColor linearFill = toLinear.map(fill);
        const qreal luminance = 0.2126 * linearFill.redF() + 0.7152 * linearFill.greenF() + 0.0722 * linearFill.blueF();
        painter->setPen((luminance > 0.179) ? Qt::black : Qt::white);
        painter->drawText(track, Qt::AlignCenter, text);
        painter->restore();
        painter->restore();
        return;
    }

    QStyleOptionProgressBar styleOption;
    styleOption.initFrom(&m_dummyProgressBar);
    // QStyleOptionProgressBar fields
    styleOption.maximum = 100;
    styleOption.minimum = 0;
    styleOption.progress = static_cast<int>(progress);
    styleOption.text = text;
    styleOption.textVisible = true;
    // QStyleOption fields
    styleOption.rect = option.rect.adjusted(0, 1, 0, -1);
    // Qt 6 requires QStyle::State_Horizontal to be set for correctly drawing horizontal progress bar
    styleOption.state = option.state | QStyle::State_Horizontal;

    const bool isEnabled = option.state.testFlag(QStyle::State_Enabled);
    styleOption.palette.setCurrentColorGroup(isEnabled ? QPalette::Active : QPalette::Disabled);

    if (color.isValid())
    {
        styleOption.palette.setColor(QPalette::Highlight, color);
    }
    else if (m_chunkColor.isValid())
    {
        styleOption.palette.setColor(QPalette::Highlight, m_chunkColor);
    }

    painter->save();
    const QStyle *style = m_dummyProgressBar.style();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);
    style->drawControl(QStyle::CE_ProgressBar, &styleOption, painter, &m_dummyProgressBar);
    painter->restore();
}

void ProgressBarPainter::applyUITheme()
{
    m_chunkColor = UIThemeManager::instance()->getColor(u"ProgressBar"_s);
}
