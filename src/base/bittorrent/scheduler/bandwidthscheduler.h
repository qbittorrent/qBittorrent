/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2010  Christophe Dumez <chris@qbittorrent.org>
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

#include <QObject>
#include <QTimer>
#include <QThread>

#include "base/asyncfilestorage.h"
#include "scheduleday.h"

class Application;

class BandwidthScheduler : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(BandwidthScheduler)

    friend class ::Application;

public:
    explicit BandwidthScheduler(QObject *parent = nullptr);
    ~BandwidthScheduler() override;

    static BandwidthScheduler *instance();

    ScheduleDay* scheduleDay(int day) const;
    ScheduleDay* today() const;
    void start();
    void stop();

    QByteArray getJson() const;

signals:
    void limitChangeRequested();
    void scheduleUpdated(int day);

private:
    static QPointer<BandwidthScheduler> m_instance;

    bool loadSchedule();
    void saveSchedule();
    void backupSchedule(QString errorMessage, bool preserveOriginal);
    bool importLegacyScheduler();

    QThread *m_ioThread;
    AsyncFileStorage *m_fileStorage;
    QList<ScheduleDay*> m_scheduleDays;
    QTimer m_timer;
};
