/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  Kieran Devlin
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

#include <cstdint>
#include <vector>

#include "fileschedulingmode.h"

/*
 * Per-file download order ("rank") and per-file sequential download.
 *
 * libtorrent only knows 8 piece priority levels (0 = do not download, 1..7) and
 * one torrent-wide "sequential" flag. It has no notion of "download file A before
 * file B" for an arbitrary number of files, nor of "download only file C sequentially".
 *
 * This module builds such a schedule on top of libtorrent. It is a pure function:
 * it takes the current state of a torrent (files, their pieces, which pieces we have)
 * and returns the piece priorities that should be applied to the libtorrent torrent handle. `TorrentImpl` calls it again whenever the input changes
 * (a piece or file completes, the user changes a setting), so the schedule moves
 * forward as the download progresses.
 *
 * Inputs per file:
 *   - priority:   the normal qBittorrent file priority (0 = "Do not download").
 *   - rank:       0 = unranked, otherwise 1..INT_MAX. Lower rank downloads first.
 *                 Files with equal rank download together.
 *   - sequential: download the pieces of this file in order.
 *
 * The queue:
 *   Wanted, incomplete files are put into a queue of "groups":
 *     1. ranked files, grouped by rank, lowest rank first;
 *     2. unranked files, grouped by priority, highest priority first.
 *   The first group is the "head" of the queue.
 *
 * Mapping groups to libtorrent piece priorities ("bands"):
 *
 *   | queue position        | Soft / RankedFirst          | Strict |
 *   |-----------------------|-----------------------------|--------|
 *   | ranked group 1 (head) | 7                           | 7      |
 *   | ranked group 2        | 6                           | 0      |
 *   | ranked group 3        | 5                           | 0      |
 *   | ranked group 4+       | 4                           | 0      |
 *   | unranked, Maximum     | 3 (RankedFirst: 0)          | 0      |
 *   | unranked, High        | 2 (RankedFirst: 0)          | 0      |
 *   | unranked, Normal      | 1 (RankedFirst: 0)          | 0      |
 *
 *   If there are no ranked files, the first unranked group is the head and gets
 *   priority 7 in Strict mode; RankedFirst behaves like Soft in that case.
 *
 *   How strict is each mechanism? libtorrent's rarest-first picker sorts pieces by
 *   `availability * (8 - priority)` (see `piece_pos::priority()` in libtorrent's piece_picker.hpp),
 *   i.e. priorities 1..7 are a *weight* against rarity, not strict tiers: a priority 7 piece
 *   is preferred over a priority 1 piece unless the latter is more than 7 times rarer.
 *   Only priority 0 is strict: the piece is not downloaded at all ("held").
 *   Therefore Soft mode is a strong preference, RankedFirst strictly keeps unranked files
 *   behind ranked ones, and Strict mode strictly downloads one group at a time.
 *   Because the schedule is rebuilt whenever a file completes, every file moves up
 *   the bands over time, so the order is preserved for any number of ranks even
 *   though libtorrent only has 7 usable levels.
 *
 * Sequential files:
 *   - At the head of the queue: the next `windowSize` missing pieces get priority 7, the rest
 *     of the file gets `PRIORITY_SEQUENTIAL_REST` (or the head band if that is lower). The window
 *     is strongly preferred, and the rest of the file uses spare bandwidth instead of wasting it.
 *     As pieces complete, the window moves forward, so the file is downloaded from start to end.
 *     Piece deadlines (time-critical pieces) are deliberately not used: combined with held pieces
 *     they make libtorrent's request logic stall, and they reduce the download speed.
 *   - Elsewhere in the queue: only the next `windowSize` missing pieces get the file's
 *     band; the rest of the file is held at priority 0. As pieces complete, the window
 *     moves forward, so the file is filled in order with spare bandwidth.
 *
 * "Download first and last pieces first" (edgePieces):
 *   While scheduling is active, it only applies to files at the head of the queue: their
 *   first and last pieces get priority 7. Raising the edge pieces of every file to priority 7
 *   (the classic behaviour) would break the requested file order.
 *
 * Pieces shared by two files (at file boundaries) get the higher of the two priorities.
 *
 * Avoiding false "torrent finished" events:
 *   libtorrent considers a torrent finished when every piece with priority > 0 is
 *   downloaded. In RankedFirst/Strict mode, and for held sequential pieces, wanted pieces
 *   are set to priority 0, so the torrent could look "finished" for a moment when the head
 *   of the queue completes. To make this unlikely, when fewer than `LOOKAHEAD_THRESHOLD`
 *   non-held pieces are missing, held pieces are released at priority 1 in queue order
 *   ("lookahead") until that many pieces are wanted again. `TorrentImpl` additionally ignores
 *   a "finished" event while the schedule still holds pieces.
 */

namespace BitTorrent::FileScheduler
{
    // Rank of a file that has no explicit download order.
    inline constexpr int UNRANKED = 0;

    inline constexpr std::uint8_t PRIORITY_HELD = 0;
    inline constexpr std::uint8_t PRIORITY_LOOKAHEAD = 1;
    inline constexpr std::uint8_t PRIORITY_TOP = 7;
    // Priority of the pieces of a sequential head file that are not in its window.
    inline constexpr std::uint8_t PRIORITY_SEQUENTIAL_REST = 6;
    // Lowest band used by ranked files in Soft/RankedFirst mode. Unranked files use 1..3.
    inline constexpr std::uint8_t PRIORITY_LOWEST_RANKED = 4;

    // When at most this many non-held pieces are missing, a lookahead piece is released.
    inline constexpr int LOOKAHEAD_THRESHOLD = 16;

    struct FileEntry
    {
        // Inclusive piece range of the file. `lastPiece < firstPiece` means the file has no pieces.
        int firstPiece = 0;
        int lastPiece = -1;
        // `DownloadPriority` value (0 = do not download).
        int priority = 1;
        // `UNRANKED` or a positive rank. Negative values are treated as `UNRANKED`.
        int rank = UNRANKED;
        bool sequential = false;
        // Number of pieces at each end of the file for "first and last pieces first", 0 = disabled.
        int edgePieces = 0;
    };

    struct Input
    {
        int pieceCount = 0;
        std::vector<FileEntry> files;
        // Pieces we already have. May be shorter than `pieceCount` (missing entries mean "don't have").
        std::vector<bool> havePieces;
        FileSchedulingMode mode = FileSchedulingMode::Soft;
        // Number of pieces in a sequential window. Values < 1 are treated as 1.
        int windowSize = 8;
    };

    struct Plan
    {
        // Priority (0..7) for every piece of the torrent.
        std::vector<std::uint8_t> piecePriorities;
        // Indexes of the files at the head of the queue (empty if nothing is left to download).
        std::vector<int> headFiles;
        // True if some missing piece of a wanted file has priority 0 (held back by the schedule).
        bool holdsPieces = false;
    };

    // True if at least one file has a rank or is sequential, i.e. a schedule is needed at all.
    bool isActive(const std::vector<FileEntry> &files);

    Plan makePlan(const Input &input);
}
