/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2024  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2020  thalieht
 * Copyright (C) 2011  Christian Kandeler
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

#include <optional>

#include <QDialog>

#include "base/bittorrent/sharelimitaction.h"
#include "base/path.h"
#include "base/settingvalue.h"

class QAbstractButton;

namespace BitTorrent
{
    class Torrent;
    class TorrentID;
}

namespace Ui
{
    class TorrentOptionsDialog;
}

class TorrentOptionsDialog final : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TorrentOptionsDialog)

public:
    explicit TorrentOptionsDialog(QWidget *parent, const QList<BitTorrent::Torrent *> &torrents);
    ~TorrentOptionsDialog() override;

public slots:
    void accept() override;

private slots:
    void handleCategoryChanged(int index);
    void handleTMMChanged();
    void handleUseDownloadPathChanged();

    void handleUpSpeedLimitChanged();
    void handleDownSpeedLimitChanged();

private:
    QList<BitTorrent::TorrentID> m_torrentIDs;
    Ui::TorrentOptionsDialog *m_ui = nullptr;
    SettingValue<QSize> m_storeDialogSize;
    QStringList m_categories;
    QString m_currentCategoriesString;
    bool m_allSameCategory = true;
    QAbstractButton *m_previousRadio = nullptr;
    struct
    {
        Path savePath;
        Path downloadPath;
        QString category;
        std::optional<qreal> ratio;
        std::optional<int> seedingTime;
        std::optional<int> inactiveSeedingTime;
        std::optional<BitTorrent::ShareLimitAction> shareLimitAction;
        int upSpeedLimit;
        int downSpeedLimit;
        Qt::CheckState autoTMM;
        Qt::CheckState useDownloadPath;
        Qt::CheckState disableDHT;
        Qt::CheckState disablePEX;
        Qt::CheckState disableLSD;
        Qt::CheckState sequential;
        Qt::CheckState firstLastPieces;
    } m_initialValues;
};
