#pragma once

#include <regex>

#include <libtorrent/torrent_info.hpp>

#include <QDir>
#include <QHostAddress>

#include "base/logger.h"
#include "base/net/geoipmanager.h"
#include "base/profile.h"

#include "peer_filter_plugin.hpp"
#include "peer_filter.hpp"
#include "peer_logger.hpp"

// bad peer filter
bool is_bad_peer(const lt::peer_info& info)
{
  std::regex id_filter("-(XL|SD|XF|QD|BN|DL)(\\d+)-");
  std::regex ua_filter(R"((\d+.\d+.\d+.\d+|cacao_torrent))");
  return std::regex_match(info.pid.data(), info.pid.data() + 8, id_filter) || std::regex_match(info.client, ua_filter);
}

// Unknown Peer filter
bool is_unknown_peer(const lt::peer_info& info)
{
  QString country = Net::GeoIPManager::instance()->lookup(QHostAddress(info.ip.data()));
  return info.client.find("Unknown") != std::string::npos && country == QLatin1String("CN");
}

// Offline Downloader filter
bool is_offline_downloader(const lt::peer_info& info)
{
  unsigned short port = info.ip.port();
  QString country = Net::GeoIPManager::instance()->lookup(QHostAddress(info.ip.data()));
  return port >= 65000 && country == QLatin1String("CN") && info.client.find("Transmission") != std::string::npos;
}

// BitTorrent Media Player Peer filter
bool is_bittorrent_media_player(const lt::peer_info& info)
{
  std::regex player_filter("-(UW\\w{4})-");
  return !!std::regex_match(info.pid.data(), info.pid.data() + 8, player_filter);
}


// drop connection action
void drop_connection(lt::peer_connection_handle ph)
{
  ph.disconnect(boost::asio::error::connection_refused, lt::operation_t::bittorrent, lt::disconnect_severity_t{0});
}


template<typename F>
auto wrap_filter(F filter, const std::string& tag)
{
  return [=](const lt::peer_info& info, bool handshake, bool* stop_filtering) {
    bool matched = filter(info);
    *stop_filtering = !handshake && !matched;
    if (matched)
      peer_logger_singleton::instance().log_peer(info, tag);
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
  return create_peer_action_plugin(th, wrap_filter(is_bad_peer, "bad peer"), drop_connection);
}

std::shared_ptr<lt::torrent_plugin> create_drop_unknown_peers_plugin(lt::torrent_handle const& th, client_data)
{
  return create_peer_action_plugin(th, wrap_filter(is_unknown_peer, "unknown peer"), drop_connection);
}

std::shared_ptr<lt::torrent_plugin> create_drop_offline_downloader_plugin(lt::torrent_handle const& th, client_data)
{
  return create_peer_action_plugin(th, wrap_filter(is_offline_downloader, "offline downloader"), drop_connection);
}

std::shared_ptr<lt::torrent_plugin> create_drop_bittorrent_media_player_plugin(lt::torrent_handle const& th, client_data)
{
  return create_peer_action_plugin(th, wrap_filter(is_bittorrent_media_player, "bittorrent media player"), drop_connection);
}

std::shared_ptr<lt::torrent_plugin> create_peer_blacklist_plugin(lt::torrent_handle const&, client_data)
{
  QDir qbt_data_dir(specialFolderLocation(SpecialFolder::Data));

  QString filter_file = qbt_data_dir.absoluteFilePath(QStringLiteral("peer_blacklist.txt"));
  // do not create plugin if filter file doesn't exists
  if (!QFile::exists(filter_file)) {
    LogMsg("'peer_blacklist.txt' doesn't exist, do not enabling blacklist plugin", Log::INFO);
    return nullptr;
  }

  // create filter object only once
  static peer_filter filter(filter_file);
  // do not create plugin if no rules were loaded
  if (filter.is_empty()) {
    LogMsg("'peer_blacklist.txt' has no valid rules, do not enabling blacklist plugin", Log::WARNING);
    return nullptr;
  }

  auto peer_in_list = [&](const lt::peer_info& info, bool handshake, bool* stop_filtering) {
    bool matched = filter.match_peer(info, handshake);
    *stop_filtering = !handshake && !matched;
    if (matched)
      peer_logger_singleton::instance().log_peer(info, "blacklist");
    return matched;
  };

  return std::make_shared<peer_action_plugin>(std::move(peer_in_list), drop_connection);
}
