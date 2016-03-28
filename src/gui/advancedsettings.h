/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015
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

#ifndef ADVANCEDSETTINGS_H
#define ADVANCEDSETTINGS_H

#include <QLabel>
#include <QSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QComboBox>
#include <QTableWidget>


class AdvancedSettings: public QTableWidget
{
    Q_OBJECT

public:
    AdvancedSettings(QWidget *parent);

public slots:
    void saveAdvancedSettings();

signals:
    void settingsChanged();

private slots:
    void updateCacheSpinSuffix(int value);

private:
    void loadAdvancedSettings();
    template <typename T> void addRow(int row, const QString &rowText, T* widget);

    QLabel labelQbtLink, labelLibtorrentLink;
    QSpinBox spin_cache, spin_save_resume_data_interval, outgoing_ports_min, outgoing_ports_max, spin_list_refresh, spin_maxhalfopen, spin_tracker_port, spin_cache_ttl;
    QCheckBox cb_os_cache, cb_recheck_completed, cb_resolve_countries, cb_resolve_hosts,
              cb_super_seeding, cb_program_notifications, cb_tracker_status,
              cb_confirm_torrent_recheck, cb_enable_tracker_ext, cb_listen_ipv6, cb_announce_all_trackers;
    QComboBox combo_iface;
    QLineEdit txt_network_address;

    // OS dependent settings
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    QCheckBox cb_update_check;
#endif

#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC))
    QCheckBox cb_use_icon_theme;
#endif
};

#endif // ADVANCEDSETTINGS_H
