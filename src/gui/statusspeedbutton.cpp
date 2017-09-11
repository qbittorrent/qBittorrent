/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017  Eugene Shalygin <eugene.shalygin@gmail.com>
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

#include "statusspeedbutton.h"

#include <cmath>

#include <QApplication>
#include <QStyleOptionButton>
#include <QStylePainter>
#include <QTextOption>

#include "base/unicodestrings.h"
#include "base/utils/misc.h"

StatusSpeedButton::Section::Section(const QString &templateText, int minDigits,
                                    bool isSpeed, bool onlyGrow,
                                    const StatusSpeedButton::SharedInfo *sharedInfo)
    : m_text {emptyText()}
    , m_lastValue {static_cast<quint64>(-1)}
    , m_template {templateText}
    , m_minDigits {minDigits}
    , m_isSpeed {isSpeed}
    , m_onlyGrow {onlyGrow}
    , m_sharedInfo {sharedInfo}
{
}

QStaticText StatusSpeedButton::Section::emptyText()
{
    QStaticText res;
    res.setTextFormat(Qt::PlainText);
    res.setTextOption({QLocale().textDirection() == Qt::LeftToRight ? Qt::AlignRight : Qt::AlignLeft});
    return res;
}

const QStaticText & StatusSpeedButton::Section::text() const
{
    return m_text;
}

void StatusSpeedButton::Section::setValue(quint64 bytes)
{
    if (bytes == m_lastValue) return;

    Utils::Misc::SizeUnit unit;
    qreal dummy;
    friendlyUnit(static_cast<qint64>(bytes), dummy, unit);

    // the widget font is not monospaced, thus some digits occupy much less space than others.
    // we prepend '1' to account for possible highest value of 1023.99
    QString numberText = m_sharedInfo->unityDigit + Utils::Misc::friendlyUnit(static_cast<qint64>(bytes),
        m_minDigits + 1 + Utils::Misc::friendlyUnitPrecision(unit), m_sharedInfo->widestDigit, m_isSpeed);

    // now we replace all other digits with the widest one
    for (int i = 0; i < numberText.size(); ++i) {
        if (numberText[i].isDigit()) {
            numberText[i] = m_sharedInfo->widestDigit;
        }
    }
    // and we've got probably the most wide possible number

    const qreal oldWidth = m_text.textWidth();
    const QString newText = m_template.arg(Utils::Misc::friendlyUnit(static_cast<qint64>(bytes), m_isSpeed));
    if (newText != m_text.text()) {
        m_text.setText(newText);
        const qreal newWidth = QFontMetricsF(m_sharedInfo->widget->font(), m_sharedInfo->widget)
            .width(m_template.arg(numberText));
        if (std::abs(newWidth - oldWidth) > 0.5 && (!m_onlyGrow || (newWidth > oldWidth))) {
            m_text.setTextWidth(newWidth);
            m_sharedInfo->widget->updateGeometry();
        }
        m_sharedInfo->widget->update();
    }
}

void StatusSpeedButton::Section::clear()
{
    const bool wasEmpty = m_text.text().isEmpty();
    m_text = emptyText();
    if (!wasEmpty) {
        m_sharedInfo->widget->updateGeometry();
        m_sharedInfo->widget->update();
    }
}

StatusSpeedButton::StatusSpeedButton(QWidget *parent)
    : QPushButton {parent}
    , m_sharedInfo {widestDecimalDigit(), QLocale().toString(1)[0], this}
    , m_currentSpeed {QLatin1String("%1"), 3, true, true, &m_sharedInfo}
    , m_speedLimit {QLatin1String("[%1]"), 3, true, false, &m_sharedInfo}
    , m_totalPayload {QLatin1String("(%1)"), 2, false, false, &m_sharedInfo}
{
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::PointingHandCursor);

    QFontMetrics fm {this->font(), this};
    m_separatorWidth = fm.width(QLatin1Char(' '));

    setCurrentSpeed(0);
    setSpeedLimit(0);
    setTotalPayload(0);
}

void StatusSpeedButton::setCurrentSpeed(quint64 currentSpeed)
{
    m_currentSpeed.setValue(currentSpeed);
}

void StatusSpeedButton::setSpeedLimit(int speedLimit)
{
    if (speedLimit)
        m_speedLimit.setValue(static_cast<quint64>(speedLimit));
    else
        m_speedLimit.clear();
}

void StatusSpeedButton::setTotalPayload(quint64 totalPayload)
{
    if (totalPayload > 0)
        m_totalPayload.setValue(totalPayload);
    else
        m_totalPayload.clear();
}

QSize StatusSpeedButton::sizeHint() const
{
    QStyleOptionButton style;
    style.initFrom(this);
    const int margin = this->style()->subElementRect(QStyle::SE_PushButtonContents, &style, this).left();

    int totalContentWidth = m_currentSpeed.text().textWidth(); // always rendered
    if (m_speedLimit.text().textWidth() >= 0) {
        totalContentWidth += m_separatorWidth + m_speedLimit.text().textWidth();
    }
    if (m_totalPayload.text().textWidth() >= 0) {
        totalContentWidth += m_separatorWidth + m_totalPayload.text().textWidth();
    }
    if (!icon().isNull()) {
        totalContentWidth += iconSize().width() + margin;
    }
    return QSize(totalContentWidth + 2 * margin, base::sizeHint().height());
}

void StatusSpeedButton::paintEvent(QPaintEvent* /*event*/)
{
    QStyleOptionButton style;
    style.initFrom(this);
    style.icon = QIcon(); // we will draw icon ourselves
    style.features |= QStyleOptionButton::Flat;
    QStylePainter painter(this);
    painter.drawControl(QStyle::CE_PushButton, style);

    const QPoint offset = this->style()->subElementRect(QStyle::SE_PushButtonContents, &style, this).topLeft();

    int hPos = offset.x();

    if (!icon().isNull()) {
        QRect iconRect(offset, iconSize());
        icon().paint(&painter, iconRect, Qt::AlignVCenter);
        hPos += iconSize().width() + offset.x();
    }

    painter.drawStaticText(hPos, 0, m_currentSpeed.text());
    hPos += m_currentSpeed.text().textWidth();
    hPos += m_separatorWidth;
    if (m_speedLimit.text().textWidth() > 0) {
        painter.drawStaticText(hPos, 0, m_speedLimit.text());
        hPos += m_speedLimit.text().textWidth();
        hPos += m_separatorWidth;
    }
    if (m_totalPayload.text().textWidth() > 0) {
        painter.drawStaticText(hPos, 0, m_totalPayload.text());
    }
}

const QChar StatusSpeedButton::widestDecimalDigit()
{
    const QString digits = QLocale().toString(1234567890);
    qreal maxWidth = 0.;
    QChar digit;
    QFontMetricsF fm {font(), this};
    for (int i = 0; i < digits.size(); ++i) {
        const qreal dWidth = fm.width(digits[i]);
        if (dWidth > maxWidth) {
            maxWidth = dWidth;
            digit = digits[i];
        }
    }
    return digit;
}
