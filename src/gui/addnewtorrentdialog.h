/*
 * Bittorrent Client using Qt and libtorrent.
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

#pragma once

#include <memory>

#include <QDialog>

#include "base/bittorrent/addtorrentparams.h"
#include "base/bittorrent/magneturi.h"
#include "base/bittorrent/torrentinfo.h"
#include "base/settingvalue.h"

namespace BitTorrent
{
    class InfoHash;
}

namespace Net
{
    struct DownloadResult;
}

namespace Ui
{
    class AddNewTorrentDialog;
}

class PropListDelegate;
class TorrentContentFilterModel;
class TorrentFileGuard;

class AddNewTorrentDialog final : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AddNewTorrentDialog)

public:
    static const int minPathHistoryLength = 0;
    static const int maxPathHistoryLength = 99;

    ~AddNewTorrentDialog() override;

    static bool isEnabled();
    static void setEnabled(bool value);
    static bool isTopLevel();
    static void setTopLevel(bool value);
    static int savePathHistoryLength();
    static void setSavePathHistoryLength(int value);

    static void show(const QString &source, const BitTorrent::AddTorrentParams &inParams, QWidget *parent);
    static void show(const QString &source, QWidget *parent);

private slots:
    void displayContentTreeMenu(const QPoint &);
    void updateDiskSpaceLabel();
    void onSavePathChanged(const QString &newPath);
    void onDownloadPathChanged(const QString &newPath);
    void onUseDownloadPathChanged(bool checked);
    void updateMetadata(const BitTorrent::TorrentInfo &metadata);
    void handleDownloadFinished(const Net::DownloadResult &downloadResult);
    void TMMChanged(int index);
    void categoryChanged(int index);
    void contentLayoutChanged(int index);
    void doNotDeleteTorrentClicked(bool checked);
    void renameSelectedFile();

    void accept() override;
    void reject() override;

private:
    explicit AddNewTorrentDialog(const BitTorrent::AddTorrentParams &inParams, QWidget *parent);
    bool loadTorrentFile(const QString &torrentPath);
    bool loadTorrentImpl();
    bool loadMagnet(const BitTorrent::MagnetUri &magnetUri);
    void populateSavePaths();
    void loadState();
    void saveState();
    void setMetadataProgressIndicator(bool visibleIndicator, const QString &labelText = {});
    void setupTreeview();
    void saveTorrentFile();
    bool hasMetadata() const;

    void showEvent(QShowEvent *event) override;

    Ui::AddNewTorrentDialog *m_ui;
    TorrentContentFilterModel *m_contentModel = nullptr;
    PropListDelegate *m_contentDelegate = nullptr;
    BitTorrent::MagnetUri m_magnetURI;
    BitTorrent::TorrentInfo m_torrentInfo;
    int m_savePathIndex = -1;
    int m_downloadPathIndex = -1;
    bool m_useDownloadPath = false;
    std::unique_ptr<TorrentFileGuard> m_torrentGuard;
    BitTorrent::AddTorrentParams m_torrentParams;

    SettingValue<QSize> m_storeDialogSize;
    SettingValue<QString> m_storeDefaultCategory;
    SettingValue<bool> m_storeRememberLastSavePath;
    SettingValue<QByteArray> m_storeTreeHeaderState;
    SettingValue<QByteArray> m_storeSplitterState;
};
