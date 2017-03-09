/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017  Michael Ziminsky
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
#include "rssdownloadrule_p.h"

#include <QRegularExpression>

#include "base/utils/string.h"

using namespace Rss;

void DownloadRuleData::setMustContain(const QString &tokens)
{
    if (m_useRegex)
        m_mustContain = QStringList() << tokens;
    else
        m_mustContain = tokens.split("|");

    // Check for single empty string - if so, no condition
    if ((m_mustContain.size() == 1) && m_mustContain[0].isEmpty())
        m_mustContain.clear();

    m_cachedRegexes.clear();
}

void DownloadRuleData::setMustNotContain(const QString &tokens)
{
    if (m_useRegex)
        m_mustNotContain = QStringList() << tokens;
    else
        m_mustNotContain = tokens.split("|");

    // Check for single empty string - if so, no condition
    if ((m_mustNotContain.size() == 1) && m_mustNotContain[0].isEmpty())
        m_mustNotContain.clear();

    m_cachedRegexes.clear();
}

void DownloadRuleData::setEpisodeFilter(const QString &episodeFilter)
{
    if (episodeFilter == m_episodeFilter)
        return;

    m_episodeFilter = episodeFilter;
    m_cachedRegexes.clear();
}

QRegularExpression DownloadRuleData::cachedRegex(const QString &expression, bool isRegex) const
{
    // Use a cache of regexes so we don't have to continually recompile - big performance increase.
    // The cache is cleared whenever the regex/wildcard, must or must not contain fields or
    // episode filter are modified.
    Q_ASSERT(!expression.isEmpty());
    QRegularExpression regex(m_cachedRegexes[expression]);

    if (!regex.pattern().isEmpty())
        return regex;

    return m_cachedRegexes[expression] = QRegularExpression(isRegex ? expression : Utils::String::wildcardToRegex(expression), QRegularExpression::CaseInsensitiveOption);
}

void DownloadRuleData::setUseRegex(bool useRegex)
{
    if (useRegex == m_useRegex)
        return;

    m_useRegex = useRegex;
    m_cachedRegexes.clear();
}

DownloadRuleData::~DownloadRuleData() {}
