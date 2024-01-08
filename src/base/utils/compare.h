/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2021  Mike Tzou (Chocobo1)
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

#pragma once

#include <Qt>
#include <QtSystemDetection>

// for QT_FEATURE_xxx, see: https://wiki.qt.io/Qt5_Build_System#How_to
#include <QtCore/private/qtcore-config_p.h>

#ifndef QBT_USE_QCOLLATOR
// macOS and Windows support 'case sensitivity' and 'numeric mode' natively
// https://github.com/qt/qtbase/blob/6.0/src/corelib/CMakeLists.txt#L777-L793
// https://github.com/qt/qtbase/blob/6.0/src/corelib/text/qcollator_macx.cpp#L74-L77
// https://github.com/qt/qtbase/blob/6.0/src/corelib/text/qcollator_win.cpp#L72-L78
#if ((QT_FEATURE_icu == 1) || defined(Q_OS_MACOS) || defined(Q_OS_WIN))
#define QBT_USE_QCOLLATOR 1
#include <QCollator>
#else
#define QBT_USE_QCOLLATOR 0
#endif
#endif

class QString;

namespace Utils::Compare
{
    int naturalCompare(const QString &left, const QString &right, Qt::CaseSensitivity caseSensitivity);

    template <Qt::CaseSensitivity caseSensitivity>
    class NaturalCompare
    {
    public:
#if (QBT_USE_QCOLLATOR == 0)
        int operator()(const QString &left, const QString &right) const
        {
            return naturalCompare(left, right, caseSensitivity);
        }
#else
        NaturalCompare()
        {
            m_collator.setNumericMode(true);
            m_collator.setCaseSensitivity(caseSensitivity);
        }

        int operator()(const QString &left, const QString &right) const
        {
            return m_collator.compare(left, right);
        }

    private:
        QCollator m_collator;
#endif
    };

    template <Qt::CaseSensitivity caseSensitivity>
    class NaturalLessThan
    {
    public:
        bool operator()(const QString &left, const QString &right) const
        {
            return (m_comparator(left, right) < 0);
        }

    private:
        NaturalCompare<caseSensitivity> m_comparator;
    };
}
