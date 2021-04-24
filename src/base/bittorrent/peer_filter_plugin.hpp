#pragma once

#include <libtorrent/extensions.hpp>
#include <libtorrent/peer_connection_handle.hpp>

#if (LIBTORRENT_VERSION_NUM >= 20000)
using client_data = lt::client_data_t;
#else
using client_data = void*;
#endif

using filter_function = std::function<bool(const lt::peer_info&, bool, bool*)>;
using action_function = std::function<void(lt::peer_connection_handle)>;

class peer_filter_plugin final : public lt::peer_plugin
{
public:
  peer_filter_plugin(lt::peer_connection_handle p, filter_function filter, action_function action)
    : m_peer_connection(p)
    , m_filter(std::move(filter))
    , m_action(std::move(action))
  {}

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

  bool on_interested() override
  {
    handle_peer();
    return peer_plugin::on_interested();
  }

  bool on_not_interested() override
  {
    handle_peer();
    return peer_plugin::on_not_interested();
  }

  bool on_have(lt::piece_index_t p) override
  {
    handle_peer();
    return peer_plugin::on_have(p);
  }

  bool on_dont_have(lt::piece_index_t p) override
  {
    handle_peer();
    return peer_plugin::on_dont_have(p);
  }

  bool on_bitfield(lt::bitfield const& bitfield) override
  {
    handle_peer();
    return peer_plugin::on_bitfield(bitfield);
  }

  bool on_have_all() override
  {
    handle_peer();
    return peer_plugin::on_have_all();
  }

  bool on_have_none() override
  {
    handle_peer();
    return peer_plugin::on_have_none();
  }

  bool on_request(lt::peer_request const& r) override
  {
    handle_peer();
    return peer_plugin::on_request(r);
  }

protected:
  void handle_peer(bool handshake = false)
  {
    if (m_stop_filtering)
      return;

    lt::peer_info info;
    m_peer_connection.get_peer_info(info);

    if (m_filter(info, handshake, &m_stop_filtering))
      m_action(m_peer_connection);
  }

private:
  lt::peer_connection_handle m_peer_connection;

  filter_function m_filter;
  action_function m_action;

  bool m_stop_filtering = false;
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
