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

#ifndef QBT_STATUSSPEEDBUTTON_H
#define QBT_STATUSSPEEDBUTTON_H

#include <QPushButton>
#include <QStaticText>

/**
* @brief Status bar button with speed text
*
* Allocates three fields for current speed, speed limit, and payload size and tries
* to minimise width variations for those fields
*
*/
class StatusSpeedButton : public QPushButton
{
    using base = QPushButton;

public:
    explicit StatusSpeedButton(QWidget *parent = nullptr);

    void setCurrentSpeed(quint64 currentSpeed);
    void setSpeedLimit(int speedLimit);
    void setTotalPayload(quint64 totalPayload);

protected:
    QSize sizeHint() const override;
    void paintEvent(QPaintEvent *event) override;

private:

    struct SharedInfo
    {
        QChar widestDigit;
        QChar unityDigit;
        QWidget *widget;
    };

    class Section
    {
    public:
        Section(const QString &templateText, int minDigits, bool isSpeed, bool onlyGrow,
                const SharedInfo *sharedInfo);
        /**
        * @brief Sets new text possibly updating width

        * @param[in] bytes new value (bytes)
        */
        void setValue(quint64 bytes);
        void clear();

        const QStaticText &text() const;

    private:
        static QStaticText emptyText();

        QStaticText m_text;
        quint64 m_lastValue;

        QString m_template;
        int m_minDigits;
        bool m_isSpeed; // move to the template?
        bool m_onlyGrow;

        const SharedInfo *m_sharedInfo;
    };

    /**
    * @brief Find widest digits in the current widget font
    */
    const QChar widestDecimalDigit();


    SharedInfo m_sharedInfo;

    Section m_currentSpeed;
    Section m_speedLimit;
    Section m_totalPayload;

    int m_separatorWidth;
};

#endif // QBT_STATUSSPEEDBUTTON_H
