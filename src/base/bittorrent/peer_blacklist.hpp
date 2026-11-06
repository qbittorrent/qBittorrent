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

#include <regex>

#include <libtorrent/torrent_info.hpp>

#include <QHostAddress>

#include "base/net/geoipmanager.h"

#include "peer_filter_plugin.hpp"

// bad peer filter
bool is_bad_peer(const lt::peer_info& info)
{
  static const std::regex id_filter("-(XL|SD|XF|QD|BN|DL|TS|DT|HP|GT)(\\d+)-");
  static const std::regex ua_filter(R"((\d+.\d+.\d+.\d+|cacao_torrent))");
  static const std::regex consume_filter(R"(((dt|hp|xm)/torrent|Gopeed dev|Rain \d+(\.\d+)*|(Taipei-torrent( dev)?)))", std::regex_constants::icase);

  // Anyway, block dt/torrent and Taipei-torrent with specific case first.
  if (std::regex_match(info.client, consume_filter)) {
      return true;
  }

  return std::regex_match(info.pid.data(), info.pid.data() + 8, id_filter) || std::regex_match(info.client, ua_filter);
}

// drop connection action
void drop_connection(lt::peer_connection_handle ph)
{
  ph.disconnect(boost::asio::error::connection_refused, lt::operation_t::bittorrent, lt::disconnect_severity_t{0});
}


template<typename F>
auto wrap_filter(F filter)
{
  return [=](const lt::peer_info& info, bool handshake, bool* stop_filtering) {
    bool matched = filter(info);
    *stop_filtering = !handshake && !matched;
    return matched;
  };
}


std::shared_ptr<lt::torrent_plugin> create_peer_action_plugin(
    const lt::torrent_handle& th,
    filter_function filter,
    action_function action)
{
  // ignore private torrents
  if (th.torrent_file() && th.torrent_file()->priv())
    return nullptr;

  return std::make_shared<peer_action_plugin>(std::move(filter), std::move(action));
}


// plugins factory functions

std::shared_ptr<lt::torrent_plugin> create_drop_bad_peers_plugin(lt::torrent_handle const& th, client_data)
{
  return create_peer_action_plugin(th, wrap_filter(is_bad_peer), drop_connection);
}
