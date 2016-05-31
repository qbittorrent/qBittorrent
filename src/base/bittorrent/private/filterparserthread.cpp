/*
 * Bittorrent Client using Qt and libt.
 * Copyright (C) 2006  Christophe Dumez
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
 * Contact : chris@qbittorrent.org
 */

#include "filterparserthread.h"

#include <QDataStream>
#include <QFile>
#include <QStringList>

#include "base/logger.h"

namespace libt = libtorrent;

namespace
{
    class DATIPv4Parser {
    public:
        const char* tryParse(const char* str, bool& ok) {
            unsigned char number = 0;

            const char* numberStart = str;
            char* endptr;
            for (; *str; ++str) {
                if (*str == '.') {
                    m_buf[number++] = static_cast<unsigned char>(strtol(numberStart, &endptr, 10));
                    if (endptr != str) {
                        break;
                    }
                    if (number == 4) { // an IP might end with '.': 192.168.1.2.
                        ok = true;
                        return str + 1;
                    }
                    numberStart = str + 1;
                }
            }
#if 1
            // The following is needed for parsing of a string with IP, but in the eMule files there is always a space after an IP,
            // and this case is handled above
            if (str != numberStart) {
                m_buf[number] = static_cast<unsigned char>(strtol(numberStart, &endptr, 10));
                if (endptr == str && number == 3) {
                    ok = true;
                    return str + 1;
                }
            }
#endif

            ok = false;
            return str + 1;
        }

        libt::address_v4::bytes_type parsed() const {
            return m_buf;
        }

    private:
        libt::address_v4::bytes_type m_buf;
    };

    bool parseIPAddress(const QByteArray &_ip, libt::address &address)
    {
        DATIPv4Parser parser;
        boost::system::error_code ec;
        bool ok = false;

        parser.tryParse(_ip.constData(), ok);
        if (ok)
            address = libt::address_v4(parser.parsed());
        else
            address = libt::address::from_string(_ip.constData(), ec);

        return !ec;
    }
}

FilterParserThread::FilterParserThread(QObject *parent)
    : QThread(parent)
    , m_abort(false)
{
}

FilterParserThread::~FilterParserThread()
{
    m_abort = true;
    wait();
}

// Parser for eMule ip filter in DAT format
int FilterParserThread::parseDATFilterFile()
{
    int ruleCount = 0;
    QFile file(m_filePath);
    if (!file.exists()) return ruleCount;

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Logger::instance()->addMessage(tr("I/O Error: Could not open ip filter file in read mode."), Log::CRITICAL);
        return ruleCount;
    }

    const QByteArray data = file.readAll();
    int start = 0;
    int endOfLine = -1;

    while (true) {
        start = endOfLine + 1;
        if (start >= data.size())
            break;
        if (data[start] == '#' || (data[start] == '/' && data[start + 1] == '/'))
            continue;

        endOfLine = data.indexOf('\n', start);
        if (endOfLine == -1) break;

        QVector<int> delimIndices = indicesOfDelimiters(data, ',', start, endOfLine);

        // Check if there is at least one item (ip range)
        if (delimIndices.isEmpty())
            continue;

        // Check if there is an access value (apparently not mandatory)
        if (delimIndices.size()) {
            // There is possibly one
            const QByteArray accesscode(data.constData() + delimIndices[0] + 1, (delimIndices[1] - 1) - (delimIndices[0] + 1) + 1);
            const int nbAccess = accesscode.toInt();
            // Ignoring this rule because access value is too high
            if (nbAccess > 127)
                continue;
        }

        // IP Range should be split by a dash
        QVector<int> delimIP = indicesOfDelimiters(data, '-', start, delimIndices[0] - 1);
        if (delimIP.size() != 1) {
            //Logger::instance()->addMessage(tr("IP filter line %1 is malformed. Line is: %2").arg(nbLine).arg(QString(line)), Log::CRITICAL);
            continue;
        }

        libt::address startAddr;
        if (!parseIPAddress(trim(data, start, delimIP[0] - 1), startAddr)) {
            //Logger::instance()->addMessage(tr("IP filter line %1 is malformed. Start IP of the range is malformed: %2").arg(nbLine).arg(QString(IPs.at(0))), Log::CRITICAL);
            continue;
        }

        libt::address endAddr;
        if (!parseIPAddress(trim(data, delimIP[0] + 1, delimIndices[0] - 1), endAddr)) {
            //Logger::instance()->addMessage(tr("IP filter line %1 is malformed. End IP of the range is malformed: %2").arg(nbLine).arg(QString(IPs.at(1))), Log::CRITICAL);
            continue;
        }

        if (startAddr.is_v4() != endAddr.is_v4()
            || startAddr.is_v6() != endAddr.is_v6()) {
            //Logger::instance()->addMessage(tr("IP filter line %1 is malformed. One IP is IPv4 and the other is IPv6!").arg(nbLine), Log::CRITICAL);
            continue;
        }

        // Now Add to the filter
        try {
            m_filter.add_rule(startAddr, endAddr, libt::ip_filter::blocked);
            ++ruleCount;
        }
        catch(std::exception &) {
            //Logger::instance()->addMessage(tr("IP filter exception thrown for line %1. Line is: %2").arg(nbLine).arg(QString(line)), Log::CRITICAL);
        }
    }

    return ruleCount;
}

// Parser for PeerGuardian ip filter in p2p format
int FilterParserThread::parseP2PFilterFile()
{
    int ruleCount = 0;
    QFile file(m_filePath);
    if (!file.exists()) return ruleCount;

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Logger::instance()->addMessage(tr("I/O Error: Could not open ip filter file in read mode."), Log::CRITICAL);
        return ruleCount;
    }

    unsigned int nbLine = 0;
    while (!file.atEnd() && !m_abort) {
        ++nbLine;
        QByteArray line = file.readLine().trimmed();
        if (line.isEmpty()) continue;
        // Ignoring commented lines
        if (line.startsWith('#') || line.startsWith("//")) continue;

        // Line is split by :
        QList<QByteArray> partsList = line.split(':');
        if (partsList.size() < 2) {
            Logger::instance()->addMessage(tr("IP filter line %1 is malformed. Line is: %2").arg(nbLine).arg(QString(line)), Log::CRITICAL);
            continue;
        }

        // Get IP range
        QList<QByteArray> IPs = partsList.last().split('-');
        if (IPs.size() != 2) {
            Logger::instance()->addMessage(tr("IP filter line %1 is malformed. Line is: %2").arg(nbLine).arg(QString(line)), Log::CRITICAL);
            continue;
        }

        libt::address startAddr;
        if (!parseIPAddress(IPs.at(0), startAddr)) {
            Logger::instance()->addMessage(tr("IP filter line %1 is malformed. Start IP of the range is malformed: %2").arg(nbLine).arg(QString(IPs.at(0))), Log::CRITICAL);
            continue;
        }

        libt::address endAddr;
        if (!parseIPAddress(IPs.at(1), endAddr)) {
            Logger::instance()->addMessage(tr("IP filter line %1 is malformed. End IP of the range is malformed: %2").arg(nbLine).arg(QString(IPs.at(1))), Log::CRITICAL);
            continue;
        }

        if (startAddr.is_v4() != endAddr.is_v4()
            || startAddr.is_v6() != endAddr.is_v6()) {
            Logger::instance()->addMessage(tr("IP filter line %1 is malformed. One IP is IPv4 and the other is IPv6!").arg(nbLine), Log::CRITICAL);
            continue;
        }

        try {
            m_filter.add_rule(startAddr, endAddr, libt::ip_filter::blocked);
            ++ruleCount;
        }
        catch(std::exception &) {
            Logger::instance()->addMessage(tr("IP filter exception thrown for line %1. Line is: %2").arg(nbLine).arg(QString(line)), Log::CRITICAL);
        }
    }

    return ruleCount;
}

int FilterParserThread::getlineInStream(QDataStream &stream, std::string &name, char delim)
{
    char c;
    int total_read = 0;
    int read;
    do {
        read = stream.readRawData(&c, 1);
        total_read += read;
        if (read > 0) {
            if (c != delim) {
                name += c;
            }
            else {
                // Delim found
                return total_read;
            }
        }
    }
    while(read > 0);

    return total_read;
}

// Parser for PeerGuardian ip filter in p2p format
int FilterParserThread::parseP2BFilterFile()
{
    int ruleCount = 0;
    QFile file(m_filePath);
    if (!file.exists()) return ruleCount;

    if (!file.open(QIODevice::ReadOnly)) {
        Logger::instance()->addMessage(tr("I/O Error: Could not open ip filter file in read mode."), Log::CRITICAL);
        return ruleCount;
    }

    QDataStream stream(&file);
    // Read header
    char buf[7];
    unsigned char version;
    if (!stream.readRawData(buf, sizeof(buf))
        || memcmp(buf, "\xFF\xFF\xFF\xFFP2B", 7)
        || !stream.readRawData((char*)&version, sizeof(version))) {
        Logger::instance()->addMessage(tr("Parsing Error: The filter file is not a valid PeerGuardian P2B file."), Log::CRITICAL);
        return ruleCount;
    }

    if ((version == 1) || (version == 2)) {
        qDebug ("p2b version 1 or 2");
        unsigned int start, end;

        std::string name;
        while(getlineInStream(stream, name, '\0') && !m_abort) {
            if (!stream.readRawData((char*)&start, sizeof(start))
                || !stream.readRawData((char*)&end, sizeof(end))) {
                Logger::instance()->addMessage(tr("Parsing Error: The filter file is not a valid PeerGuardian P2B file."), Log::CRITICAL);
                return ruleCount;
            }

            // Network byte order to Host byte order
            // asio address_v4 constructor expects it
            // that way
            libt::address_v4 first(ntohl(start));
            libt::address_v4 last(ntohl(end));
            // Apply to bittorrent session
            try {
                m_filter.add_rule(first, last, libt::ip_filter::blocked);
                ++ruleCount;
            }
            catch(std::exception &) {}
        }
    }
    else if (version == 3) {
        qDebug ("p2b version 3");
        unsigned int namecount;
        if (!stream.readRawData((char*)&namecount, sizeof(namecount))) {
            Logger::instance()->addMessage(tr("Parsing Error: The filter file is not a valid PeerGuardian P2B file."), Log::CRITICAL);
            return ruleCount;
        }

        namecount = ntohl(namecount);
        // Reading names although, we don't really care about them
        for (unsigned int i = 0; i < namecount; ++i) {
            std::string name;
            if (!getlineInStream(stream, name, '\0')) {
                Logger::instance()->addMessage(tr("Parsing Error: The filter file is not a valid PeerGuardian P2B file."), Log::CRITICAL);
                return ruleCount;
            }

            if (m_abort) return ruleCount;
        }

        // Reading the ranges
        unsigned int rangecount;
        if (!stream.readRawData((char*)&rangecount, sizeof(rangecount))) {
            Logger::instance()->addMessage(tr("Parsing Error: The filter file is not a valid PeerGuardian P2B file."), Log::CRITICAL);
            return ruleCount;
        }

        rangecount = ntohl(rangecount);
        unsigned int name, start, end;
        for (unsigned int i = 0; i < rangecount; ++i) {
            if (!stream.readRawData((char*)&name, sizeof(name))
                || !stream.readRawData((char*)&start, sizeof(start))
                || !stream.readRawData((char*)&end, sizeof(end))) {
                Logger::instance()->addMessage(tr("Parsing Error: The filter file is not a valid PeerGuardian P2B file."), Log::CRITICAL);
                return ruleCount;
            }

            // Network byte order to Host byte order
            // asio address_v4 constructor expects it
            // that way
            libt::address_v4 first(ntohl(start));
            libt::address_v4 last(ntohl(end));
            // Apply to bittorrent session
            try {
                m_filter.add_rule(first, last, libt::ip_filter::blocked);
                ++ruleCount;
            }
            catch(std::exception &) {}

            if (m_abort) return ruleCount;
        }
    }
    else {
        Logger::instance()->addMessage(tr("Parsing Error: The filter file is not a valid PeerGuardian P2B file."), Log::CRITICAL);
    }

    return ruleCount;
}

// Process ip filter file
// Supported formats:
//  * eMule IP list (DAT): http://wiki.phoenixlabs.org/wiki/DAT_Format
//  * PeerGuardian Text (P2P): http://wiki.phoenixlabs.org/wiki/P2P_Format
//  * PeerGuardian Binary (P2B): http://wiki.phoenixlabs.org/wiki/P2B_Format
void FilterParserThread::processFilterFile(const QString &filePath)
{
    if (isRunning()) {
        // Already parsing a filter, m_abort first
        m_abort = true;
        wait();
    }

    m_abort = false;
    m_filePath = filePath;
    m_filter = libt::ip_filter();
    // Run it
    start();
}

libt::ip_filter FilterParserThread::IPfilter()
{
    return m_filter;
}

void FilterParserThread::run()
{
    qDebug("Processing filter file");
    int ruleCount = 0;
    if (m_filePath.endsWith(".p2p", Qt::CaseInsensitive)) {
        // PeerGuardian p2p file
        ruleCount = parseP2PFilterFile();
    }
    else if (m_filePath.endsWith(".p2b", Qt::CaseInsensitive)) {
        // PeerGuardian p2b file
        ruleCount = parseP2BFilterFile();
    }
    else if (m_filePath.endsWith(".dat", Qt::CaseInsensitive)) {
        // eMule DAT format
        ruleCount = parseDATFilterFile();
    }

    if (m_abort) return;

    try {
        emit IPFilterParsed(ruleCount);
    }
    catch(std::exception &) {
        emit IPFilterError();
    }

    qDebug("IP Filter thread: finished parsing, filter applied");
}

QVector<int> FilterParserThread::indicesOfDelimiters(const QByteArray &data, const char delimiter, const int start, const int end)
{
    if (start >= end) return QVector<int>();

    QVector<int> indices;
    int index = start;
    while (true) {
        index = data.indexOf(delimiter, index);
        if (index == -1 || index >= end)
            break;
        indices.append(index);
        ++index;
    }

    return indices;
}

QByteArray FilterParserThread::trim(const QByteArray &data, const int start, const int end)
{
    if (start >= end) return QByteArray();

    int first = start;
    int last = end;

    for (int i = start; i <= end; ++i) {
        if (data[i] != ' ') {
            first = i;
            break;
        }
    }

    for (int i = end; i >= start; --i) {
        if (data[i] != ' ') {
            last = i;
            break;
        }
    }

    if (first >= last) return QByteArray();

    return QByteArray(data.constData() + first, last - first + 1);
}
