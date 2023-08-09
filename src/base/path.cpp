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

#include <algorithm>

#include <QDataStream>
#include <QDir>
#include <QFileInfo>
#include <QList>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QStringView>

#include "base/concepts/stringable.h"
#include "base/global.h"

#if defined(Q_OS_WIN)
const Qt::CaseSensitivity CASE_SENSITIVITY = Qt::CaseInsensitive;
#else
const Qt::CaseSensitivity CASE_SENSITIVITY = Qt::CaseSensitive;
#endif

const int PATHLIST_TYPEID = qRegisterMetaType<PathList>("PathList");

namespace
{
    QString cleanPath(const QString &path)
    {
        const bool hasSeparator = std::any_of(path.cbegin(), path.cend(), [](const QChar c)
        {
            return (c == u'/') || (c == u'\\');
        });
        return hasSeparator ? QDir::cleanPath(path) : path;
    }

#ifdef Q_OS_WIN
    bool hasDriveLetter(const QStringView path)
    {
        const QRegularExpression driveLetterRegex {u"^[A-Za-z]:/"_s};
        return driveLetterRegex.match(path).hasMatch();
    }
#endif
}

// `Path` should satisfy `Stringable` concept in order to be stored in settings as string
static_assert(Stringable<Path>);

Path::Path(const QString &pathStr)
    : m_pathStr {cleanPath(pathStr)}
{
}

Path::Path(const std::string &pathStr)
    : Path(QString::fromStdString(pathStr))
{
}

bool Path::isValid() const
{
    // does not support UNC path

    if (isEmpty())
        return false;

    // https://stackoverflow.com/a/31976060
#if defined(Q_OS_WIN)
    QStringView view = m_pathStr;
    if (hasDriveLetter(view))
        view = view.mid(3);

    // \\37 is using base-8 number system
    const QRegularExpression regex {u"[\\0-\\37:?\"*<>|]"_s};
    return !regex.match(view).hasMatch();
#elif defined(Q_OS_MACOS)
    const QRegularExpression regex {u"[\\0:]"_s};
#else
    const QRegularExpression regex {u"\\0"_s};
#endif
    return !m_pathStr.contains(regex);
}

bool Path::isEmpty() const
{
    return m_pathStr.isEmpty();
}

bool Path::isAbsolute() const
{
    // `QDir::isAbsolutePath` treats `:` as a path to QResource, so handle it manually
    if (m_pathStr.startsWith(u':'))
        return false;
    return QDir::isAbsolutePath(m_pathStr);
}

bool Path::isRelative() const
{
    // `QDir::isRelativePath` treats `:` as a path to QResource, so handle it manually
    if (m_pathStr.startsWith(u':'))
        return true;
    return QDir::isRelativePath(m_pathStr);
}

bool Path::exists() const
{
    return !isEmpty() && QFileInfo::exists(m_pathStr);
}

Path Path::rootItem() const
{
    // does not support UNC path

    const int slashIndex = m_pathStr.indexOf(u'/');
    if (slashIndex < 0)
        return *this;

    if (slashIndex == 0) // *nix absolute path
        return createUnchecked(u"/"_s);

#ifdef Q_OS_WIN
    // should be `c:/` instead of `c:`
    if ((slashIndex == 2) && hasDriveLetter(m_pathStr))
        return createUnchecked(m_pathStr.left(slashIndex + 1));
#endif
    return createUnchecked(m_pathStr.left(slashIndex));
}

Path Path::parentPath() const
{
    // does not support UNC path

    const int slashIndex = m_pathStr.lastIndexOf(u'/');
    if (slashIndex == -1)
        return {};

    if (slashIndex == 0) // *nix absolute path
        return (m_pathStr.size() == 1) ? Path() : createUnchecked(u"/"_s);

#ifdef Q_OS_WIN
    // should be `c:/` instead of `c:`
    // Windows "drive letter" is limited to one alphabet
    if ((slashIndex == 2) && hasDriveLetter(m_pathStr))
        return (m_pathStr.size() == 3) ? Path() : createUnchecked(m_pathStr.left(slashIndex + 1));
#endif
    return createUnchecked(m_pathStr.left(slashIndex));
}

QString Path::filename() const
{
    const int slashIndex = m_pathStr.lastIndexOf(u'/');
    if (slashIndex == -1)
        return m_pathStr;

    return m_pathStr.mid(slashIndex + 1);
}

QString Path::extension() const
{
    const QString suffix = QMimeDatabase().suffixForFileName(m_pathStr);
    if (!suffix.isEmpty())
        return (u"." + suffix);

    const int slashIndex = m_pathStr.lastIndexOf(u'/');
    const auto filename = QStringView(m_pathStr).mid(slashIndex + 1);
    const int dotIndex = filename.lastIndexOf(u'.', -2);
    return ((dotIndex == -1) ? QString() : filename.mid(dotIndex).toString());
}

bool Path::hasExtension(const QStringView ext) const
{
    Q_ASSERT(ext.startsWith(u'.') && (ext.size() >= 2));

    return m_pathStr.endsWith(ext, Qt::CaseInsensitive);
}

bool Path::hasAncestor(const Path &other) const
{
    if (other.isEmpty() || (m_pathStr.size() <= other.m_pathStr.size()))
        return false;

    return (m_pathStr[other.m_pathStr.size()] == u'/')
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

Path Path::removedExtension() const
{
    return createUnchecked(m_pathStr.chopped(extension().size()));
}

void Path::removeExtension(const QStringView ext)
{
    if (hasExtension(ext))
        m_pathStr.chop(ext.size());
}

Path Path::removedExtension(const QStringView ext) const
{
    return (hasExtension(ext) ? createUnchecked(m_pathStr.chopped(ext.size())) : *this);
}

QString Path::data() const
{
    return m_pathStr;
}

QString Path::toString() const
{
    return QDir::toNativeSeparators(m_pathStr);
}

std::filesystem::path Path::toStdFsPath() const
{
#ifdef Q_OS_WIN
    return {data().toStdWString(), std::filesystem::path::format::generic_format};
#else
    return {data().toStdString(), std::filesystem::path::format::generic_format};
#endif
}

Path &Path::operator/=(const Path &other)
{
    *this = *this / other;
    return *this;
}

Path &Path::operator+=(const QStringView str)
{
    *this = *this + str;
    return *this;
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
        filePath.m_pathStr.remove(0, (commonRootFolder.m_pathStr.size() + 1));
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

Path operator/(const Path &lhs, const Path &rhs)
{
    if (rhs.isEmpty())
        return lhs;

    if (lhs.isEmpty())
        return rhs;

    return Path(lhs.m_pathStr + u'/' + rhs.m_pathStr);
}

Path operator+(const Path &lhs, const QStringView rhs)
{
    return Path(lhs.data() + rhs);
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

std::size_t qHash(const Path &key, const std::size_t seed)
{
    return ::qHash(key.data(), seed);
}
