/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2010  Christophe Dumez
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
 *
 * Contact : chris@qbittorrent.org
 */

#ifndef PROGRAMUPDATER_H
#define PROGRAMUPDATER_H

#include <QObject>
#include <QUrl>

class QNetworkReply;
class QNetworkAccessManager;

class ProgramUpdater : public QObject
{
    Q_OBJECT
public:
    explicit ProgramUpdater(QObject *parent = 0, bool invokedByUser = false);
    ~ProgramUpdater();
    void checkForUpdates();
    void updateProgram();

protected:
    QString extractVersionNumber(const QString& title) const;
    bool isVersionMoreRecent(const QString& new_version) const;

protected slots:
    void rssDownloadFinished(QNetworkReply* reply);
    void setUpdateUrl(QString title);

signals:
    void updateCheckFinished(bool update_available, QString version, bool invokedByUser);

private:
    QString m_updateUrl;
    QNetworkAccessManager *mp_manager;
    bool m_invokedByUser;
};

#endif // PROGRAMUPDATER_H
