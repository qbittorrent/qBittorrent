/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  c0re100 <corehusky@gmail.com>
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

#include <algorithm>
#include <fstream>
#include <string>

#include <QFileInfo>
#include <QRegularExpression>
#include <QString>
#include <QVector>

#include <libtorrent/peer_info.hpp>

#include "base/logger.h"

namespace {

bool qregex_has_match(const QRegularExpression& re, const QString& str)
{
  auto m = re.match(str);
  return m.hasMatch();
}

}

class peer_filter
{
public:
  explicit peer_filter(const QString& filter_file)
  {
    QString log_tag = QFileInfo(filter_file).fileName();

    std::ifstream ifs(filter_file.toStdString());
    std::string peer_id, client;
    while (ifs >> peer_id >> client) {
      QRegularExpression peer_id_re(QString::fromStdString(peer_id));
      QRegularExpression client_re(QString::fromStdString(client));

      auto msg_tmpl = u"'%1': invalid %2 matching expression '%3' detected at line %4, ignoring rule"_s;
      int line = m_filters.size() + 1;

      if (!peer_id_re.isValid())
        LogMsg(msg_tmpl.arg(log_tag).arg(u"peer id"_s).arg(peer_id_re.pattern()).arg(line), Log::WARNING);

      if (!client_re.isValid())
        LogMsg(msg_tmpl.arg(log_tag).arg(u"client name"_s).arg(client_re.pattern()).arg(line), Log::WARNING);

      if (peer_id_re.isValid() && client_re.isValid())
        m_filters.append({peer_id_re, client_re});
    }
  }

  bool match_peer(const lt::peer_info& info, bool skip_name) const
  {
    QString peer_id = QString::fromLatin1(info.pid.data(), 8);
    QString client = QString::fromStdString(info.client);
    return std::any_of(m_filters.begin(), m_filters.end(),
                       [&](const auto& filter) {
                           return qregex_has_match(filter[0], peer_id) &&
                               (skip_name || qregex_has_match(filter[1], client));
                       });
  }

  bool is_empty() const { return m_filters.isEmpty(); }
  int rules_count() const { return m_filters.size(); }

private:
  QVector<QVector<QRegularExpression>> m_filters;
};
