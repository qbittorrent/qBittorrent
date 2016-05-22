/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016 Eugene Shalygin
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

#include "downloadingdeadlinedialog.h"

#include <QTime>

#include "base/settingsstorage.h"
#include "base/utils/misc.h"

#include "ui_downloadingdeadlinedialog.h"

namespace
{
#define SETTINGS_KEY(name) "DownloadingDeadlineDialog/" name
    constexpr const char KEY_SPEED[] = SETTINGS_KEY("speed");
    constexpr const char KEY_ONLY_UNCOMPLETED[] = SETTINGS_KEY("onlyUncompleted");
}

DownloadingDeadlineDialog::DownloadingDeadlineDialog(const QString &fileOrTorrentName,
                                                     qlonglong fileOrTorrentSize, qlonglong remainingSize,
                                                     QWidget *parent)
    : QDialog {parent}
    , m_ui {new Ui::DownloadingDeadlineDialog()}
    , m_size {fileOrTorrentSize}
    , m_remainingSize {remainingSize}
{
    m_ui->setupUi(this);

    m_ui->lblName->setText(fileOrTorrentName);
    m_ui->lblSize->setText(Utils::Misc::friendlyUnit(m_size, false));
    m_ui->lblRemainingSize->setText(Utils::Misc::friendlyUnit(m_remainingSize, false));
    m_ui->sbSpeed->setValue(SettingsStorage::instance()->loadValue(QLatin1String(KEY_SPEED), 2000).toInt());
    m_ui->chbxUncompletedOnly->setChecked(SettingsStorage::instance()->loadValue(QLatin1String(KEY_ONLY_UNCOMPLETED), true).toBool());

    speedChanged(m_ui->sbSpeed->value());

    setupSignals();
}

DownloadingDeadlineDialog::~DownloadingDeadlineDialog()
{
    delete m_ui;
}

std::chrono::milliseconds DownloadingDeadlineDialog::downloadingDeadline() const
{
#ifdef QBT_USES_QT5
    return std::chrono::milliseconds {m_ui->teDeadline->time().msecsSinceStartOfDay()};
#else
    return std::chrono::milliseconds {QTime(0, 0, 0, 0).msecsTo(m_ui->teDeadline->time())};
#endif
}

std::chrono::milliseconds DownloadingDeadlineDialog::downloadingDelay() const
{
#ifdef QBT_USES_QT5
    return std::chrono::milliseconds {m_ui->teDealy->time().msecsSinceStartOfDay()};
#else
    return std::chrono::milliseconds {QTime(0, 0, 0, 0).msecsTo(m_ui->teDealy->time())};
#endif
}

bool DownloadingDeadlineDialog::onlyUncompletedPieces() const
{
    return m_ui->chbxUncompletedOnly->isChecked();
}

void DownloadingDeadlineDialog::setupSignals()
{
    connect(m_ui->sbSpeed, SIGNAL(valueChanged(int)), this, SLOT(speedChanged(int)));
    connect(m_ui->teDeadline, SIGNAL(timeChanged(const QTime&)), this, SLOT(deadlineChanged(const QTime&)));
    connect(m_ui->chbxUncompletedOnly, SIGNAL(clicked(bool)), this, SLOT(uncompletedOnlyClicked()));
}

qlonglong DownloadingDeadlineDialog::sizeToDownload() const
{
    return onlyUncompletedPieces() ? m_remainingSize : m_size;
}

void DownloadingDeadlineDialog::speedChanged(int speed)
{
    // speed is in kb/s
    int downloadingTime = static_cast<int>(sizeToDownload() / speed * 8 / 1024);     // seconds
    constexpr int secondsInDay = 24 * 60 * 60;     // 86400
    if (downloadingTime > secondsInDay)
        downloadingTime = secondsInDay - 1;

#ifdef QBT_USES_QT5
    m_ui->teDeadline->setTime(QTime::fromMSecsSinceStartOfDay(downloadingTime * 1000));
#else
    m_ui->teDeadline->setTime(QTime(0, 0, 0 ,0).addMSecs(downloadingTime * 1000));
#endif
}

void DownloadingDeadlineDialog::deadlineChanged(const QTime &deadline)
{
#ifdef QBT_USES_QT5
    const int downloadingTime = deadline.msecsSinceStartOfDay() / 1000;
#else
    const int downloadingTime = QTime(0, 0, 0, 0).msecsTo(deadline) / 1000;
#endif
    const int speed = sizeToDownload() / downloadingTime * 8 / 1024;
    m_ui->sbSpeed->setValue(speed);
}

void DownloadingDeadlineDialog::uncompletedOnlyClicked()
{
    // preserve speed, recompute deadline
    speedChanged(m_ui->sbSpeed->value());
}

void DownloadingDeadlineDialog::accept()
{
    SettingsStorage::instance()->storeValue(QLatin1String(KEY_SPEED), m_ui->sbSpeed->value());
    SettingsStorage::instance()->storeValue(QLatin1String(KEY_ONLY_UNCOMPLETED), m_ui->chbxUncompletedOnly->isChecked());

    QDialog::accept();
}
