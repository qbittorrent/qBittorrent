#pragma once

#include <libtorrent/extensions.hpp>
#include <libtorrent/peer_connection_handle.hpp>

using filter_function = std::function<bool(const lt::peer_info&, bool)>;
using action_function = std::function<void(lt::peer_connection_handle)>;

class peer_filter_plugin final : public lt::peer_plugin
{
public:
  peer_filter_plugin(lt::peer_connection_handle p, filter_function filter, action_function action)
    : m_peer_connection(p)
    , m_filter(std::move(filter))
    , m_action(std::move(action))
  {}

  void add_handshake(lt::entry& e) override
  {
    handle_peer(true);
    peer_plugin::add_handshake(e);
  }

  bool on_handshake(lt::span<char const> d) override
  {
    handle_peer(true);
    return peer_plugin::on_handshake(d);
  }

  bool on_extension_handshake(lt::bdecode_node const& d) override
  {
    handle_peer(true);
    return peer_plugin::on_extension_handshake(d);
  }

protected:
  void handle_peer(bool handshake = false)
  {
    lt::peer_info info;
    m_peer_connection.get_peer_info(info);

    if (m_filter(info, handshake))
      m_action(m_peer_connection);
  }

private:
  lt::peer_connection_handle m_peer_connection;

  filter_function m_filter;
  action_function m_action;
};


class peer_action_plugin : public lt::torrent_plugin
{
public:
  peer_action_plugin(filter_function filter, action_function action)
    : m_filter(std::move(filter))
    , m_action(std::move(action))
  {}

  std::shared_ptr<lt::peer_plugin> new_connection(lt::peer_connection_handle const& p) override
  {
    return std::make_shared<peer_filter_plugin>(p, m_filter, m_action);
  }

private:
  filter_function m_filter;
  action_function m_action;
};


class peer_action_plugin_creator
{
public:
  peer_action_plugin_creator(filter_function filter, action_function action)
    : m_filter(std::move(filter))
    , m_action(std::move(action))
  {}

  std::shared_ptr<lt::torrent_plugin> operator()(lt::torrent_handle const&, void*)
  {
    return std::make_shared<peer_action_plugin>(m_filter, m_action);
  }

private:
  filter_function m_filter;
  action_function m_action;
};
