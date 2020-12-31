/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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

#include "filterparserthread.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <optional>
#include <string_view>
#include <utility>

#include <libtorrent/error_code.hpp>

#include <QDataStream>
#include <QFile>

#include "base/logger.h"

using namespace std::literals;

namespace
{
    struct IPFilterEntry
    {
        std::string_view range;
        int accessLevel = 0;
        // We don't care about this
        // std::string_view description;
    };

    IPFilterEntry parseDATLine(std::string_view line)
    {
        IPFilterEntry entry;

        // Each line should follow this format:
        // 001.009.096.105 - 001.009.096.105 , 000 , Some organization
        // The 2nd entry is access level and if >=127 the IP range isn't blocked.
        if (const auto firstComma = line.find(','); firstComma != line.npos)
        {
            entry.range = line.substr(0, firstComma);
            const auto pos  = std::find_if_not((line.data() + firstComma + 1), (line.data() + line.size()),
                                          [](unsigned char c) { return std::isspace(c); } );

            // from_chars will stop when it encounters non-numeric chars
            // we don't care for any other conversion failure
            std::from_chars(pos, (line.data() + line.size()), entry.accessLevel);
        }
        else
        {
            // Apparently only range is mandatory
            entry.range = line;
        }

        return entry;
    }

    IPFilterEntry parseP2PLine(std::string_view line)
    {
        IPFilterEntry entry;

        // Each line should follow this format:
        // Some organization:1.0.0.0-1.255.255.255
        // The "Some organization" part might contain a ':' char itself so we find the last occurrence
        // Apparently this format wasn't designed for IPv6, so no effort will be made
        // to parse possible IPv6 literals wrapped in []. These characters can appear in
        // the "some organization" part too. It isn't worth implementing such complex parsing
        // for a format that doesn't support IPv6 and there aren't any lists with IPv6 in them.
        if (const auto colon = line.rfind(':'); colon != line.npos)
            entry.range = line.substr(colon + 1);
        else
            entry.range = line;

        return entry;
    }

    std::pair<std::string_view, std::string_view> parseRange(std::string_view range)
    {
        std::string_view start;
        std::string_view end;

        if (const auto pos  = range.find('-'); pos != range.npos)
        {
            start = range.substr(0, pos);
            const auto pos_non_space  = std::find_if_not((range.data() + pos + 1), (range.data() + range.size()),
                                              [](unsigned char c) { return std::isspace(c); } );
            end = range.substr(pos_non_space - range.data());
        }

        return std::make_pair(start, end);
    }

    std::optional<lt::address> parseIPv4Address(std::string_view IPstr)
    {
        lt::address_v4::bytes_type octets = {};
        int octetIndex = 0;

        for (auto lastPos = IPstr.data();
             lastPos < (IPstr.data() + IPstr.size());
             ++octetIndex)
        {
            lt::address_v4::bytes_type::value_type octet = 0;

            const auto &[ptr, ec] = std::from_chars(lastPos, (IPstr.data() + IPstr.size()), octet);
            if (ec != std::errc())
                return {};

            octets[octetIndex] = octet;
            if (octetIndex == 3)
                return lt::address_v4(octets);

            lastPos = ptr + 1;
        }

        return {};
    }

    std::optional<lt::address> parseIPAddress(std::string_view IPstr)
    {
        lt::error_code ec;

        if (auto addr = parseIPv4Address(IPstr); addr)
            return addr;

        // Possibly an IPv6 address. Caller checks for validity
        if (auto addr = lt::make_address(IPstr, ec); !ec)
            return addr;

        return {};
    }

    constexpr int BUFFER_SIZE = 2 * 1024 * 1024; // 2 MiB
    constexpr int MAX_LOGGED_ERRORS = 5;
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

// Parser for eMule ip filter in DAT/P2P formats
template <FilterParserThread::Format format> int FilterParserThread::parseTxtFilterFile()
{
    int ruleCount = 0;
    QFile file(m_filePath);
    if (!file.exists()) return ruleCount;

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        LogMsg(tr("I/O Error: Could not open IP filter file in read mode."), Log::CRITICAL);
        return ruleCount;
    }

    std::vector<char> buffer(BUFFER_SIZE, '\0');
    std::string_view::size_type leftoverDataSize = 0;
    int nbLine = 0;
    int parseErrorCount = 0;
    const auto addLog = [&parseErrorCount](const QString &msg)
    {
        if (parseErrorCount <= MAX_LOGGED_ERRORS)
            LogMsg(msg, Log::CRITICAL);
    };

    while (true)
    {
        const qint64 bytesRead = file.read((buffer.data() + leftoverDataSize), (BUFFER_SIZE - leftoverDataSize));
        if (bytesRead < 0)
            break;
        const std::string_view::size_type dataSize = bytesRead + leftoverDataSize;
        if ((bytesRead == 0) && (dataSize == 0))
            break;
        std::string_view buffer_view(buffer.data(), dataSize);

        std::string_view::size_type start = 0;
        while (start < dataSize)
        {
            std::string_view line;
            // The file might have ended without the last line having a newline
            if (!((bytesRead == 0) && (dataSize > 0)))
            {
                if (const auto pos = buffer_view.find('\n', start);
                    pos != buffer_view.npos)
                {
                    line = buffer_view.substr(start, (pos + 1) - start);
                    start = pos + 1;
                }
            }
            else
            {
                line = buffer_view.substr(start);
                start = dataSize;
            }

            if (line.empty())
            {
                // read the next chunk from file
                // but first move(copy) the leftover data to the front of the buffer
                leftoverDataSize = dataSize - start;
                memmove(buffer.data(), (buffer.data() + start), leftoverDataSize);
                break;
            }

            ++nbLine;

            if (line == "\n"sv)
                continue;

            if ((line.find('#') == 0) || (line.find("//") == 0))
                continue;

            const auto &[range, accessLevel] = (format == Format::DAT) ? parseDATLine(line) : parseP2PLine(line);

            if constexpr (format == Format::DAT)
            {
                if (accessLevel >= 127)
                    continue;
            }

            const auto &[startIP, endIP] = parseRange(range);

            if (startIP.empty() || endIP.empty())
            {
                ++parseErrorCount;
                addLog(tr("IP filter line %1 is malformed.").arg(nbLine));
                continue;
            }

            const std::optional<lt::address> startAddr = parseIPAddress(startIP);
            if (!startAddr)
            {
                ++parseErrorCount;
                addLog(tr("IP filter line %1 is malformed. Start IP of the range is malformed.").arg(nbLine));
                continue;
            }

            const std::optional<lt::address> endAddr = parseIPAddress(endIP);
            if (!endAddr)
            {
                ++parseErrorCount;
                addLog(tr("IP filter line %1 is malformed. End IP of the range is malformed.").arg(nbLine));
                continue;
            }

            if ((startAddr->is_v4() != endAddr->is_v4())
                || (startAddr->is_v6() != endAddr->is_v6()))
            {
                ++parseErrorCount;
                addLog(tr("IP filter line %1 is malformed. One IP is IPv4 and the other is IPv6!").arg(nbLine));
                continue;
            }

            // Now Add to the filter
            try
            {
                m_filter.add_rule(*startAddr, *endAddr, lt::ip_filter::blocked);
                ++ruleCount;
            }
            catch (const std::exception &e)
            {
                ++parseErrorCount;
                addLog(tr("IP filter exception thrown for line %1. Exception is: %2")
                           .arg(nbLine).arg(QString::fromLocal8Bit(e.what())));
            }
        }

        if (start >= dataSize)
            leftoverDataSize = 0;
    }

    if (parseErrorCount > MAX_LOGGED_ERRORS)
        LogMsg(tr("%1 extra IP filter parsing errors occurred.", "513 extra IP filter parsing errors occurred.")
                   .arg(parseErrorCount - MAX_LOGGED_ERRORS), Log::CRITICAL);
    return ruleCount;
}

int FilterParserThread::getlineInStream(QDataStream &stream, std::string &name, const char delim)
{
    char c;
    int totalRead = 0;
    int read;
    do
    {
        read = stream.readRawData(&c, 1);
        totalRead += read;
        if (read > 0)
        {
            if (c != delim)
            {
                name += c;
            }
            else
            {
                // Delim found
                return totalRead;
            }
        }
    }
    while (read > 0);

    return totalRead;
}

// Parser for PeerGuardian ip filter in p2p format
int FilterParserThread::parseP2BFilterFile()
{
    int ruleCount = 0;
    QFile file(m_filePath);
    if (!file.exists()) return ruleCount;

    if (!file.open(QIODevice::ReadOnly))
    {
        LogMsg(tr("I/O Error: Could not open IP filter file in read mode."), Log::CRITICAL);
        return ruleCount;
    }

    QDataStream stream(&file);
    // Read header
    char buf[7];
    unsigned char version;
    if (!stream.readRawData(buf, sizeof(buf))
        || memcmp(buf, "\xFF\xFF\xFF\xFFP2B", 7)
        || !stream.readRawData(reinterpret_cast<char*>(&version), sizeof(version)))
        {
        LogMsg(tr("Parsing Error: The filter file is not a valid PeerGuardian P2B file."), Log::CRITICAL);
        return ruleCount;
    }

    if ((version == 1) || (version == 2))
    {
        qDebug ("p2b version 1 or 2");
        unsigned int start, end;

        std::string name;
        while (getlineInStream(stream, name, '\0') && !m_abort)
        {
            if (!stream.readRawData(reinterpret_cast<char*>(&start), sizeof(start))
                || !stream.readRawData(reinterpret_cast<char*>(&end), sizeof(end)))
                {
                LogMsg(tr("Parsing Error: The filter file is not a valid PeerGuardian P2B file."), Log::CRITICAL);
                return ruleCount;
            }

            // Network byte order to Host byte order
            // asio address_v4 constructor expects it
            // that way
            const lt::address_v4 first(ntohl(start));
            const lt::address_v4 last(ntohl(end));
            // Apply to bittorrent session
            try
            {
                m_filter.add_rule(first, last, lt::ip_filter::blocked);
                ++ruleCount;
            }
            catch (const std::exception &) {}
        }
    }
    else if (version == 3)
    {
        qDebug ("p2b version 3");
        unsigned int namecount;
        if (!stream.readRawData(reinterpret_cast<char*>(&namecount), sizeof(namecount)))
        {
            LogMsg(tr("Parsing Error: The filter file is not a valid PeerGuardian P2B file."), Log::CRITICAL);
            return ruleCount;
        }

        namecount = ntohl(namecount);
        // Reading names although, we don't really care about them
        for (unsigned int i = 0; i < namecount; ++i)
        {
            std::string name;
            if (!getlineInStream(stream, name, '\0'))
            {
                LogMsg(tr("Parsing Error: The filter file is not a valid PeerGuardian P2B file."), Log::CRITICAL);
                return ruleCount;
            }

            if (m_abort) return ruleCount;
        }

        // Reading the ranges
        unsigned int rangecount;
        if (!stream.readRawData(reinterpret_cast<char*>(&rangecount), sizeof(rangecount)))
        {
            LogMsg(tr("Parsing Error: The filter file is not a valid PeerGuardian P2B file."), Log::CRITICAL);
            return ruleCount;
        }

        rangecount = ntohl(rangecount);
        unsigned int name, start, end;
        for (unsigned int i = 0; i < rangecount; ++i)
        {
            if (!stream.readRawData(reinterpret_cast<char*>(&name), sizeof(name))
                || !stream.readRawData(reinterpret_cast<char*>(&start), sizeof(start))
                || !stream.readRawData(reinterpret_cast<char*>(&end), sizeof(end)))
                {
                LogMsg(tr("Parsing Error: The filter file is not a valid PeerGuardian P2B file."), Log::CRITICAL);
                return ruleCount;
            }

            // Network byte order to Host byte order
            // asio address_v4 constructor expects it
            // that way
            const lt::address_v4 first(ntohl(start));
            const lt::address_v4 last(ntohl(end));
            // Apply to bittorrent session
            try
            {
                m_filter.add_rule(first, last, lt::ip_filter::blocked);
                ++ruleCount;
            }
            catch (const std::exception &) {}

            if (m_abort) return ruleCount;
        }
    }
    else
    {
        LogMsg(tr("Parsing Error: The filter file is not a valid PeerGuardian P2B file."), Log::CRITICAL);
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
    if (isRunning())
    {
        // Already parsing a filter, m_abort first
        m_abort = true;
        wait();
    }

    m_abort = false;
    m_filePath = filePath;
    m_filter = lt::ip_filter();
    // Run it
    start();
}

lt::ip_filter FilterParserThread::IPfilter()
{
    return m_filter;
}

void FilterParserThread::run()
{
    qDebug("Processing filter file");
    int ruleCount = 0;
    if (m_filePath.endsWith(".p2p", Qt::CaseInsensitive))
    {
        // PeerGuardian p2p file
        ruleCount = parseTxtFilterFile<Format::P2P>();
    }
    else if (m_filePath.endsWith(".p2b", Qt::CaseInsensitive))
    {
        // PeerGuardian p2b file
        ruleCount = parseP2BFilterFile();
    }
    else if (m_filePath.endsWith(".dat", Qt::CaseInsensitive))
    {
        // eMule DAT format
        ruleCount = parseTxtFilterFile<Format::DAT>();
    }

    if (m_abort) return;

    try
    {
        emit IPFilterParsed(ruleCount);
    }
    catch (const std::exception &)
    {
        emit IPFilterError();
    }

    qDebug("IP Filter thread: finished parsing, filter applied");
}
