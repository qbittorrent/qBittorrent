/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015  Vladimir Golovnev <glassez@yandex.ru>
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

#include <QtGlobal>
#include <QCoreApplication>
#include <QDateTime>
#include <QHash>
#include <QVariant>

#include "base/pathfwd.h"

class QByteArray;
class QHostAddress;
class QString;

struct DataFieldDescriptor;

class GeoIPDatabase
{
    Q_DECLARE_TR_FUNCTIONS(GeoIPDatabase)

public:
    static GeoIPDatabase *load(const Path &filename, QString &error);
    static GeoIPDatabase *load(const QByteArray &data, QString &error);

    ~GeoIPDatabase();

    QString type() const;
    quint16 ipVersion() const;
    QDateTime buildEpoch() const;
    QString lookup(const QHostAddress &hostAddr) const;

private:
    explicit GeoIPDatabase(quint32 size);

    bool parseMetadata(const QVariantHash &metadata, QString &error);
    bool loadDB(QString &error) const;
    QVariantHash readMetadata() const;

    QVariant readDataField(quint32 &offset) const;
    bool readDataFieldDescriptor(quint32 &offset, DataFieldDescriptor &out) const;
    void fromBigEndian(uchar *buf, quint32 len) const;
    QVariant readMapValue(quint32 &offset, quint32 count) const;
    QVariant readArrayValue(quint32 &offset, quint32 count) const;

    template<typename T>
    QVariant readPlainValue(quint32 &offset, quint8 len) const
    {
        T value = 0;
        const uchar *const data = m_data + offset;
        const quint32 availSize = m_size - offset;

        if ((len > 0) && (len <= sizeof(T) && (availSize >= len)))
        {
            // copy input data to last 'len' bytes of 'value'
            uchar *dst = reinterpret_cast<uchar *>(&value) + (sizeof(T) - len);
            memcpy(dst, data, len);
            fromBigEndian(reinterpret_cast<uchar *>(&value), sizeof(T));
            offset += len;
        }

        return QVariant::fromValue(value);
    }

    // Metadata
    quint16 m_ipVersion;
    quint16 m_recordSize;
    quint32 m_nodeCount;
    int m_nodeSize;
    int m_indexSize;
    int m_recordBytes;
    QDateTime m_buildEpoch;
    QString m_dbType;
    // Search data
    mutable QHash<quint32, QString> m_countries;
    quint32 m_size;
    uchar *m_data;
};
