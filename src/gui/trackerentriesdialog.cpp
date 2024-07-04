/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2019  Mike Tzou (Chocobo1)
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

#include "trackerentriesdialog.h"

#include <algorithm>

#include <QHash>
#include <QList>

#include "base/bittorrent/trackerentry.h"
#include "ui_trackerentriesdialog.h"
#include "utils.h"

#define SETTINGS_KEY(name) u"TrackerEntriesDialog/" name

TrackerEntriesDialog::TrackerEntriesDialog(QWidget *parent)
    : QDialog(parent)
    , m_ui(new Ui::TrackerEntriesDialog)
    , m_storeDialogSize(SETTINGS_KEY(u"Size"_s))
{
    m_ui->setupUi(this);

    connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    loadSettings();
}

TrackerEntriesDialog::~TrackerEntriesDialog()
{
    saveSettings();

    delete m_ui;
}

void TrackerEntriesDialog::setTrackers(const QList<BitTorrent::TrackerEntry> &trackers)
{
    int maxTier = -1;
    QHash<int, QString> tiers;  // <tier, tracker URLs>

    for (const BitTorrent::TrackerEntry &entry : trackers)
    {
        tiers[entry.tier] += (entry.url + u'\n');
        maxTier = std::max(maxTier, entry.tier);
    }

    QString text = tiers.value(0);

    for (int i = 1; i <= maxTier; ++i)
        text += (u'\n' + tiers.value(i));

    m_ui->plainTextEdit->setPlainText(text);
}

QList<BitTorrent::TrackerEntry> TrackerEntriesDialog::trackers() const
{
    return BitTorrent::parseTrackerEntries(m_ui->plainTextEdit->toPlainText());
}

void TrackerEntriesDialog::saveSettings()
{
    m_storeDialogSize = size();
}

void TrackerEntriesDialog::loadSettings()
{
    if (const QSize dialogSize = m_storeDialogSize; dialogSize.isValid())
        resize(dialogSize);
}
