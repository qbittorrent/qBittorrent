/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2011  Christophe Dumez <chris@qbittorrent.org>
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

#include <QDialog>

#include "base/settingvalue.h"

class QStandardItemModel;

namespace BitTorrent
{
    class TorrentHandle;
}
namespace Ui
{
    class PreviewSelectDialog;
}
class PreviewListDelegate;

class PreviewSelectDialog final : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY(PreviewSelectDialog)

public:
    enum PreviewColumn
    {
        NAME,
        SIZE,
        PROGRESS,
        FILE_INDEX,

        NB_COLUMNS
    };

    PreviewSelectDialog(QWidget *parent, const BitTorrent::TorrentHandle *torrent);
    ~PreviewSelectDialog();

signals:
    void readyToPreviewFile(QString) const;

private slots:
    void previewButtonClicked();

private:
    void showEvent(QShowEvent *event) override;

    void loadWindowState();
    void saveWindowState();

    Ui::PreviewSelectDialog *m_ui;
    QStandardItemModel *m_previewListModel;
    PreviewListDelegate *m_listDelegate;
    const BitTorrent::TorrentHandle *m_torrent;
    bool m_headerStateInitialized = false;

    // Settings
    SettingValue<QSize> m_storeDialogSize;
    SettingValue<QByteArray> m_storeTreeHeaderState;
};
