#pragma once

#include <QDir>

#include "base/logger.h"
#include "base/profile.h"

#include "peer_filter_plugin.hpp"
#include "peer_filter.hpp"
#include "peer_logger.hpp"

// filter factory function
std::unique_ptr<peer_filter> create_peer_filter(const QString& filename)
{
  QDir qbt_data_dir(specialFolderLocation(SpecialFolder::Data));

  QString filter_file = qbt_data_dir.absoluteFilePath(filename);
  // do not create plugin if filter file doesn't exists
  if (!QFile::exists(filter_file)) {
    LogMsg(QString("'%1' doesn't exist, do not enabling filter").arg(filename), Log::NORMAL);
    return nullptr;
  }

  auto filter = std::make_unique<peer_filter>(filter_file);
  if (filter->is_empty()) {
    LogMsg(QString("'%1' has no valid rules, do not enabling filter").arg(filename), Log::WARNING);
    filter.reset();
  } else {
    LogMsg(QString("'%1' contains %2 valid rules").arg(filename).arg(filter->rules_count()), Log::INFO);
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

  std::shared_ptr<lt::torrent_plugin> new_torrent(const lt::torrent_handle&, client_data) override
  {
    // do not waste CPU and memory for useless objects when no filters are enabled
    if (!m_blacklist && !m_whitelist)
      return nullptr;
    return std::make_shared<peer_action_plugin>([this](auto&&... args) { return filter(args...); }, drop_peer_connection);
  }

protected:
  bool filter(const lt::peer_info& info, bool handshake, bool* stop_filtering) const
  {
    if (m_blacklist) {
      bool matched = m_blacklist->match_peer(info, false);
      *stop_filtering = !handshake && !matched;
      if (matched)
        peer_logger_singleton::instance().log_peer(info, "blacklist");
      return matched;
    }

    if (m_whitelist) {
      bool matched = m_whitelist->match_peer(info, handshake);
      *stop_filtering = !handshake && matched;
      if (!matched)
        peer_logger_singleton::instance().log_peer(info, "whitelist");
      return !matched;
    }

    return false;
  }

private:
  std::unique_ptr<peer_filter> m_blacklist;
  std::unique_ptr<peer_filter> m_whitelist;
};
