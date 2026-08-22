/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  qBittorrent contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "helpers.h"

#include <algorithm>
#include <limits>

namespace Streaming
{
    qint64 ByteRange::length() const
    {
        return (last - first + 1);
    }

    nonstd::expected<ByteRange, RangeError> parseHttpRange(const QByteArray &value, const qint64 resourceSize)
    {
        if (resourceSize < 0)
            return nonstd::make_unexpected(RangeError::Unsatisfiable);

        const QByteArray trimmedValue = value.trimmed();
        const qsizetype equalsPos = trimmedValue.indexOf('=');
        if ((equalsPos <= 0) || (trimmedValue.left(equalsPos).trimmed().compare("bytes", Qt::CaseInsensitive) != 0))
            return nonstd::make_unexpected(RangeError::Malformed);

        const QByteArray rangeSpec = trimmedValue.mid(equalsPos + 1).trimmed();
        if (rangeSpec.contains(','))
            return nonstd::make_unexpected(RangeError::MultipleRanges);

        if (rangeSpec.count('-') != 1)
            return nonstd::make_unexpected(RangeError::Malformed);

        const qsizetype dashPos = rangeSpec.indexOf('-');
        const QByteArray firstPart = rangeSpec.left(dashPos).trimmed();
        const QByteArray lastPart = rangeSpec.mid(dashPos + 1).trimmed();
        if (firstPart.isEmpty() && lastPart.isEmpty())
            return nonstd::make_unexpected(RangeError::Malformed);

        if (resourceSize == 0)
            return nonstd::make_unexpected(RangeError::Unsatisfiable);

        bool ok = false;
        if (firstPart.isEmpty())
        {
            const qint64 suffixLength = lastPart.toLongLong(&ok);
            if (!ok || (suffixLength <= 0))
                return nonstd::make_unexpected(ok ? RangeError::Unsatisfiable : RangeError::Malformed);

            const qint64 length = std::min(suffixLength, resourceSize);
            return ByteRange {.first = resourceSize - length, .last = resourceSize - 1};
        }

        const qint64 first = firstPart.toLongLong(&ok);
        if (!ok || (first < 0))
            return nonstd::make_unexpected(RangeError::Malformed);
        if (first >= resourceSize)
            return nonstd::make_unexpected(RangeError::Unsatisfiable);

        if (lastPart.isEmpty())
            return ByteRange {.first = first, .last = resourceSize - 1};

        const qint64 requestedLast = lastPart.toLongLong(&ok);
        if (!ok || (requestedLast < 0))
            return nonstd::make_unexpected(RangeError::Malformed);
        if (requestedLast < first)
            return nonstd::make_unexpected(RangeError::Unsatisfiable);

        return ByteRange {.first = first, .last = std::min(requestedLast, resourceSize - 1)};
    }

    std::optional<PieceRange> mapByteRangeToPieces(const qint64 fileRelativeFirst, const qint64 fileRelativeLast
            , const qint64 torrentFileOffset, const qint64 pieceLength, const int piecesCount)
    {
        if ((fileRelativeFirst < 0) || (fileRelativeLast < fileRelativeFirst) || (torrentFileOffset < 0)
                || (pieceLength <= 0) || (piecesCount <= 0))
            return std::nullopt;

        if (fileRelativeLast > (std::numeric_limits<qint64>::max() - torrentFileOffset))
            return std::nullopt;

        const qint64 first = (torrentFileOffset + fileRelativeFirst) / pieceLength;
        const qint64 last = (torrentFileOffset + fileRelativeLast) / pieceLength;
        if ((first < 0) || (last >= piecesCount) || (first > std::numeric_limits<int>::max()))
            return std::nullopt;

        return PieceRange {.first = static_cast<int>(first), .last = static_cast<int>(last)};
    }
}
