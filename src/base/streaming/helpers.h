/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  qBittorrent contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#pragma once

#include <optional>

#include <QByteArray>
#include <QtTypes>

#include "base/3rdparty/expected.hpp"

namespace Streaming
{
    struct ByteRange
    {
        qint64 first = 0;
        qint64 last = 0;

        qint64 length() const;
    };

    enum class RangeError
    {
        Malformed,
        MultipleRanges,
        Unsatisfiable
    };

    struct PieceRange
    {
        int first = 0;
        int last = 0;
    };

    nonstd::expected<ByteRange, RangeError> parseHttpRange(const QByteArray &value, qint64 resourceSize);

    std::optional<PieceRange> mapByteRangeToPieces(qint64 fileRelativeFirst, qint64 fileRelativeLast
            , qint64 torrentFileOffset, qint64 pieceLength, int piecesCount);
}
