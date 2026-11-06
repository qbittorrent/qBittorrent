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

#include "filescheduler.h"

#include <algorithm>

namespace
{
    using namespace BitTorrent;
    using namespace BitTorrent::FileScheduler;

    struct Group
    {
        std::vector<int> files;
        bool ranked = false;
        int priority = 0; // only for unranked groups
    };

    std::uint8_t clampPriority(const int value)
    {
        return static_cast<std::uint8_t>(std::clamp(value, 0, static_cast<int>(PRIORITY_TOP)));
    }

    int effectiveRank(const FileEntry &file)
    {
        return (file.rank > 0) ? file.rank : UNRANKED;
    }

    bool hasPieces(const FileEntry &file)
    {
        return file.lastPiece >= file.firstPiece;
    }

    // Band of the unranked group with the given file priority in Soft mode.
    // Unranked files stay below every ranked band but keep their relative priorities.
    std::uint8_t unrankedBand(const int priority)
    {
        if (priority >= 7) // DownloadPriority::Maximum
            return 3;
        if (priority >= 6) // DownloadPriority::High
            return 2;
        return 1;
    }

    std::uint8_t groupBand(const Group &group, const int groupIndex, const int rankedGroupCount, const FileSchedulingMode mode)
    {
        if (mode == FileSchedulingMode::Strict)
            return (groupIndex == 0) ? PRIORITY_TOP : PRIORITY_HELD;

        if (group.ranked)
            return static_cast<std::uint8_t>(std::max((PRIORITY_TOP - groupIndex), static_cast<int>(PRIORITY_LOWEST_RANKED)));

        if ((mode == FileSchedulingMode::RankedFirst) && (rankedGroupCount > 0))
            return PRIORITY_HELD;

        return unrankedBand(group.priority);
    }
}

bool BitTorrent::FileScheduler::isActive(const std::vector<FileEntry> &files)
{
    return std::ranges::any_of(files, [](const FileEntry &file)
    {
        return (effectiveRank(file) != UNRANKED) || file.sequential;
    });
}

BitTorrent::FileScheduler::Plan BitTorrent::FileScheduler::makePlan(const Input &input)
{
    const int pieceCount = std::max(input.pieceCount, 0);
    const int windowSize = std::max(input.windowSize, 1);

    const auto isValidPiece = [pieceCount](const int piece) { return (piece >= 0) && (piece < pieceCount); };
    const auto have = [&input](const int piece)
    {
        return (static_cast<std::size_t>(piece) < input.havePieces.size()) && input.havePieces[piece];
    };

    Plan plan;
    plan.piecePriorities.assign(pieceCount, PRIORITY_HELD);

    const auto raise = [&plan, &isValidPiece](const int piece, const std::uint8_t priority)
    {
        if (isValidPiece(piece))
            plan.piecePriorities[piece] = std::max(plan.piecePriorities[piece], priority);
    };

    // 1. Split wanted files into complete and pending (incomplete) ones.
    //    `wantedMissing` marks pieces that are still needed by some wanted file.
    std::vector<int> pending;
    std::vector<bool> wantedMissing(pieceCount, false);
    for (int fileIndex = 0; fileIndex < static_cast<int>(input.files.size()); ++fileIndex)
    {
        const FileEntry &file = input.files[fileIndex];
        if ((file.priority <= 0) || !hasPieces(file))
            continue;

        const int first = std::max(file.firstPiece, 0);
        const int last = std::min(file.lastPiece, (pieceCount - 1));
        bool complete = true;
        for (int piece = first; piece <= last; ++piece)
        {
            if (!have(piece))
            {
                complete = false;
                wantedMissing[piece] = true;
            }
        }

        if (complete)
        {
            // Keep downloaded pieces "wanted" so that libtorrent's wanted/done accounting stays meaningful.
            for (int piece = first; piece <= last; ++piece)
                raise(piece, clampPriority(file.priority));
        }
        else
        {
            pending.push_back(fileIndex);
        }
    }

    if (pending.empty())
        return plan;

    // 2. Build the queue of groups.
    std::vector<int> ranked;
    std::vector<int> unranked;
    for (const int fileIndex : pending)
    {
        if (effectiveRank(input.files[fileIndex]) != UNRANKED)
            ranked.push_back(fileIndex);
        else
            unranked.push_back(fileIndex);
    }

    std::ranges::stable_sort(ranked, [&input](const int left, const int right)
    {
        return effectiveRank(input.files[left]) < effectiveRank(input.files[right]);
    });
    std::ranges::stable_sort(unranked, [&input](const int left, const int right)
    {
        return input.files[left].priority > input.files[right].priority;
    });

    std::vector<Group> groups;
    for (const int fileIndex : ranked)
    {
        const int rank = effectiveRank(input.files[fileIndex]);
        if (groups.empty() || (effectiveRank(input.files[groups.back().files.front()]) != rank))
            groups.push_back({.files = {}, .ranked = true, .priority = 0});
        groups.back().files.push_back(fileIndex);
    }
    const int rankedGroupCount = static_cast<int>(groups.size());
    for (const int fileIndex : unranked)
    {
        const int priority = input.files[fileIndex].priority;
        if ((static_cast<int>(groups.size()) == rankedGroupCount) || (groups.back().priority != priority))
            groups.push_back({.files = {}, .ranked = false, .priority = priority});
        groups.back().files.push_back(fileIndex);
    }

    plan.headFiles = groups.front().files;

    // 3. Assign bands.
    for (int groupIndex = 0; groupIndex < static_cast<int>(groups.size()); ++groupIndex)
    {
        const Group &group = groups[groupIndex];
        const std::uint8_t band = groupBand(group, groupIndex, rankedGroupCount, input.mode);
        const bool isHead = (groupIndex == 0);

        for (const int fileIndex : group.files)
        {
            const FileEntry &file = input.files[fileIndex];
            const int first = std::max(file.firstPiece, 0);
            const int last = std::min(file.lastPiece, (pieceCount - 1));

            if (file.sequential && isHead)
            {
                // Sequential file at the head: the window gets the top priority, the rest of the file
                // stays wanted just below it, so spare bandwidth is not wasted.
                const std::uint8_t restPriority = std::min(band, PRIORITY_SEQUENTIAL_REST);
                int windowed = 0;
                for (int piece = first; piece <= last; ++piece)
                {
                    if (have(piece))
                    {
                        raise(piece, band);
                    }
                    else if (windowed < windowSize)
                    {
                        raise(piece, PRIORITY_TOP);
                        ++windowed;
                    }
                    else
                    {
                        raise(piece, restPriority);
                    }
                }
            }
            else if (file.sequential && (band != PRIORITY_HELD))
            {
                // Sequential file that is not at the head: only the window may download.
                int windowed = 0;
                for (int piece = first; piece <= last; ++piece)
                {
                    if (have(piece))
                    {
                        raise(piece, band);
                    }
                    else if (windowed < windowSize)
                    {
                        raise(piece, band);
                        ++windowed;
                    }
                }
            }
            else
            {
                for (int piece = first; piece <= last; ++piece)
                    raise(piece, band);
            }

            if (isHead && (file.edgePieces > 0))
            {
                for (int i = 0; i < file.edgePieces; ++i)
                {
                    raise((first + i), PRIORITY_TOP);
                    raise((last - i), PRIORITY_TOP);
                }
            }
        }
    }

    // 4. Detect held pieces and release a lookahead piece near the end of the active part.
    int activeMissing = 0;
    for (int piece = 0; piece < pieceCount; ++piece)
    {
        if (!wantedMissing[piece] || have(piece))
            continue;

        if (plan.piecePriorities[piece] == PRIORITY_HELD)
            plan.holdsPieces = true;
        else
            ++activeMissing;
    }

    if (plan.holdsPieces && (activeMissing < LOOKAHEAD_THRESHOLD))
    {
        // Keep at least `LOOKAHEAD_THRESHOLD` missing pieces wanted by releasing held pieces in queue order,
        // so that they cannot all complete before the schedule is moved forward.
        int toRelease = LOOKAHEAD_THRESHOLD - activeMissing;
        for (const Group &group : groups)
        {
            for (const int fileIndex : group.files)
            {
                const FileEntry &file = input.files[fileIndex];
                const int first = std::max(file.firstPiece, 0);
                const int last = std::min(file.lastPiece, (pieceCount - 1));
                for (int piece = first; (piece <= last) && (toRelease > 0); ++piece)
                {
                    if (!have(piece) && (plan.piecePriorities[piece] == PRIORITY_HELD))
                    {
                        plan.piecePriorities[piece] = PRIORITY_LOOKAHEAD;
                        --toRelease;
                    }
                }
            }
        }

        plan.holdsPieces = false;
        for (int piece = 0; piece < pieceCount; ++piece)
        {
            if (wantedMissing[piece] && !have(piece) && (plan.piecePriorities[piece] == PRIORITY_HELD))
            {
                plan.holdsPieces = true;
                break;
            }
        }
    }

    return plan;
}
