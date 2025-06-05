/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2024  Vladimir Golovnev <glassez@yandex.ru>
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

#include <QWidget>

#include "base/bittorrent/sharelimitaction.h"

namespace Ui
{
    class TorrentShareLimitsWidget;
}

class TorrentShareLimitsWidget final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TorrentShareLimitsWidget)

public:
    explicit TorrentShareLimitsWidget(QWidget *parent = nullptr);
    ~TorrentShareLimitsWidget() override;

    void setRatioLimit(qreal ratioLimit);
    void setSeedingTimeLimit(int seedingTimeLimit);
    void setInactiveSeedingTimeLimit(int inactiveSeedingTimeLimit);
    void setShareLimitAction(BitTorrent::ShareLimitAction action);

    void setDefaultLimits(qreal ratioLimit, int seedingTimeLimit, int inactiveSeedingTimeLimit);

    std::optional<qreal> ratioLimit() const;
    std::optional<int> seedingTimeLimit() const;
    std::optional<int> inactiveSeedingTimeLimit() const;
    std::optional<BitTorrent::ShareLimitAction> shareLimitAction() const;

private:
    void refreshRatioLimitControls();
    void refreshSeedingTimeLimitControls();
    void refreshInactiveSeedingTimeLimitControls();

    Ui::TorrentShareLimitsWidget *m_ui = nullptr;

    int m_seedingTimeLimit = 1440;
    int m_inactiveSeedingTimeLimit = 1440;
    qreal m_ratioLimit = 1;

    int m_defaultSeedingTimeLimit = -1;
    int m_defaultInactiveSeedingTimeLimit = -1;
    qreal m_defaultRatioLimit = -1;
};
