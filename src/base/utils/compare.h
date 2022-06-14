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
#include <QtGlobal>

#if !defined(Q_OS_WIN) && (!defined(Q_OS_UNIX) || defined(Q_OS_MACOS) || defined(QT_FEATURE_icu))
#define QBT_USE_QCOLLATOR
#include <QCollator>
#endif

class QString;

namespace Utils::Compare
{
#ifdef QBT_USE_QCOLLATOR
    template <Qt::CaseSensitivity caseSensitivity>
    class NaturalCompare
    {
    public:
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
    };
#else
    int naturalCompare(const QString &left, const QString &right, Qt::CaseSensitivity caseSensitivity);

    template <Qt::CaseSensitivity caseSensitivity>
    class NaturalCompare
    {
    public:
        int operator()(const QString &left, const QString &right) const
        {
            return naturalCompare(left, right, caseSensitivity);
        }
    };
#endif

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
