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

#include <QFile>
#include <QHostAddress>
#include <QDataStream>
#include <QStringList>

#include <libtorrent/session.hpp>
#include <libtorrent/ip_filter.hpp>

#include "core/logger.h"
#include "filterparserthread.h"

namespace libt = libtorrent;

FilterParserThread::FilterParserThread(libt::session *s, QObject *parent)
    : QThread(parent)
    , m_session(s)
    , m_abort(false)
{
}

FilterParserThread::~FilterParserThread()
{
    m_abort = true;
    wait();
}

// Parser for eMule ip filter in DAT format
int FilterParserThread::parseDATFilterFile(QString m_filePath, libt::ip_filter &filter)
{
    int ruleCount = 0;
    QFile file(m_filePath);
    if (!file.exists()) return ruleCount;

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Logger::instance()->addMessage(tr("I/O Error: Could not open ip filer file in read mode."), Log::CRITICAL);
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
        const uint nbElem = partsList.size();

        // IP Range should be split by a dash
        QList<QByteArray> IPs = partsList.first().split('-');
        if (IPs.size() != 2) {
            qDebug("Ipfilter.dat: line %d is malformed.", nbLine);
            qDebug("Line was %s", line.constData());
            continue;
        }

        boost::system::error_code ec;
        const QString strStartIP = cleanupIPAddress(IPs.at(0));
        if (strStartIP.isEmpty()) {
            qDebug("Ipfilter.dat: line %d is malformed.", nbLine);
            qDebug("Start IP of the range is malformated: %s", qPrintable(strStartIP));
            continue;
        }
        libt::address startAddr = libt::address::from_string(qPrintable(strStartIP), ec);
        if (ec) {
            qDebug("Ipfilter.dat: line %d is malformed.", nbLine);
            qDebug("Start IP of the range is malformated: %s", qPrintable(strStartIP));
            continue;
        }

        const QString strEndIP = cleanupIPAddress(IPs.at(1));
        if (strEndIP.isEmpty()) {
            qDebug("Ipfilter.dat: line %d is malformed.", nbLine);
            qDebug("End IP of the range is malformated: %s", qPrintable(strEndIP));
            continue;
        }

        libt::address endAddr = libt::address::from_string(qPrintable(strEndIP), ec);
        if (ec) {
            qDebug("Ipfilter.dat: line %d is malformed.", nbLine);
            qDebug("End IP of the range is malformated: %s", qPrintable(strEndIP));
            continue;
        }

        if (startAddr.is_v4() != endAddr.is_v4()) {
            qDebug("Ipfilter.dat: line %d is malformed.", nbLine);
            qDebug("One IP is IPv4 and the other is IPv6!");
            continue;
        }

        // Check if there is an access value (apparently not mandatory)
        int nbAccess = 0;
        if (nbElem > 1) {
            // There is possibly one
            nbAccess = partsList.at(1).trimmed().toInt();
        }

        if (nbAccess > 127) {
            // Ignoring this rule because access value is too high
            continue;
        }

        // Now Add to the filter
        try {
            filter.add_rule(startAddr, endAddr, libt::ip_filter::blocked);
            ++ruleCount;
        }
        catch(std::exception &) {
            qDebug("Bad line in filter file, avoided crash...");
        }
    }

    file.close();
    return ruleCount;
}

// Parser for PeerGuardian ip filter in p2p format
int FilterParserThread::parseP2PFilterFile(QString m_filePath, libt::ip_filter &filter)
{
    int ruleCount = 0;
    QFile file(m_filePath);
    if (!file.exists()) return ruleCount;

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Logger::instance()->addMessage(tr("I/O Error: Could not open ip filer file in read mode."), Log::CRITICAL);
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
            qDebug("p2p file: line %d is malformed.", nbLine);
            continue;
        }

        // Get IP range
        QList<QByteArray> IPs = partsList.last().split('-');
        if (IPs.size() != 2) {
            qDebug("p2p file: line %d is malformed.", nbLine);
            qDebug("line was: %s", line.constData());
            continue;
        }

        boost::system::error_code ec;
        QString strStartIP = cleanupIPAddress(IPs.at(0));
        if (strStartIP.isEmpty()) {
            qDebug("p2p file: line %d is malformed.", nbLine);
            qDebug("Start IP is invalid: %s", qPrintable(strStartIP));
            continue;
        }

        libt::address startAddr = libt::address::from_string(qPrintable(strStartIP), ec);
        if (ec) {
            qDebug("p2p file: line %d is malformed.", nbLine);
            qDebug("Start IP is invalid: %s", qPrintable(strStartIP));
            continue;
        }

        QString strEndIP = cleanupIPAddress(IPs.at(1));
        if (strEndIP.isEmpty()) {
            qDebug("p2p file: line %d is malformed.", nbLine);
            qDebug("End IP is invalid: %s", qPrintable(strStartIP));
            continue;
        }

        libt::address endAddr = libt::address::from_string(qPrintable(strEndIP), ec);
        if (ec) {
            qDebug("p2p file: line %d is malformed.", nbLine);
            qDebug("End IP is invalid: %s", qPrintable(strStartIP));
            continue;
        }

        if (startAddr.is_v4() != endAddr.is_v4()) {
            qDebug("p2p file: line %d is malformed.", nbLine);
            qDebug("Line was: %s", line.constData());
            continue;
        }

        try {
            filter.add_rule(startAddr, endAddr, libt::ip_filter::blocked);
            ++ruleCount;
        }
        catch(std::exception &) {
            qDebug("p2p file: line %d is malformed.", nbLine);
            qDebug("Line was: %s", line.constData());
            continue;
        }
    }

    file.close();
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
int FilterParserThread::parseP2BFilterFile(QString m_filePath, libt::ip_filter &filter)
{
    int ruleCount = 0;
    QFile file(m_filePath);
    if (!file.exists()) return ruleCount;

    if (!file.open(QIODevice::ReadOnly)) {
        Logger::instance()->addMessage(tr("I/O Error: Could not open ip filer file in read mode."), Log::CRITICAL);
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
                filter.add_rule(first, last, libt::ip_filter::blocked);
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
                filter.add_rule(first, last, libt::ip_filter::blocked);
                ++ruleCount;
            }
            catch(std::exception &) {}

            if (m_abort) return ruleCount;
        }
    }
    else {
        Logger::instance()->addMessage(tr("Parsing Error: The filter file is not a valid PeerGuardian P2B file."), Log::CRITICAL);
    }

    file.close();
    return ruleCount;
}

// Process ip filter file
// Supported formats:
//  * eMule IP list (DAT): http://wiki.phoenixlabs.org/wiki/DAT_Format
//  * PeerGuardian Text (P2P): http://wiki.phoenixlabs.org/wiki/P2P_Format
//  * PeerGuardian Binary (P2B): http://wiki.phoenixlabs.org/wiki/P2B_Format
void FilterParserThread::processFilterFile(QString _filePath)
{
    if (isRunning()) {
        // Already parsing a filter, m_abort first
        m_abort = true;
        wait();
    }

    m_abort = false;
    m_filePath = _filePath;
    // Run it
    start();
}

void FilterParserThread::processFilterList(libt::session *s, const QStringList &IPs)
{
    // First, import current filter
    libt::ip_filter filter = s->get_ip_filter();
    foreach (const QString &ip, IPs) {
        qDebug("Manual ban of peer %s", ip.toLocal8Bit().constData());
        boost::system::error_code ec;
        libt::address addr = libt::address::from_string(ip.toLocal8Bit().constData(), ec);
        Q_ASSERT(!ec);
        if (!ec)
            filter.add_rule(addr, addr, libt::ip_filter::blocked);
    }

    s->set_ip_filter(filter);
}

QString FilterParserThread::cleanupIPAddress(QString _ip)
{
    QHostAddress ip(_ip.trimmed());
    if (ip.isNull()) return QString();

    return ip.toString();
}

void FilterParserThread::run()
{
    qDebug("Processing filter file");
    libt::ip_filter filter = m_session->get_ip_filter();
    int ruleCount = 0;
    if (m_filePath.endsWith(".p2p", Qt::CaseInsensitive)) {
        // PeerGuardian p2p file
        ruleCount = parseP2PFilterFile(m_filePath, filter);
    }
    else if (m_filePath.endsWith(".p2b", Qt::CaseInsensitive)) {
        // PeerGuardian p2b file
        ruleCount = parseP2BFilterFile(m_filePath, filter);
    }
    else if (m_filePath.endsWith(".dat", Qt::CaseInsensitive)) {
        // eMule DAT format
        ruleCount = parseDATFilterFile(m_filePath, filter);
    }

    if (m_abort) return;

    try {
        m_session->set_ip_filter(filter);
        emit IPFilterParsed(ruleCount);
    }
    catch(std::exception &) {
        emit IPFilterError();
    }

    qDebug("IP Filter thread: finished parsing, filter applied");
}
