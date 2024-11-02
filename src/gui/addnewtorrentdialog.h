/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2022-2024  Vladimir Golovnev <glassez@yandex.ru>
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

#include "base/path.h"
#include "base/settingvalue.h"
#include "filterpatternformat.h"

class LineEdit;

namespace BitTorrent
{
    class TorrentDescriptor;
    class TorrentInfo;
    struct AddTorrentParams;
}

namespace Ui
{
    class AddNewTorrentDialog;
}

class AddNewTorrentDialog final : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AddNewTorrentDialog)

public:
    explicit AddNewTorrentDialog(const BitTorrent::TorrentDescriptor &torrentDescr
            , const BitTorrent::AddTorrentParams &inParams, QWidget *parent);
    ~AddNewTorrentDialog() override;

    bool isDoNotDeleteTorrentChecked() const;
    void updateMetadata(const BitTorrent::TorrentInfo &metadata);

signals:
    void torrentAccepted(const BitTorrent::TorrentDescriptor &torrentDescriptor, const BitTorrent::AddTorrentParams &addTorrentParams);
    void torrentRejected(const BitTorrent::TorrentDescriptor &torrentDescriptor);

private slots:
    void updateDiskSpaceLabel();
    void onSavePathChanged(const Path &newPath);
    void onDownloadPathChanged(const Path &newPath);
    void onUseDownloadPathChanged(bool checked);
    void TMMChanged(int index);
    void categoryChanged(int index);
    void contentLayoutChanged();

    void accept() override;
    void reject() override;

private:
    class TorrentContentAdaptor;
    struct Context;

    void showEvent(QShowEvent *event) override;

    void setCurrentContext(std::shared_ptr<Context> context);
    void updateCurrentContext();
    void populateSavePaths();
    void loadState();
    void saveState();
    void setMetadataProgressIndicator(bool visibleIndicator, const QString &labelText = {});
    void setupTreeview();
    void saveTorrentFile();
    void showContentFilterContextMenu();
    void setContentFilterPattern();

    Ui::AddNewTorrentDialog *m_ui = nullptr;
    std::unique_ptr<TorrentContentAdaptor> m_contentAdaptor;
    int m_savePathIndex = -1;
    int m_downloadPathIndex = -1;
    bool m_useDownloadPath = false;
    LineEdit *m_filterLine = nullptr;

    std::shared_ptr<Context> m_currentContext;

    SettingValue<QSize> m_storeDialogSize;
    SettingValue<QString> m_storeDefaultCategory;
    SettingValue<bool> m_storeRememberLastSavePath;
    SettingValue<QByteArray> m_storeTreeHeaderState;
    SettingValue<QByteArray> m_storeSplitterState;
    SettingValue<FilterPatternFormat> m_storeFilterPatternFormat;
};
