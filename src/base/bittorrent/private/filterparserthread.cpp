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
    bool parseIPAddress(QString _ip, libt::address &address)
    {
        _ip = _ip.trimmed();

        // Emule .DAT files contain leading zeroes in IPv4 addresses
        // eg 001.009.106.186
        // We need to remove them because both QHostAddress and Boost.Asio fail to parse them.
        QStringList octets = _ip.split('.', QString::SkipEmptyParts);
        if (octets.size() == 4) {
            QString octet; // it is faster to not recreate this object in the loop
            for (int i = 0; i < 4; i++) {
                octet = octets[i];
                if ((octet[0] == QChar('0')) && (octet.count() > 1)) {
                    if ((octet[1] == QChar('0')) && (octet.count() > 2))
                        octet.remove(0, 2);
                    else
                        octet.remove(0, 1);

                    octets[i] = octet;
                }
            }

            _ip = octets.join(".");
        }

        boost::system::error_code ec;
        address = libt::address::from_string(_ip.toLatin1().constData(), ec);

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

    unsigned int nbLine = 0;
    while (!file.atEnd() && !m_abort) {
        ++nbLine;
        QByteArray line = file.readLine();
        // Ignoring empty lines
        line = line.trimmed();
        if (line.isEmpty()) continue;
        // Ignoring commented lines
        if (line.startsWith('#') || line.startsWith("//")) continue;

        // Line should be split by commas
        QList<QByteArray> partsList = line.split(',');

        // Check if there is at least one item (ip range)
        if (partsList.isEmpty())
            continue;

        // Check if there is an access value (apparently not mandatory)
        if (partsList.size() > 1) {
            // There is possibly one
            const int nbAccess = partsList.at(1).trimmed().toInt();
            // Ignoring this rule because access value is too high
            if (nbAccess > 127)
                continue;
        }

        // IP Range should be split by a dash
        QList<QByteArray> IPs = partsList.first().split('-');
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

        // Now Add to the filter
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
