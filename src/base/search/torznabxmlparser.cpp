/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2021  Vladimir Golovnev <glassez@yandex.ru>
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

#include "torznabxmlparser.h"

#include <QXmlStreamReader>

#include "base/utils/string.h"

const int torznabXMLParsingResultTypeId = qRegisterMetaType<TorznabRSSParsingResult>();

void TorznabXMLParser::parse(const QString &indexerName, const QByteArray &feedData)
{
    QXmlStreamReader xml {feedData};
    m_indexerName = indexerName;
    m_result = {};
    m_result.items.reserve(1024); // just to avoid multiple memory reallocations

    if (xml.readNextStartElement())
    {
        bool foundChannel = false;

        if (xml.name() == QLatin1String("rss"))
        {
            // Find channels
            while (xml.readNextStartElement())
            {
                if (xml.name() == QLatin1String("channel"))
                {
                    parseRSSChannel(xml);
                    foundChannel = true;
                    break;
                }

                xml.skipCurrentElement();
            }
        }
        else if (xml.name() == QLatin1String("error"))
        {
            m_result.error = xml.attributes().value(QLatin1String("description")).toString();
        }

        if (!foundChannel)
            xml.raiseError(tr("Invalid Torznab XML."));
    }

    if (m_result.error.isEmpty() && xml.hasError())
    {
        m_result.error = tr("%1 (line: %2, column: %3, offset: %4).")
                .arg(xml.errorString()).arg(xml.lineNumber())
                .arg(xml.columnNumber()).arg(xml.characterOffset());
    }

    emit finished(m_indexerName, m_result);
}

void TorznabXMLParser::parseRSSChannel(QXmlStreamReader &xml)
{
    while (xml.readNextStartElement())
    {
        if (xml.name() == QLatin1String("item"))
            parseRSSItem(xml);

        if (!xml.isEndElement())
            xml.skipCurrentElement();
    }
}

void TorznabXMLParser::parseRSSItem(QXmlStreamReader &xml)
{
    using namespace Utils::String;

    SearchResult searchResult;

    while (xml.readNextStartElement())
    {
        const auto name = xml.name();

        if (name == QLatin1String("title"))
        {
            searchResult.fileName = xml.readElementText().trimmed();
        }
        else if (name == QLatin1String("guid"))
        {
            if (searchResult.descrLink.isEmpty())
                searchResult.descrLink = xml.readElementText();
        }
        else if (name == QLatin1String("comments"))
        {
            searchResult.descrLink = xml.readElementText();
        }
        else if (name == QLatin1String("size"))
        {
            searchResult.fileSize = parseLongLong(xml.readElementText().trimmed()).value_or(-1);
        }
        else if (name == QLatin1String("link"))
        {
            if (searchResult.fileUrl.isEmpty())
                searchResult.fileUrl = xml.readElementText();
        }
        else if (name == QLatin1String("attr"))
        {
            const auto attrName = xml.attributes().value(QLatin1String("name"));

            if (attrName == QLatin1String("peers"))
            {
                searchResult.numLeechers = parseLongLong(xml.attributes().value(QLatin1String("value")).toString()).value_or(-1);
            }
            else if (attrName == QLatin1String("seeders"))
            {
                searchResult.numSeeders = parseLongLong(xml.attributes().value(QLatin1String("value")).toString()).value_or(-1);
            }
            else if (attrName == QLatin1String("magneturl"))
            {
                searchResult.fileUrl = xml.attributes().value(QLatin1String("value")).toString();
            }
        }

        if (!xml.isEndElement())
            xml.skipCurrentElement();
    }

    if ((searchResult.numLeechers != -1) && (searchResult.numSeeders != -1))
        searchResult.numLeechers = searchResult.numLeechers - searchResult.numSeeders;

    if (!searchResult.fileName.isEmpty() && !searchResult.fileUrl.isEmpty())
    {
        searchResult.indexerName = m_indexerName;
        m_result.items.append(searchResult);
    }
}
