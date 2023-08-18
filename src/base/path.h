/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2022  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2012  Christophe Dumez <chris@qbittorrent.org>
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

#include <filesystem>

#include <QMetaType>
#include <QString>

#include "pathfwd.h"

class QStringView;

class Path final
{
public:
    Path() = default;

    explicit Path(const QString &pathStr);
    explicit Path(const std::string &pathStr);

    bool isValid() const;
    bool isEmpty() const;
    bool isAbsolute() const;
    bool isRelative() const;

    bool exists() const;

    Path rootItem() const;
    Path parentPath() const;

    QString filename() const;

    QString extension() const;
    bool hasExtension(QStringView ext) const;
    void removeExtension();
    Path removedExtension() const;
    void removeExtension(QStringView ext);
    Path removedExtension(QStringView ext) const;

    bool hasAncestor(const Path &other) const;
    Path relativePathOf(const Path &childPath) const;

    QString data() const;
    QString toString() const;
    std::filesystem::path toStdFsPath() const;

    Path &operator/=(const Path &other);
    Path &operator+=(QStringView str);

    static Path commonPath(const Path &left, const Path &right);

    static Path findRootFolder(const PathList &filePaths);
    static void stripRootFolder(PathList &filePaths);
    static void addRootFolder(PathList &filePaths, const Path &rootFolder);

    friend Path operator/(const Path &lhs, const Path &rhs);

private:
    // this constructor doesn't perform any checks
    // so it's intended for internal use only
    static Path createUnchecked(const QString &pathStr);

    QString m_pathStr;
};

Q_DECLARE_METATYPE(Path)

bool operator==(const Path &lhs, const Path &rhs);
Path operator+(const Path &lhs, QStringView rhs);

QDataStream &operator<<(QDataStream &out, const Path &path);
QDataStream &operator>>(QDataStream &in, Path &path);

std::size_t qHash(const Path &key, std::size_t seed = 0);
