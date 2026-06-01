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

#include <QString>
#include <libtorrent/extensions.hpp>
#include <libtorrent/peer_connection_handle.hpp>

#if (LIBTORRENT_VERSION_NUM >= 20000)
using client_data = lt::client_data_t;
#else
using client_data = void *;
#endif

using filter_function = std::function<bool(const lt::peer_info &, bool, bool *)>;

class peer_shadowban_plugin final : public lt::peer_plugin
{
public:
    peer_shadowban_plugin(lt::peer_connection_handle p)
        : m_peer_connection(p)
    {
    }

    bool on_request(lt::peer_request const &r) override
    {
        if (is_shadowbanned_peer())
        {
            return true;
        }
        return peer_plugin::on_request(r);
    }

    // don't send request if peer shadowbanned to prevent use this function to leech
    bool write_request(lt::peer_request const &r) override
    {
        if (is_shadowbanned_peer())
        {
            return true;
        }
        return peer_plugin::write_request(r);
    }

protected:
    bool is_shadowbanned_peer()
    {
        lt::peer_info info;
        m_peer_connection.get_peer_info(info);

        QString peer_ip = QString::fromStdString(info.ip.address().to_string());
        QStringList shadowbannedIPs =
            CachedSettingValue<QStringList>(u"State/ShadowBannedIPs"_s, QStringList(), Algorithm::sorted<QStringList>).get();

        return shadowbannedIPs.contains(peer_ip);
    }

private:
    lt::peer_connection_handle m_peer_connection;
};

class peer_shadowban_action_plugin : public lt::torrent_plugin
{
public:
    peer_shadowban_action_plugin()
    {
    }

    std::shared_ptr<lt::peer_plugin> new_connection(lt::peer_connection_handle const &p) override
    {
        return std::make_shared<peer_shadowban_plugin>(p);
    }
};

std::shared_ptr<lt::torrent_plugin> create_peer_shadowban_plugin(const lt::torrent_handle &th, client_data)
{
    // ignore private torrents
    if (th.torrent_file() && th.torrent_file()->priv())
        return nullptr;

    return std::make_shared<peer_shadowban_action_plugin>();
}
