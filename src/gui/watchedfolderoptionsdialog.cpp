/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2021-2023  Vladimir Golovnev <glassez@yandex.ru>
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

#include "watchedfolderoptionsdialog.h"

#include "base/global.h"
#include "addtorrentparamswidget.h"
#include "ui_watchedfolderoptionsdialog.h"

#define SETTINGS_KEY(name) u"WatchedFolderOptionsDialog/" name

WatchedFolderOptionsDialog::WatchedFolderOptionsDialog(
        const TorrentFilesWatcher::WatchedFolderOptions &watchedFolderOptions, QWidget *parent)
    : QDialog {parent}
    , m_ui {new Ui::WatchedFolderOptionsDialog}
    , m_addTorrentParamsWidget {new AddTorrentParamsWidget(watchedFolderOptions.addTorrentParams)}
    , m_storeDialogSize {SETTINGS_KEY(u"DialogSize"_s)}
{
    m_ui->setupUi(this);
    m_ui->groupBoxParameters->layout()->addWidget(m_addTorrentParamsWidget);

    connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    loadState();
}

WatchedFolderOptionsDialog::~WatchedFolderOptionsDialog()
{
    saveState();
    delete m_ui;
}

TorrentFilesWatcher::WatchedFolderOptions WatchedFolderOptionsDialog::watchedFolderOptions() const
{
    TorrentFilesWatcher::WatchedFolderOptions watchedFolderOptions;
    watchedFolderOptions.recursive = m_ui->checkBoxRecursive->isChecked();
    watchedFolderOptions.addTorrentParams = m_addTorrentParamsWidget->addTorrentParams();

    return watchedFolderOptions;
}

void WatchedFolderOptionsDialog::loadState()
{
    if (const QSize dialogSize = m_storeDialogSize; dialogSize.isValid())
        resize(dialogSize);
}

void WatchedFolderOptionsDialog::saveState()
{
    m_storeDialogSize = size();
}
