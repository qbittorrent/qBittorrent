/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2024  Radu Carpa <radu.carpa@cern.ch>
 * Copyright (C) 2017  Mike Tzou (Chocobo1)
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

#include <QDialog>
#include <QThreadPool>

#include "base/bittorrent/torrentcreator.h"
#include "base/path.h"
#include "base/settingvalue.h"

namespace Ui
{
    class TorrentCreatorDialog;
}

class TorrentCreatorDialog final : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TorrentCreatorDialog)

public:
    TorrentCreatorDialog(QWidget *parent = nullptr, const Path &defaultPath = {});
    ~TorrentCreatorDialog() override;
    void updateInputPath(const Path &path);

private slots:
    void updateProgressBar(int progress);
    void updatePiecesCount();
    void onCreateButtonClicked();
    void onAddFileButtonClicked();
    void onAddFolderButtonClicked();
    void handleCreationFailure(const QString &msg);
    void handleCreationSuccess(const BitTorrent::TorrentCreatorResult &result);

private:
    void dropEvent(QDropEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;

    void saveSettings();
    void loadSettings();
    void setInteractionEnabled(bool enabled) const;

    int getPieceSize() const;
#ifdef QBT_USES_LIBTORRENT2
    BitTorrent::TorrentFormat getTorrentFormat() const;
#else
    int getPaddedFileSizeLimit() const;
#endif

    Ui::TorrentCreatorDialog *m_ui = nullptr;
    QThreadPool m_threadPool;

    // settings
    SettingValue<QSize> m_storeDialogSize;
    SettingValue<int> m_storePieceSize;
    SettingValue<bool> m_storePrivateTorrent;
    SettingValue<bool> m_storeStartSeeding;
    SettingValue<bool> m_storeIgnoreRatio;
#ifdef QBT_USES_LIBTORRENT2
    SettingValue<int> m_storeTorrentFormat;
#else
    SettingValue<bool> m_storeOptimizeAlignment;
    SettingValue<int> m_paddedFileSizeLimit;
#endif
    SettingValue<Path> m_storeLastAddPath;
    SettingValue<QString> m_storeTrackerList;
    SettingValue<QString> m_storeWebSeedList;
    SettingValue<QString> m_storeComments;
    SettingValue<Path> m_storeLastSavePath;
    SettingValue<QString> m_storeSource;
};
