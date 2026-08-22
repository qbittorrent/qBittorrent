#pragma once

#include <libtorrent/add_torrent_params.hpp>

namespace BitTorrent
{
    enum class DedupeAction
    {
        Revert,
        Insert,
        MagnetInsert
    };

    int manageDedupedFilenames(lt::add_torrent_params &atp, const DedupeAction action);
}
