#pragma once

#include <algorithm>
#include <fstream>
#include <string>

#include <QDir>
#include <QRegularExpression>
#include <QString>
#include <QVector>

#include <libtorrent/peer_info.hpp>

#include "base/logger.h"
#include "base/profile.h"

#include "peer_filter_plugin.hpp"
#include "peer_logger.hpp"


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
    std::ifstream ifs(filter_file.toStdString());
    std::string peer_id, client;
    while (ifs >> peer_id >> client) {
      QRegularExpression peer_id_re(QString::fromStdString(peer_id));
      QRegularExpression client_re(QString::fromStdString(client));

      QString msg_tmpl("whitelist: invalid %1 matching expression '%2' detected, ignoring rule");

      if (!peer_id_re.isValid())
        LogMsg(msg_tmpl.arg("peer id").arg(peer_id_re.pattern()), Log::WARNING);

      if (!client_re.isValid())
        LogMsg(msg_tmpl.arg("client name").arg(client_re.pattern()), Log::WARNING);

      if (peer_id_re.isValid() && client_re.isValid())
        m_filters.append({peer_id_re, client_re});
    }

    if (m_filters.isEmpty())
      LogMsg("whitelist: no rules were loaded, any connections will be dropped", Log::CRITICAL);
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

private:
  QVector<QVector<QRegularExpression>> m_filters;
};


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
