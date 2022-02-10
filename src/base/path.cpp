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

#include "path.h"

#include <QDataStream>
#include <QDir>
#include <QFileInfo>
#include <QList>
#include <QMimeDatabase>
#include <QRegularExpression>

#if defined(Q_OS_WIN)
const Qt::CaseSensitivity CASE_SENSITIVITY = Qt::CaseInsensitive;
#else
const Qt::CaseSensitivity CASE_SENSITIVITY = Qt::CaseSensitive;
#endif

const int PATHLIST_TYPEID = qRegisterMetaType<PathList>("PathList");

Path::Path(const QString &pathStr)
    : m_pathStr {QDir::cleanPath(pathStr)}
{
}

Path::Path(const std::string &pathStr)
    : Path(QString::fromStdString(pathStr))
{
}

Path::Path(const char pathStr[])
    : Path(QString::fromLatin1(pathStr))
{
}

bool Path::isValid() const
{
    if (isEmpty())
        return false;

#if defined(Q_OS_WIN)
    const QRegularExpression regex {QLatin1String("[:?\"*<>|]")};
#elif defined(Q_OS_MACOS)
    const QRegularExpression regex {QLatin1String("[\\0:]")};
#else
    const QRegularExpression regex {QLatin1String("[\\0]")};
#endif
    return !m_pathStr.contains(regex);
}

bool Path::isEmpty() const
{
    return m_pathStr.isEmpty();
}

bool Path::isAbsolute() const
{
    return QDir::isAbsolutePath(m_pathStr);
}

bool Path::isRelative() const
{
    return QDir::isRelativePath(m_pathStr);
}

bool Path::exists() const
{
    return !isEmpty() && QFileInfo::exists(m_pathStr);
}

Path Path::rootItem() const
{
    const int slashIndex = m_pathStr.indexOf(QLatin1Char('/'));
    if (slashIndex < 0)
        return *this;

    if (slashIndex == 0) // *nix absolute path
        return createUnchecked(QLatin1String("/"));

    return createUnchecked(m_pathStr.left(slashIndex));
}

Path Path::parentPath() const
{
    const int slashIndex = m_pathStr.lastIndexOf(QLatin1Char('/'));
    if (slashIndex == -1)
        return {};

    if (slashIndex == 0) // *nix absolute path
        return (m_pathStr.size() == 1) ? Path() : createUnchecked(QLatin1String("/"));

    return createUnchecked(m_pathStr.left(slashIndex));
}

QString Path::filename() const
{
    const int slashIndex = m_pathStr.lastIndexOf('/');
    if (slashIndex == -1)
        return m_pathStr;

    return m_pathStr.mid(slashIndex + 1);
}

QString Path::extension() const
{
    const QString suffix = QMimeDatabase().suffixForFileName(m_pathStr);
    if (!suffix.isEmpty())
        return (QLatin1String(".") + suffix);

    const int slashIndex = m_pathStr.lastIndexOf(QLatin1Char('/'));
    const auto filename = QStringView(m_pathStr).mid(slashIndex + 1);
    const int dotIndex = filename.lastIndexOf(QLatin1Char('.'));
    return ((dotIndex == -1) ? QString() : filename.mid(dotIndex).toString());
}

bool Path::hasExtension(const QString &ext) const
{
    return (extension().compare(ext, Qt::CaseInsensitive) == 0);
}

bool Path::hasAncestor(const Path &other) const
{
    if (other.isEmpty() || (m_pathStr.size() <= other.m_pathStr.size()))
        return false;

    return (m_pathStr[other.m_pathStr.size()] == QLatin1Char('/'))
            && m_pathStr.startsWith(other.m_pathStr, CASE_SENSITIVITY);
}

Path Path::relativePathOf(const Path &childPath) const
{
    // If both paths are relative, we assume that they have the same base path
    if (isRelative() && childPath.isRelative())
        return Path(QDir(QDir::home().absoluteFilePath(m_pathStr)).relativeFilePath(QDir::home().absoluteFilePath(childPath.data())));

    return Path(QDir(m_pathStr).relativeFilePath(childPath.data()));
}

void Path::removeExtension()
{
    m_pathStr.chop(extension().size());
}

QString Path::data() const
{
    return m_pathStr;
}

QString Path::toString() const
{
    return QDir::toNativeSeparators(m_pathStr);
}

Path &Path::operator/=(const Path &other)
{
    *this = *this / other;
    return *this;
}

Path &Path::operator+=(const QString &str)
{
    *this = *this + str;
    return *this;
}

Path &Path::operator+=(const char str[])
{
    return (*this += QString::fromLatin1(str));
}

Path &Path::operator+=(const std::string &str)
{
    return (*this += QString::fromStdString(str));
}

Path Path::commonPath(const Path &left, const Path &right)
{
    if (left.isEmpty() || right.isEmpty())
        return {};

    const QList<QStringView> leftPathItems = QStringView(left.m_pathStr).split(u'/');
    const QList<QStringView> rightPathItems = QStringView(right.m_pathStr).split(u'/');
    int commonItemsCount = 0;
    qsizetype commonPathSize = 0;
    while ((commonItemsCount < leftPathItems.size()) && (commonItemsCount < rightPathItems.size()))
    {
        const QStringView leftPathItem = leftPathItems[commonItemsCount];
        const QStringView rightPathItem = rightPathItems[commonItemsCount];
        if (leftPathItem.compare(rightPathItem, CASE_SENSITIVITY) != 0)
            break;

        ++commonItemsCount;
        commonPathSize += leftPathItem.size();
    }

    if (commonItemsCount > 0)
        commonPathSize += (commonItemsCount - 1); // size of intermediate separators

    return Path::createUnchecked(left.m_pathStr.left(commonPathSize));
}

Path Path::findRootFolder(const PathList &filePaths)
{
    Path rootFolder;
    for (const Path &filePath : filePaths)
    {
        const auto filePathElements = QStringView(filePath.m_pathStr).split(u'/');
        // if at least one file has no root folder, no common root folder exists
        if (filePathElements.count() <= 1)
            return {};

        if (rootFolder.isEmpty())
            rootFolder.m_pathStr = filePathElements.at(0).toString();
        else if (rootFolder.m_pathStr != filePathElements.at(0))
            return {};
    }

    return rootFolder;
}

void Path::stripRootFolder(PathList &filePaths)
{
    const Path commonRootFolder = findRootFolder(filePaths);
    if (commonRootFolder.isEmpty())
        return;

    for (Path &filePath : filePaths)
        filePath.m_pathStr = filePath.m_pathStr.mid(commonRootFolder.m_pathStr.size() + 1);
}

void Path::addRootFolder(PathList &filePaths, const Path &rootFolder)
{
    Q_ASSERT(!rootFolder.isEmpty());

    for (Path &filePath : filePaths)
        filePath = rootFolder / filePath;
}

Path Path::createUnchecked(const QString &pathStr)
{
    Path path;
    path.m_pathStr = pathStr;

    return path;
}

bool operator==(const Path &lhs, const Path &rhs)
{
    return (lhs.data().compare(rhs.data(), CASE_SENSITIVITY) == 0);
}

bool operator!=(const Path &lhs, const Path &rhs)
{
    return !(lhs == rhs);
}

Path operator/(const Path &lhs, const Path &rhs)
{
    if (rhs.isEmpty())
        return lhs;

    if (lhs.isEmpty())
        return rhs;

    return Path(lhs.m_pathStr + QLatin1Char('/') + rhs.m_pathStr);
}

Path operator+(const Path &lhs, const QString &rhs)
{
    return Path(lhs.m_pathStr + rhs);
}

Path operator+(const Path &lhs, const char rhs[])
{
    return lhs + QString::fromLatin1(rhs);
}

Path operator+(const Path &lhs, const std::string &rhs)
{
    return lhs + QString::fromStdString(rhs);
}

QDataStream &operator<<(QDataStream &out, const Path &path)
{
    out << path.data();
    return out;
}

QDataStream &operator>>(QDataStream &in, Path &path)
{
    QString pathStr;
    in >> pathStr;
    path = Path(pathStr);
    return in;
}

uint qHash(const Path &key, const uint seed)
{
    return ::qHash(key.data(), seed);
}
