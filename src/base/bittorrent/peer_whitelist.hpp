#pragma once

#include <QDir>

#include "base/logger.h"
#include "base/profile.h"

#include "peer_filter.hpp"
#include "peer_filter_plugin.hpp"
#include "peer_logger.hpp"

std::shared_ptr<lt::torrent_plugin> create_peer_whitelist_plugin(lt::torrent_handle const&, client_data)
{
  QDir qbt_data_dir(specialFolderLocation(SpecialFolder::Data));

  QString filter_file = qbt_data_dir.absoluteFilePath(QStringLiteral("peer_whitelist.txt"));
  // do not create plugin if filter file doesn't exists
  if (!QFile::exists(filter_file)) {
    LogMsg("'peer_whitelist.txt' doesn't exist, do not enabling whitelist plugin", Log::INFO);
    return nullptr;
  }

  // create filter object only once
  static peer_filter filter(filter_file);
  // do not create plugin if no rules were loaded
  if (filter.is_empty()) {
    LogMsg("'peer_whitelist.txt' has no valid rules, do not enabling whitelist plugin", Log::WARNING);
    return nullptr;
  }

  auto peer_not_in_list = [&](const lt::peer_info& info, bool handshake, bool* stop_filtering) {
    bool matched = filter.match_peer(info, handshake);
    *stop_filtering = !handshake && matched;
    if (!matched)
      peer_logger_singleton::instance().log_peer(info, "whitelist");
    return !matched;
  };

  auto drop_connection = [](lt::peer_connection_handle p) {
    p.disconnect(boost::asio::error::connection_refused,
                 lt::operation_t::bittorrent,
                 lt::disconnect_severity_t{0});
  };

  return std::make_shared<peer_action_plugin>(std::move(peer_not_in_list), std::move(drop_connection));
}
