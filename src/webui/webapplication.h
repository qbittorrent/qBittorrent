/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2014  Vladimir Golovnev <glassez@yandex.ru>
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

#ifndef WEBAPPLICATION_H
#define WEBAPPLICATION_H

#include <QStringList>
#include "abstractwebapplication.h"

class WebApplication: public AbstractWebApplication
{
    Q_DISABLE_COPY(WebApplication)

public:
    explicit WebApplication(QObject* parent = 0);

private:
    // Actions
    void action_public_webui();
    void action_public_index();
    void action_public_login();
    void action_public_logout();
    void action_public_theme();
    void action_public_images();
    void action_query_torrents();
    void action_query_preferences();
    void action_query_transferInfo();
    void action_query_propertiesGeneral();
    void action_query_propertiesTrackers();
    void action_query_propertiesWebSeeds();
    void action_query_propertiesFiles();
    void action_sync_maindata();
    void action_command_shutdown();
    void action_command_download();
    void action_command_upload();
    void action_command_addTrackers();
    void action_command_resumeAll();
    void action_command_pauseAll();
    void action_command_resume();
    void action_command_pause();
    void action_command_setPreferences();
    void action_command_setFilePrio();
    void action_command_getGlobalUpLimit();
    void action_command_getGlobalDlLimit();
    void action_command_setGlobalUpLimit();
    void action_command_setGlobalDlLimit();
    void action_command_getTorrentsUpLimit();
    void action_command_getTorrentsDlLimit();
    void action_command_setTorrentsUpLimit();
    void action_command_setTorrentsDlLimit();
    void action_command_alternativeSpeedLimitsEnabled();
    void action_command_toggleAlternativeSpeedLimits();
    void action_command_toggleSequentialDownload();
    void action_command_toggleFirstLastPiecePrio();
    void action_command_setSuperSeeding();
    void action_command_setForceStart();
    void action_command_delete();
    void action_command_deletePerm();
    void action_command_increasePrio();
    void action_command_decreasePrio();
    void action_command_topPrio();
    void action_command_bottomPrio();
    void action_command_recheck();
    void action_version_api();
    void action_version_api_min();
    void action_version_qbittorrent();

    typedef void (WebApplication::*Action)();

    QString scope_;
    QString action_;
    QStringList args_;

    void processRequest();

    bool isPublicScope();
    void parsePath();

    static QMap<QString, QMap<QString, Action> > initializeActions();
    static QMap<QString, QMap<QString, Action> > actions_;
};

#endif // WEBAPPLICATION_H
