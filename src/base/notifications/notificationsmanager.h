/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015 Eugene Shalygin
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

#ifndef NOTIFICATIONSMANAGER_H
#define NOTIFICATIONSMANAGER_H

#include <QObject>
#include <QPointer>

class Application;
class QUrl;

namespace BitTorrent
{
    class TorrentHandle;
}

namespace Notifications
{
    class Notifier;
    class Request;
    enum class CloseReason;
}

namespace Notifications
{
    class Manager: public QObject
    {
        Q_OBJECT
        Q_DISABLE_COPY(Manager)

    public:
        typedef Manager this_type;

        static this_type &instance();
        const Notifier *notifier() const;

        // Notifications properties
        static bool areNotificationsEnabled();
        static void setNotificationsEnabled(bool value);

    protected:
        explicit Manager(Notifier *notifier = nullptr, QObject *parent = nullptr);

        ~Manager();

    private:
        friend class ::Application;
        static void setInstance(this_type *ptr);

    private slots:
        void handleAddTorrentFailure(const QString &error) const;
        // called when a torrent has finished
        void handleTorrentFinished(BitTorrent::TorrentHandle *const torrent) const;
        // Notification when disk is full
        void handleFullDiskError(BitTorrent::TorrentHandle *const torrent, QString msg) const;
        void handleDownloadFromUrlFailure(QString url, QString reason) const;

        void notificationActionTriggered(const Request &request, const QString &actionId);
        void notificationClosed(const Request &request, CloseReason reason);

    private:
        void connectSlots();
        void resetNotifier(Notifier *notifier = nullptr);
        virtual Notifier *createNotifier();
        virtual void openUrl(const QUrl &url);

        static this_type *m_instance; // threading-related issues are not expected for this class, thus a simple pointer
        QPointer<Notifier> m_notifier;
    };
}

#endif // NOTIFICATIONSMANAGER_H
