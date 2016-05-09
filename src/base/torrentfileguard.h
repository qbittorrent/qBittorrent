/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016  Eugene Shalygin <eugene.shalygin@gmail.com>
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

#include <QString>
#include <QMetaType>

class QMetaEnum;
/// Utility class to defer file deletion
class FileGuard
{
public:
    FileGuard(const QString &path = QString());
    ~FileGuard();

    /// Cancels or re-enables deferred file deletion
    void setAutoRemove(bool remove) noexcept;

private:
    QString m_path;
    bool m_remove;
};

/// Reads settings for .torrent files from preferences
/// and sets the file guard up accordingly
class TorrentFileGuard
{
    Q_GADGET

public:
    TorrentFileGuard(const QString &path = QString());
    ~TorrentFileGuard();

    /// marks the torrent file as loaded (added) into the BitTorrent::Session
    void markAsAddedToSession();
    void setAutoRemove(bool remove);

    enum AutoDeleteMode     // do not change these names: they are stored in config file
    {
        Never,
        IfAdded,
        Always
    };

    // static interface to get/set preferences
    static AutoDeleteMode autoDeleteMode();
    static void setautoDeleteMode(AutoDeleteMode mode);

private:
    static QMetaEnum modeMetaEnum();
#if QT_VERSION < QT_VERSION_CHECK(5, 5, 0)
    Q_ENUMS(AutoDeleteMode)
#else
    Q_ENUM(AutoDeleteMode)
#endif
    AutoDeleteMode m_mode;
    bool m_wasAdded;
    // Qt 4 moc has troubles with Q_GADGET: if Q_GADGET is present in a class, moc unconditionally
    // references in the generated code the statiMetaObject from the class ancestor.
    // Moreover, if the ancestor class has Q_GADGET but does not have other
    // Q_ declarations, moc does not generate staticMetaObject for it. These results
    // in referencing the non existent staticMetaObject and such code fails to compile.
    // This problem is NOT present in Qt 5.7.0 and maybe in some older Qt 5 versions too
    // Qt 4.8.7 has it.
    // Therefore, we can't inherit FileGuard :(
    FileGuard m_guard;
};

#if QT_VERSION < QT_VERSION_CHECK(5, 5, 0)
Q_DECLARE_METATYPE(TorrentFileGuard::AutoDeleteMode)
#endif
