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

#include "geoipdatabase.h"

#include <QByteArray>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QHostAddress>
#include <QVariant>

#include "base/global.h"
#include "base/path.h"

namespace
{
    const qint32 MAX_FILE_SIZE = 67108864; // 64MB
    const quint32 MAX_METADATA_SIZE = 131072; // 128KB
    const QByteArray METADATA_BEGIN_MARK = QByteArrayLiteral("\xab\xcd\xefMaxMind.com");
    const char DATA_SECTION_SEPARATOR[16] = {0};

    enum class DataType
    {
        Unknown = 0,
        Pointer = 1,
        String = 2,
        Double = 3,
        Bytes = 4,
        Integer16 = 5,
        Integer32 = 6,
        Map = 7,
        SignedInteger32 = 8,
        Integer64 = 9,
        Integer128 = 10,
        Array = 11,
        DataCacheContainer = 12,
        EndMarker = 13,
        Boolean = 14,
        Float = 15
    };
}

struct DataFieldDescriptor
{
    DataType fieldType {DataType::Unknown};
    union
    {
        quint32 fieldSize = 0;
        quint32 offset; // Pointer
    };
};

GeoIPDatabase::GeoIPDatabase(const quint32 size)
    : m_size(size)
    , m_data(new uchar[size])
{
}

GeoIPDatabase *GeoIPDatabase::load(const Path &filename, QString &error)
{
    QFile file {filename.data()};
    if (file.size() > MAX_FILE_SIZE)
    {
        error = tr("Unsupported database file size.");
        return nullptr;
    }

    if (!file.open(QFile::ReadOnly))
    {
        error = file.errorString();
        return nullptr;
    }

    auto *db = new GeoIPDatabase(file.size());

    if (file.read(reinterpret_cast<char *>(db->m_data), db->m_size) != db->m_size)
    {
        error = file.errorString();
        delete db;
        return nullptr;
    }


    if (!db->parseMetadata(db->readMetadata(), error) || !db->loadDB(error))
    {
        delete db;
        return nullptr;
    }

    return db;
}

GeoIPDatabase *GeoIPDatabase::load(const QByteArray &data, QString &error)
{
    if (data.size() > MAX_FILE_SIZE)
    {
        error = tr("Unsupported database file size.");
        return nullptr;
    }

    auto *db = new GeoIPDatabase(data.size());

    memcpy(reinterpret_cast<char *>(db->m_data), data.constData(), db->m_size);

    if (!db->parseMetadata(db->readMetadata(), error) || !db->loadDB(error))
    {
        delete db;
        return nullptr;
    }

    return db;
}

GeoIPDatabase::~GeoIPDatabase()
{
    delete [] m_data;
}

QString GeoIPDatabase::type() const
{
    return m_dbType;
}

quint16 GeoIPDatabase::ipVersion() const
{
    return m_ipVersion;
}

QDateTime GeoIPDatabase::buildEpoch() const
{
    return m_buildEpoch;
}

QString GeoIPDatabase::lookup(const QHostAddress &hostAddr) const
{
    Q_IPV6ADDR addr = hostAddr.toIPv6Address();

    const uchar *ptr = m_data;

    for (int i = 0; i < 16; ++i)
    {
        for (int j = 0; j < 8; ++j)
        {
            const bool right = static_cast<bool>((addr[i] >> (7 - j)) & 1);
            // Interpret the left/right record as number
            if (right)
                ptr += m_recordBytes;

            quint32 id = 0;
            auto *idPtr = reinterpret_cast<uchar *>(&id);
            memcpy(&idPtr[4 - m_recordBytes], ptr, m_recordBytes);
            fromBigEndian(idPtr, 4);

            if (id == m_nodeCount)
            {
                return {};
            }
            if (id > m_nodeCount)
            {
                QString country = m_countries.value(id);
                if (country.isEmpty())
                {
                    const quint32 offset = id - m_nodeCount - sizeof(DATA_SECTION_SEPARATOR);
                    quint32 tmp = offset + m_indexSize + sizeof(DATA_SECTION_SEPARATOR);
                    const QVariant val = readDataField(tmp);
                    if (val.userType() == QMetaType::QVariantHash)
                    {
                        country = val.toHash()[u"country"_s].toHash()[u"iso_code"_s].toString();
                        m_countries[id] = country;
                    }
                }
                return country;
            }

            ptr = m_data + (id * m_nodeSize);
        }
    }

    return {};
}

#define CHECK_METADATA_REQ(key, type) \
if (!metadata.contains(key)) \
{ \
    error = errMsgNotFound.arg(key); \
    return false; \
} \
if (metadata.value(key).userType() != QMetaType::type) \
{ \
    error = errMsgInvalid.arg(key);  \
    return false; \
}

#define CHECK_METADATA_OPT(key, type) \
if (metadata.contains(key)) \
{ \
    if (metadata.value(key).userType() != QMetaType::type) \
    { \
        error = errMsgInvalid.arg(key);  \
        return false; \
    } \
}

bool GeoIPDatabase::parseMetadata(const QVariantHash &metadata, QString &error)
{
    const QString errMsgNotFound = tr("Metadata error: '%1' entry not found.");
    const QString errMsgInvalid = tr("Metadata error: '%1' entry has invalid type.");

    qDebug() << "Parsing MaxMindDB metadata...";

    CHECK_METADATA_REQ(u"binary_format_major_version"_s, UShort);
    CHECK_METADATA_REQ(u"binary_format_minor_version"_s, UShort);
    const uint versionMajor = metadata.value(u"binary_format_major_version"_s).toUInt();
    const uint versionMinor = metadata.value(u"binary_format_minor_version"_s).toUInt();
    if (versionMajor != 2)
    {
        error = tr("Unsupported database version: %1.%2").arg(versionMajor).arg(versionMinor);
        return false;
    }

    CHECK_METADATA_REQ(u"ip_version"_s, UShort);
    m_ipVersion = metadata.value(u"ip_version"_s).value<quint16>();
    if (m_ipVersion != 6)
    {
        error = tr("Unsupported IP version: %1").arg(m_ipVersion);
        return false;
    }

    CHECK_METADATA_REQ(u"record_size"_s, UShort);
    m_recordSize = metadata.value(u"record_size"_s).value<quint16>();
    if (m_recordSize != 24)
    {
        error = tr("Unsupported record size: %1").arg(m_recordSize);
        return false;
    }
    m_nodeSize = m_recordSize / 4;
    m_recordBytes = m_nodeSize / 2;

    CHECK_METADATA_REQ(u"node_count"_s, UInt);
    m_nodeCount = metadata.value(u"node_count"_s).value<quint32>();
    m_indexSize = m_nodeCount * m_nodeSize;

    CHECK_METADATA_REQ(u"database_type"_s, QString);
    m_dbType = metadata.value(u"database_type"_s).toString();

    CHECK_METADATA_REQ(u"build_epoch"_s, ULongLong);
    m_buildEpoch = QDateTime::fromSecsSinceEpoch(metadata.value(u"build_epoch"_s).toULongLong());

    CHECK_METADATA_OPT(u"languages"_s, QVariantList);
    CHECK_METADATA_OPT(u"description"_s, QVariantHash);

    return true;
}

bool GeoIPDatabase::loadDB(QString &error) const
{
    qDebug() << "Parsing IP geolocation database index tree...";

    const int nodeSize = m_recordSize / 4; // in bytes
    const int indexSize = m_nodeCount * nodeSize;
    if ((m_size < (indexSize + sizeof(DATA_SECTION_SEPARATOR)))
        || (memcmp(m_data + indexSize, DATA_SECTION_SEPARATOR, sizeof(DATA_SECTION_SEPARATOR)) != 0))
        {
        error = tr("Database corrupted: no data section found.");
        return false;
    }

    return true;
}

QVariantHash GeoIPDatabase::readMetadata() const
{
    const char *ptr = reinterpret_cast<const char *>(m_data);
    quint32 size = m_size;
    if (m_size > MAX_METADATA_SIZE)
    {
        ptr += m_size - MAX_METADATA_SIZE;
        size = MAX_METADATA_SIZE;
    }

    const QByteArray data = QByteArray::fromRawData(ptr, size);
    int index = data.lastIndexOf(METADATA_BEGIN_MARK);
    if (index >= 0)
    {
        if (m_size > MAX_METADATA_SIZE)
            index += (m_size - MAX_METADATA_SIZE); // from begin of all data
        auto offset = static_cast<quint32>(index + METADATA_BEGIN_MARK.size());
        const QVariant metadata = readDataField(offset);
        if (metadata.userType() == QMetaType::QVariantHash)
            return metadata.toHash();
    }

    return {};
}

QVariant GeoIPDatabase::readDataField(quint32 &offset) const
{
    DataFieldDescriptor descr;
    if (!readDataFieldDescriptor(offset, descr))
        return {};

    quint32 locOffset = offset;
    bool usePointer = false;
    if (descr.fieldType == DataType::Pointer)
    {
        usePointer = true;
        // convert offset from data section to global
        locOffset = descr.offset + (m_nodeCount * m_recordSize / 4) + sizeof(DATA_SECTION_SEPARATOR);
        if (!readDataFieldDescriptor(locOffset, descr))
            return {};
    }

    QVariant fieldValue;
    switch (descr.fieldType)
    {
    case DataType::Pointer:
        qDebug() << "* Illegal Pointer using";
        break;
    case DataType::String:
        fieldValue = QString::fromUtf8(reinterpret_cast<const char *>(m_data + locOffset), descr.fieldSize);
        locOffset += descr.fieldSize;
        break;
    case DataType::Double:
        if (descr.fieldSize == 8)
            fieldValue = readPlainValue<double>(locOffset, descr.fieldSize);
        else
            qDebug() << "* Invalid field size for type: Double";
        break;
    case DataType::Bytes:
        fieldValue = QByteArray(reinterpret_cast<const char *>(m_data + locOffset), descr.fieldSize);
        locOffset += descr.fieldSize;
        break;
    case DataType::Integer16:
        fieldValue = readPlainValue<quint16>(locOffset, descr.fieldSize);
        break;
    case DataType::Integer32:
        fieldValue = readPlainValue<quint32>(locOffset, descr.fieldSize);
        break;
    case DataType::Map:
        fieldValue = readMapValue(locOffset, descr.fieldSize);
        break;
    case DataType::SignedInteger32:
        fieldValue = readPlainValue<qint32>(locOffset, descr.fieldSize);
        break;
    case DataType::Integer64:
        fieldValue = readPlainValue<quint64>(locOffset, descr.fieldSize);
        break;
    case DataType::Integer128:
        qDebug() << "* Unsupported data type: Integer128";
        break;
    case DataType::Array:
        fieldValue = readArrayValue(locOffset, descr.fieldSize);
        break;
    case DataType::DataCacheContainer:
        qDebug() << "* Unsupported data type: DataCacheContainer";
        break;
    case DataType::EndMarker:
        qDebug() << "* Unsupported data type: EndMarker";
        break;
    case DataType::Boolean:
        fieldValue = QVariant::fromValue(static_cast<bool>(descr.fieldSize));
        break;
    case DataType::Float:
        if (descr.fieldSize == 4)
            fieldValue = readPlainValue<float>(locOffset, descr.fieldSize);
        else
            qDebug() << "* Invalid field size for type: Float";
        break;
    default:
        qDebug() << "* Unsupported data type: Unknown";
    }

    if (!usePointer)
        offset = locOffset;
    return fieldValue;
}

bool GeoIPDatabase::readDataFieldDescriptor(quint32 &offset, DataFieldDescriptor &out) const
{
    const uchar *dataPtr = m_data + offset;
    const int availSize = m_size - offset;
    if (availSize < 1) return false;

    out.fieldType = static_cast<DataType>((dataPtr[0] & 0xE0) >> 5);
    if (out.fieldType == DataType::Pointer)
    {
        const int size = ((dataPtr[0] & 0x18) >> 3);
        if (availSize < (size + 2)) return false;

        if (size == 0)
            out.offset = ((dataPtr[0] & 0x07) << 8) + dataPtr[1];
        else if (size == 1)
            out.offset = ((dataPtr[0] & 0x07) << 16) + (dataPtr[1] << 8) + dataPtr[2] + 2048;
        else if (size == 2)
            out.offset = ((dataPtr[0] & 0x07) << 24) + (dataPtr[1] << 16) + (dataPtr[2] << 8) + dataPtr[3] + 526336;
        else if (size == 3)
            out.offset = (dataPtr[1] << 24) + (dataPtr[2] << 16) + (dataPtr[3] << 8) + dataPtr[4];

        offset += size + 2;
        return true;
    }

    out.fieldSize = dataPtr[0] & 0x1F;
    if (out.fieldSize <= 28)
    {
        if (out.fieldType == DataType::Unknown)
        {
            out.fieldType = static_cast<DataType>(dataPtr[1] + 7);
            if ((out.fieldType <= DataType::Map) || (out.fieldType > DataType::Float) || (availSize < 3))
                return false;
            offset += 2;
        }
        else
        {
            offset += 1;
        }
    }
    else if (out.fieldSize == 29)
    {
        if (availSize < 2) return false;
        out.fieldSize = dataPtr[1] + 29;
        offset += 2;
    }
    else if (out.fieldSize == 30)
    {
        if (availSize < 3) return false;
        out.fieldSize = (dataPtr[1] << 8) + dataPtr[2] + 285;
        offset += 3;
    }
    else if (out.fieldSize == 31)
    {
        if (availSize < 4) return false;
        out.fieldSize = (dataPtr[1] << 16) + (dataPtr[2] << 8) + dataPtr[3] + 65821;
        offset += 4;
    }

    return true;
}

void GeoIPDatabase::fromBigEndian([[maybe_unused]] uchar *buf, [[maybe_unused]] const quint32 len) const
{
#if (Q_BYTE_ORDER == Q_LITTLE_ENDIAN)
    std::reverse(buf, buf + len);
#endif
}

QVariant GeoIPDatabase::readMapValue(quint32 &offset, const quint32 count) const
{
    QVariantHash map;

    for (quint32 i = 0; i < count; ++i)
    {
        QVariant field = readDataField(offset);
        if (field.userType() != QMetaType::QString)
            return {};

        const QString key = field.toString();
        field = readDataField(offset);
        if (field.userType() == QMetaType::UnknownType)
            return {};

        map[key] = field;
    }

    return map;
}

QVariant GeoIPDatabase::readArrayValue(quint32 &offset, const quint32 count) const
{
    QVariantList array;

    for (quint32 i = 0; i < count; ++i)
    {
        const QVariant field = readDataField(offset);
        if (field.userType() == QMetaType::UnknownType)
            return {};

        array.append(field);
    }

    return array;
}

template <typename T>
QVariant GeoIPDatabase::readPlainValue(quint32 &offset, const quint8 len) const
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
