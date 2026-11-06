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

#include <QDir>

#include "base/logger.h"
#include "base/profile.h"
#include "base/path.h"

#include "peer_filter_plugin.hpp"
#include "peer_filter.hpp"

// filter factory function
std::unique_ptr<peer_filter> create_peer_filter(const QString& filename)
{
  Path qbt_data_dir = specialFolderLocation(SpecialFolder::Data) / Path(filename);

  QString filter_file = qbt_data_dir.toString();
  // do not create plugin if filter file doesn't exists
  if (!QFile::exists(filter_file)) {
    LogMsg(u"'%1' doesn't exist. The corresponding filter is disabled."_s.arg(filename), Log::NORMAL);

    return nullptr;
  }

  auto filter = std::make_unique<peer_filter>(filter_file);
  if (filter->is_empty()) {
    LogMsg(u"'%1' has no valid rules. The corresponding filter is disabled."_s.arg(filename), Log::WARNING);
    filter.reset();
  } else {
    LogMsg(u"'%1' contains %2 valid rules."_s.arg(filename).arg(filter->rules_count()), Log::INFO);
  }

  return filter;
}


// drop connection action
void drop_peer_connection(lt::peer_connection_handle ph)
{
  ph.disconnect(boost::asio::error::connection_refused, lt::operation_t::bittorrent, lt::disconnect_severity_t{0});
}


class peer_filter_session_plugin final : public lt::plugin
{
public:
  peer_filter_session_plugin()
    : m_blacklist(create_peer_filter(QStringLiteral("peer_blacklist.txt")))
    , m_whitelist(create_peer_filter(QStringLiteral("peer_whitelist.txt")))
  {
  }

  std::shared_ptr<lt::torrent_plugin> new_torrent(const lt::torrent_handle& th, client_data) override
  {
    // do not waste CPU and memory for useless objects when no filters are enabled
    if (!m_blacklist && !m_whitelist)
      return nullptr;

    // ignore private torrents
    if (th.torrent_file() && th.torrent_file()->priv())
      return nullptr;

    return std::make_shared<peer_action_plugin>([this](auto&&... args) { return filter(args...); }, drop_peer_connection);
  }

protected:
  bool filter(const lt::peer_info& info, bool handshake, bool* stop_filtering) const
  {
    if (m_blacklist) {
      // always match with both pid & client name when applying blacklist
      bool matched_blacklist = m_blacklist->match_peer(info, false);
      if (matched_blacklist) {
        *stop_filtering = true;
        return true;
      }
    }

    if (m_whitelist) {
      bool matched_whitelist = m_whitelist->match_peer(info, handshake);
      if (!matched_whitelist) {
        *stop_filtering = true;
        return true;
      }
    }

    // if the peer got passed the handshake phase and get here, don't filter it anymore
    if (!handshake) {
      *stop_filtering = true;
    }
    return false;
  }

private:
  std::unique_ptr<peer_filter> m_blacklist;
  std::unique_ptr<peer_filter> m_whitelist;
};
